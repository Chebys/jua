#include <iostream>
#include <fstream>
#include <filesystem>
#include "jua-vm.h"
#include <windows.h>

namespace fs = std::filesystem;
using std::cout;

void SetConsoleToUTF8() {
    SetConsoleOutputCP(CP_UTF8);  // 设置输出编码为UTF-8
    SetConsoleCP(CP_UTF8);        // 设置输入编码为UTF-8
}

struct JuaRuntime: JuaVM{
    const char* main; //主模块名称
    fs::path cwd;
    JuaRuntime(const char* name, fs::path _cwd=fs::current_path()): main(name), cwd(_cwd){
        auto api = new Jua_Obj(this);
        api->setProp("version", new Jua_Str(this, "JuaRuntime 0.1"));
        api->setProp("alert", makeFunc([this](jualist& args){
            if(args.size() < 1) throw new JuaError("api.alert() requires at least 1 argument");
            auto msg = args[0];
            cout << "Runtime.alert: " << msg->toString() << '\n';
            return Jua_Null::getInst();
        }));
        _G->setProp("Runtime", api);
        auto fs_api = new Jua_Obj(this);
        fs_api->setProp("cwd", new Jua_Str(this, cwd.string()));
        fs_api->setProp("openAsBuffer", makeFunc([this](jualist& args){
            if(args.size() < 1) throw new JuaError("fs.openAsBuffer() requires at least 1 argument");
            auto pathVal = args[0];
            if(pathVal->type != Jua_Val::Str)
                throw new JuaError("fs.openAsBuffer() requires a string argument");
            fs::path fp = cwd/pathVal->toString();
            std::fstream file(fp, std::ios_base::in | std::ios_base::binary);
            if(!file.is_open()){
                std::string err = "Cannot open file: ";
                throw new JuaError(err+fp.string());
            }
            size_t len = fs::file_size(fp);
            auto buf = new Jua_Buffer(this, len);
            file.read((char*)buf->bytes, len);
            return buf;
        }));
        fs_api->setProp("write", makeFunc([this](jualist& args){
            if(args.size() < 2) throw new JuaError("fs.write() requires at least 2 arguments");
            auto pathVal = args[0];
            if(pathVal->type != Jua_Val::Str)
                throw new JuaError("fs.write() requires a string as the first argument");
            auto data = args[1];
            fs::path fp = cwd/pathVal->toString();
            std::fstream file(fp, std::ios_base::out | std::ios_base::binary);
            if(!file.is_open()){
                std::string err = "Cannot open file for writing: ";
                throw new JuaError(err+fp.string());
            }
            if(data->type == Jua_Val::Str){
                auto str = static_cast<Jua_Str*>(data);
                file.write(str->value.data(), str->value.size());
            } else if(data->isType(Jua_Buffer::type_id)){
                auto buf = static_cast<Jua_Buffer*>(data);
                file.write((char*)buf->bytes, buf->length);
            } else {
                throw new JuaError("fs.write() requires a string or buffer as the second argument");
            }
            return Jua_Null::getInst();
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
        JuaVM::run(script);
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
        cout << err->toDebugString() << '\n';
    }
};

std::string getMultilineInput() {
    // 以分号结尾表示输入结束
    std::string result;
    std::string line;
    while (true) {
        std::cout << "> ";
        std::getline(std::cin, line);
        result += line;
        if (!line.empty() && line.back() == ';') break;
        result += '\n';
    }
    
    return result;
}

int main(int argc, char* argv[]){
    SetConsoleToUTF8();
    if(argc > 1){
        const char* main_module = argv[1];
        JuaRuntime runtime(main_module);
        runtime.run();
    }else{
        cout << "Jua REPL. Add ';' to end input. Type 'exit;' to quit.\n";
        JuaRuntime rt("<repl>");
        while (true) {
            std::string input = getMultilineInput();
            if (input.empty()) continue;
            if (input == "exit;") break;
            try{
                auto val = rt.eval(input);
                val->gc();
            }catch(JuaError* e){
                rt.j_stderr(e);
            }
        }
    }
    return 0;
}