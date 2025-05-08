#include <iostream>
#include <fstream>
#include <filesystem>
#include "jua-internal.h"

namespace fs = std::filesystem;
using std::cout;

#include <windows.h>
void SetConsoleToUTF8() {
    SetConsoleOutputCP(CP_UTF8);  // 设置输出编码为UTF-8
    SetConsoleCP(CP_UTF8);        // 设置输入编码为UTF-8
}

struct JuaRuntime: JuaVM{
    using JuaVM::JuaVM;
    JuaRuntime(const char* name): JuaVM(findModule(name)){} //提供主模块名称
    std::string findModule(const std::string& name)override{
        fs::path fp = name+".jua";
        std::fstream file(fp, std::ios_base::in);
        size_t len = fs::file_size(fp);
        string script(len, '\0');
        file.read(&script[0], len);
        //cout<<script<<'\n';
        return script;
    }
    void j_stdout(jualist& vals){
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

int main(){
    try{
        SetConsoleToUTF8();
        cout << "构造 JuaRuntime\n";
        JuaRuntime RT("test");
        cout << "构造 JuaRuntime 完毕\n";
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