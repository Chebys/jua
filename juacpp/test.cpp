#include <iostream>
#include <fstream>
#include <filesystem>
#include "jua-vm.h"

#ifdef JUA_DEBUG
const char* default_module = "expressions";
#else
const char* default_module = "main";
#endif

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
        cout << err->toDebugString() << '\n';
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
        cout << "JuaVM::run\n";
        RT.run();
        cout << "JuaVM::run 完毕\n";
    }catch(JuaError* err){
        cout << "在 JuaVM::run 外部捕获 JuaError: " << err->toDebugString() << '\n';
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