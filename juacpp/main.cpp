#include <iostream>
#include <fstream>
#include <filesystem>
#include "jua-vm.h"
#include "jua-syntax.h"

#ifdef _WIN32
#include <windows.h>
#endif

using namespace Jua;
namespace fs = std::filesystem;
using std::cout;

static void SetConsoleToUTF8(){
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);  // 输出编码
    SetConsoleCP(CP_UTF8);        // 输入编码
#endif
}

// print 的转换由宿主决定（见 抽象操作.md），不必等于 ToString。
// 官方实现：没有 toString 元方法的对象显示其键，便于调试。
static std::string renderForPrint(Value* v){
    if(v->type == Value::Obj && !v->getMetaMethod("toString"))
        return v->safeToString();
    return v->toString();
}

struct JuaRuntime: VM{
    const char* main; //主模块名称
    fs::path cwd;
    bool failed = false; //本次运行是否已经报过错（决定退出码）
    JuaRuntime(const char* name, fs::path _cwd=fs::current_path()): main(name), cwd(_cwd){
        auto api = new Object(this);
        api->setProp("version", new String(this, "JuaRuntime 0.1"));
        //GC 诊断接口（宿主 API，不属于语言）
        api->setProp("collect", makeFunc([this](jualist&){
            return new Number(this, (double)collect());
        }));
        api->setProp("liveValues", makeFunc([this](jualist&){
            return new Number(this, (double)liveCount());
        }));
        api->setProp("gcStats", makeFunc([this](jualist&){
            auto stats = new Object(this);
            stats->setProp("collections", new Number(this, (double)collectedCount));
            stats->setProp("freed", new Number(this, (double)freedCount));
            stats->setProp("live", new Number(this, (double)liveCount()));
            return stats;
        }));
        api->setProp("alert", makeFunc([this](jualist& args){
            if(args.size() < 1) throw new JuaError("api.alert() requires at least 1 argument");
            auto msg = args[0];
            cout << "Runtime.alert: " << msg->toString() << '\n';
            return Null::getInst();
        }));
        _G->setProp("Runtime", api);
        auto fs_api = new Object(this);
        fs_api->setProp("cwd", new String(this, cwd.string()));
        fs_api->setProp("openAsBuffer", makeFunc([this](jualist& args){
            if(args.size() < 1) throw new JuaError("fs.openAsBuffer() requires at least 1 argument");
            auto pathVal = args[0];
            if(pathVal->type != Value::Str)
                throw new JuaError("fs.openAsBuffer() requires a string argument");
            fs::path fp = cwd/pathVal->toString();
            std::fstream file(fp, std::ios_base::in | std::ios_base::binary);
            if(!file.is_open()){
                std::string err = "Cannot open file: ";
                throw new JuaError(err+fp.string());
            }
            size_t len = fs::file_size(fp);
            auto buf = new Buffer(this, len);
            file.read((char*)buf->bytes, len);
            return buf;
        }));
        fs_api->setProp("write", makeFunc([this](jualist& args){
            if(args.size() < 2) throw new JuaError("fs.write() requires at least 2 arguments");
            auto pathVal = args[0];
            if(pathVal->type != Value::Str)
                throw new JuaError("fs.write() requires a string as the first argument");
            auto data = args[1];
            fs::path fp = cwd/pathVal->toString();
            std::fstream file(fp, std::ios_base::out | std::ios_base::binary);
            if(!file.is_open()){
                std::string err = "Cannot open file for writing: ";
                throw new JuaError(err+fp.string());
            }
            if(data->type == Value::Str){
                auto str = static_cast<String*>(data);
                file.write(str->value.data(), str->value.size());
            } else if(data->isType(Buffer::type_id)){
                auto buf = static_cast<Buffer*>(data);
                file.write((char*)buf->bytes, buf->length);
            } else {
                throw new JuaError("fs.write() requires a string or buffer as the second argument");
            }
            return Null::getInst();
        }));
        modules["fs"] = fs_api;
    }
    void run(){
        string script;
        try{
            script = findModule(main);
        }catch(JuaError* e){
            j_stderr(e);
            return;
        }
        VM::run(script);
    }
    std::string findModule(const std::string& name){
        fs::path fp = cwd/(name+".jua");
        std::fstream file(fp, std::ios_base::in);
        if(!file.is_open()){
            std::string err = "未找到模块：";
            throw new JuaError(err + name);
        }
        size_t len = fs::file_size(fp);
        string script(len, '\0');
        file.read(&script[0], len);
        //cout<<script<<'\n';
        return script;
    }
    void j_stdout(const jualist& vals){
        size_t len = vals.size();
        if(!len)cout << '\n';
        std::string str(renderForPrint(vals[0]));
        for(size_t i=1; i<len; i++){
            str.push_back('\t');
            str.append(renderForPrint(vals[i]));
        }
        cout << str << '\n';
    };
    void j_stderr(JuaError* err){
        //错误走 stderr，并标记本次运行失败；CLI 模式据此给出退出码
        cout.flush();
        std::cerr << err->toDebugString() << '\n';
        std::cerr.flush();
        failed = true;
    }
};

