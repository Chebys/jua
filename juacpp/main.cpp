#include <iostream>
#include <fstream>
#include <filesystem>
#include "jua-vm.h"

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

struct JuaRuntime: VM{
    const char* main; //主模块名称
    fs::path cwd;
    bool failed = false; //本次运行是否已经报过错（决定退出码）
    JuaRuntime(const char* name, fs::path _cwd=fs::current_path()): main(name), cwd(_cwd){
        auto api = new Object(this);
        api->setProp("version", new String(this, "JuaRuntime 0.1"));
        //GC 诊断接口（宿主 API，不属于语言）
        api->setProp("collect", makeFunc([this](jualist& args){
            return new Number(this, (double)collect());
        }));
        api->setProp("liveValues", makeFunc([this](jualist& args){
            return new Number(this, (double)liveCount());
        }));
        api->setProp("gcStats", makeFunc([this](jualist& args){
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
        std::string str(vals[0]->toString());
        for(size_t i=1; i<len; i++){
            str.push_back('\t');
            str.append(vals[i]->toString());
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

// 读一条以分号结尾的多行输入。返回 false 表示输入已结束（EOF）。
bool readInput(std::string& result){
    result.clear();
    std::string line;
    while (true) {
        std::cout << "> " << std::flush;
        if (!std::getline(std::cin, line)) return false; // EOF：必须退出，否则会空转
        result += line;
        if (!line.empty() && line.back() == ';') return true;
        result += '\n';
    }
}

int main(int argc, char* argv[]){
    SetConsoleToUTF8();
    if(argc <= 1){
        //无参数：REPL
        cout << "Jua REPL. Add ';' to end input. Type 'exit;' to quit.\n";
        JuaRuntime rt("<repl>");
        while (true) {
            std::string input;
            if (!readInput(input)) break;
            if (input == "exit;") break;
            try{
                rt.eval(input);
            }catch(JuaError* e){
                rt.j_stderr(e);
            }catch(const std::string& s){
                std::cerr << s << '\n';
            }catch(const char* s){
                std::cerr << s << '\n';
            }
        }
        return 0;
    }

    //带参数：运行一个模块。退出码 0 = 成功，1 = 失败，2 = 用法错误
    try{
        JuaRuntime rt(argv[1]);
        rt.run();
        return rt.failed ? 1 : 0;
    }catch(JuaError* e){
        std::cerr << e->toDebugString() << '\n';
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