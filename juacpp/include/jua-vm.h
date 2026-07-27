#include "unordered_set"
#include "jua-value.h"
namespace Jua {

    struct VM{
    std::unordered_map<std::string, Value*> modules;
    std::unordered_set<Value*> ownedValues;

    Scope* _G;
    Object* classProto;
    Object* StringProto;
    Object* NumberProto;
    Object* BooleanProto;
    Object* FunctionProto;
    Object* ObjectProto;
    Object* ArrayProto;
    Object* BufferProto;
    Object* RangeProto;
    Object* ErrorProto;
    Object* TryResProto;

    NativeFunction* obj_new;
    NativeFunction* obj_hasOwn;
    NativeFunction* obj_next;

    VM();
    ~VM(){
        //todo: 释放所有值
    }
    void run(const std::string&);
    Value* eval(const std::string&); //不捕获错误
    void own(Value*); //管理生命周期

    protected:
    void initBuiltins();
    void makeGlobal(); //在构造函数中调用，重写没有意义；要添加内置值请在子类构造函数中进行
    virtual std::string findModule(const std::string& name) = 0;
    virtual void j_stdout(const jualist&){};
    virtual void j_stderr(JuaError*){};
    NativeFunction* makeFunc(NativeFunction::Native fn){
        return new NativeFunction(this, fn);
    }
    Object* buildClass(NativeFunction::Native constructor);
    Object* makeRangeProto();
    Object* makeNumberProto();
    Object* makeStringProto();
    Object* makeFunctionProto();
    Object* makeObjectProto();

    Object* makeArrayProto();
    Object* makeBufferProto();
    Object* makeErrorProto();
    Object* makeTryResProto();

    Object* makeMath();
    Object* makeJSON();

    private:
    size_t idcounter = 0;
    Value* require(const std::string& name);
    typedef std::string Encoder(double);
    typedef double Decoder(const std::string&);
    NativeFunction* makeEncodeFunc(Encoder);
    NativeFunction* makeDecodeFunc(Decoder);
};

}