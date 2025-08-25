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