// REPL 输入的三种状态
enum class InputStatus{
    Complete,   // 能解析出完整的函数体，可以执行
    Incomplete, // 输入在末尾处用完了，继续读下一行
    Error       // 真的写错了
};

// 尝试把 source 解析成一个函数体。成功时 *body 有效；
// 失败时 *err 有效，由调用者释放。
static InputStatus tryParse(const std::string& source, FunctionBody** body, JuaSyntaxError** err){
    try{
        *body = parse(source);
        *err = nullptr;
        return InputStatus::Complete;
    }catch(JuaSyntaxError* e){
        *body = nullptr;
        *err = e;
        return e->incomplete ? InputStatus::Incomplete : InputStatus::Error;
    }
}

// REPL：读到能构成完整语句时立即执行，不再依赖分号。
int runRepl(){
    cout << "Jua REPL. Input runs as soon as it forms a complete statement.\n";
    cout << "Type 'exit' to quit, Ctrl+D (Ctrl+Z on Windows) to leave.\n";
    JuaRuntime rt("<repl>");
    //整个 REPL 会话共用一个作用域，上一行声明的变量下一行才能用。
    //它是宿主长期持有的值，必须登记为 GC 根，否则会被回收。
    auto replEnv = new Scope(rt._G);
    rt.own(replEnv);

    std::string buffer;
    while(true){
        cout << (buffer.empty() ? "> " : "... ") << std::flush;
        std::string line;
        if(!std::getline(std::cin, line)) break; // EOF：丢弃未完成的输入并退出
        if(buffer.empty()){
            if(line == "exit" || line == "exit;") break;
            // 空行直接忽略，免得把空输入当成一条语句
            if(line.find_first_not_of(" \t\r") == std::string::npos) continue;
        }
        buffer += line;
        buffer += '\n';

        FunctionBody* body = nullptr;
        JuaSyntaxError* syn = nullptr;
        auto status = tryParse(buffer, &body, &syn);
        if(status == InputStatus::Incomplete){
            delete syn;
            continue; // 继续读：提示符会变成 "... "
        }
        if(status == InputStatus::Error){
            rt.j_stderr(syn);
            delete syn;
            buffer.clear();
            continue;
        }
        try{
            body->exec(replEnv);
        }catch(JuaError* e){
            rt.j_stderr(e);
            delete e;
        }catch(const std::string& s){
            std::cerr << s << '\n';
        }catch(const char* s){
            std::cerr << s << '\n';
        }
        buffer.clear();
    }
    return 0;
}

int main(int argc, char* argv[]){
    SetConsoleToUTF8();
    if(argc <= 1) return runRepl();

    //带参数：运行一个模块。退出码 0 = 成功，1 = 失败，2 = 用法错误
    try{
        JuaRuntime rt(argv[1]);
        rt.run();
        return rt.failed ? 1 : 0;
    }catch(JuaError* e){
        std::cerr << e->toDebugString() << '\n';
        delete e;
        return 1;
    }catch(const std::string& s){
        std::cerr << s << '\n';
        return 1;
    }catch(const char* s){
        std::cerr << s << '\n';
        return 1;
    }catch(const std::exception& e){
        std::cerr << "std::exception: " << e.what() << '\n';
        return 1;
    }catch(...){
        std::cerr << "unknown error\n";
        return 1;
    }
}