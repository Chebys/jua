// 已弃用（2026-11-06）：宿主统一由 main.cpp 提供，本文件不再参与构建。
// 保留仅为历史参考。请不要在这里修 bug —— 改 main.cpp。
//
// Jua CLI 宿主：运行一个模块，用退出码报告结果。
//   退出码 0 = 成功；1 = jua 错误或未捕获异常；2 = 用法错误
// 程序自身的 print 走 stdout，错误与诊断走 stderr，便于测试脚本区分。
#include <iostream>
#include <fstream>
#include <filesystem>
#include "jua-vm.h"

#ifdef _WIN32
#include <windows.h>
#endif

using namespace Jua;
namespace fs = std::filesystem;

static void setConsoleToUTF8(){
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

struct JuaCli: VM{
    std::string mainName;
    fs::path cwd;
    bool failed = false; //是否已经向 stderr 报告过错误

    JuaCli(const std::string& name, fs::path dir = fs::current_path())
        : mainName(name), cwd(dir){
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
            std::cout << "Runtime.alert: " << args[0]->toString() << '\n';
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
            if(!file.is_open())
                throw new JuaError("Cannot open file: " + fp.string());
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
            if(!file.is_open())
                throw new JuaError("Cannot open file for writing: " + fp.string());
            if(data->type == Value::Str){
                auto str = static_cast<String*>(data);
                file.write(str->value.data(), str->value.size());
            }else if(data->isType(Buffer::type_id)){
                auto buf = static_cast<Buffer*>(data);
                file.write((char*)buf->bytes, buf->length);
            }else{
                throw new JuaError("fs.write() requires a string or buffer as the second argument");
            }
            return Null::getInst();
        }));
        modules["fs"] = fs_api;
    }

    void run(){
        VM::run(findModule(mainName));
    }

    std::string findModule(const std::string& name){
        fs::path fp = cwd/(name+".jua");
        std::fstream file(fp, std::ios_base::in);
        if(!file.is_open())
            throw std::string("Module not found: ") + name;
        size_t len = fs::file_size(fp);
        std::string script(len, '\0');
        file.read(&script[0], len);
        return script;
    }

    void j_stdout(const jualist& vals){
        size_t len = vals.size();
        if(!len){
            std::cout << '\n';
            return;
        }
        std::string str(vals[0]->toString());
        for(size_t i=1; i<len; i++){
            str.push_back('\t');
            str.append(vals[i]->toString());
        }
        std::cout << str << '\n';
    }

    void j_stderr(JuaError* err){
        std::cout.flush();
        std::cerr << err->toDebugString() << '\n';
        std::cerr.flush();
        failed = true;
    }
};

int main(int argc, char* argv[]){
    setConsoleToUTF8();
    if(argc < 2){
        std::cerr << "usage: jua <module>\n";
        return 2;
    }
    try{
        JuaCli rt(argv[1]);
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
