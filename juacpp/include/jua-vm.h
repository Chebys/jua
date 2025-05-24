#include "jua-value.h"

struct JuaVM{
    std::unordered_map<string, Jua_Val*> modules;
    Scope* _G;
    JuaVM();
    void run(const string&);
    Jua_Val* eval(const string&); //不捕获错误

    protected:
    virtual void initBuiltins();
    virtual void makeGlobal();
    virtual string findModule(const string& name) = 0;
    virtual void j_stdout(const jualist&){};
    virtual void j_stderr(JuaError*){};

    private:
    Jua_Val* require(const string& name);
};