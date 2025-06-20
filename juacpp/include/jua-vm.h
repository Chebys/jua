#include "jua-value.h"

struct JuaVM{
    std::unordered_map<string, Jua_Val*> modules;

    Scope* _G;
    Jua_Obj* classProto;
    Jua_Obj* StringProto;
    Jua_Obj* NumberProto;
    Jua_Obj* BooleanProto;
    Jua_Obj* FunctionProto;
    Jua_Obj* ObjectProto;
    Jua_Obj* ArrayProto;
    Jua_Obj* BufferProto;
    Jua_Obj* RangeProto;

    Jua_NativeFunc* obj_new;
    Jua_NativeFunc* obj_hasOwn;
    Jua_NativeFunc* obj_next;

    JuaVM();
    ~JuaVM(){
        _G->release();
        for(auto& [name, val]: modules){
            val->release();
        }
        //todo: 释放所有值
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
    Jua_Obj* buildClass(Jua_NativeFunc::Native constructor);
    Jua_Obj* makeRangeProto();
    Jua_Obj* makeNumberProto();
    Jua_Obj* makeStringProto();
    Jua_Obj* makeObjectProto();

    private:
    size_t idcounter = 0;
    Jua_Val* require(const string& name);
    typedef void Encoder(double, string&);
    typedef double Decoder(const string&);
    Jua_NativeFunc* makeEncodeFunc(Encoder);
    Jua_NativeFunc* makeDecodeFunc(Decoder);
};