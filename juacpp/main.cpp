#include <iostream>
#include <fstream>
#include <filesystem>
#include "jua-vm.h"

namespace fs = std::filesystem;
using std::cout;

#include <windows.h>
void SetConsoleToUTF8() {
    SetConsoleOutputCP(CP_UTF8);  // 设置输出编码为UTF-8
    SetConsoleCP(CP_UTF8);        // 设置输入编码为UTF-8
}

struct JuaRuntime: JuaVM{
    const char* main; //主模块名称
    fs::path cwd;
    JuaRuntime(const char* name, fs::path _cwd=fs::current_path()): main(name), cwd(_cwd){}
    void run(){
        JuaVM::run(findModule(main));
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

int main(int argc, char* argv[]){
    SetConsoleToUTF8();
    if(argc > 1){
        const char* main_module = argv[1];
        JuaRuntime runtime(main_module);
        runtime.run();
    }else{
        throw "todo: 进入 REPL 模式";
    }
    return 0;
}