#include <iostream>
#include <fstream>
#include <filesystem>
#include "jua-vm.h"

const char* default_module = "0";

namespace fs = std::filesystem;
using std::cout;

#include <windows.h>
void SetConsoleToUTF8() {
    SetConsoleOutputCP(CP_UTF8);  // 设置输出编码为UTF-8
    SetConsoleCP(CP_UTF8);        // 设置输入编码为UTF-8
}

struct JuaRuntime: VM{
    const char* main; //主模块名称
    fs::path cwd;
    JuaRuntime(const char* name, fs::path _cwd=fs::current_path()): main(name), cwd(_cwd){
        auto api = new Object(this);
        api->setProp("version", new String(this, "JuaRuntime 0.1"));
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
        VM::run(findModule(main));
    }
    std::string findModule(const std::string& name){
        fs::path fp = cwd/(name+".jua");
        std::fstream file(fp, std::ios_base::in);
        if(!file.is_open()){
            std::string err = "未找到模块：";
            throw err+name;
        }
        size_t len = fs::file_size(fp);
        string script(len, '\0');
        file.read(&script[0], len);
        //cout<<script<<'\n';
        return script;
    }
    void j_stdout(const jualist& vals){
        cout << "j_stdout: ";
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
        cout << "j_stderr: " << err->toDebugString() << '\n';
    }
};

int main(int argc, char* argv[]){
    //第一个参数指定模块名（不含jua后缀），默认 "main"
    try{
        const char* main_module;
        if(argc>1)
            main_module = argv[1];
        else
            main_module = default_module;
        SetConsoleToUTF8();
        cout << "构造 JuaRuntime\n";
        JuaRuntime RT(main_module);
        cout << "VM::run\n";
        RT.run();
        cout << "VM::run 完毕\n";
    }catch(JuaError* err){
        cout << "在 VM::run 外部捕获 JuaError: " << err->toDebugString() << '\n';
    }catch(const char* str){
        cout<<str<<'\n';
    }catch(string str){
        cout<<str<<'\n';
    }catch(const std::exception& e){
        std::cerr << "std::exception " << e.what() << '\n';
    }catch(...){
        cout<<"error of unknown type\n";
    }
    cout << "按 Enter 键退出\n";
    getchar();
    return 0;
}