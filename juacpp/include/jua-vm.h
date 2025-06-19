#include "jua-value.h"

struct JuaVM{
    std::unordered_map<string, Jua_Val*> modules;

    Scope* _G;
    Jua_Obj* StringProto;
    Jua_Obj* NumberProto;
    Jua_Obj* BooleanProto;
    Jua_Obj* FunctionProto;
    Jua_Obj* ArrayProto;
    Jua_Obj* BufferProto;
    Jua_Obj* RangeProto;

    JuaVM();
    ~JuaVM(){
        _G->release();
        for(auto& [name, val]: modules){
            val->release();
        }
        delete StringProto;
        delete NumberProto;
        delete BooleanProto;
        delete FunctionProto;
        delete ArrayProto;
        delete BufferProto;
        delete RangeProto;
    }
    void run(const string&);
    Jua_Val* eval(const string&); //不捕获错误

    protected:
    virtual void initBuiltins();
    virtual void makeGlobal();
    virtual string findModule(const string& name) = 0;
    virtual void j_stdout(const jualist&){};
    virtual void j_stderr(JuaError*){};
    Jua_NativeFunc* makeFunc(Jua_NativeFunc::Native fn){
        return new Jua_NativeFunc(this, fn);
    }

    private:
    Jua_Val* require(const string& name);
};