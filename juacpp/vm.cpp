#include "jua-vm.h"
#include "jua-syntax.h"

JuaVM::JuaVM(){
    initBuiltins();
    makeGlobal();
}

void JuaVM::run(const string& script){
    try{
        auto val = eval(script);
        val->gc();
    }catch(JuaError* e){
        j_stderr(e);
    }
}

Jua_Val* JuaVM::eval(const string& script){
    //返回非空指针，可能需要垃圾回收
    //d_log("eval");
    //d_log(script);
    auto body = parse(script);
    return body->exec(new Scope(_G));
}
void JuaVM::initBuiltins(){
    StringProto = new Jua_Obj(this);
    NumberProto = new Jua_Obj(this);
    BooleanProto = new Jua_Obj(this);
    FunctionProto = new Jua_Obj(this);
    ArrayProto = new Jua_Obj(this);
    BufferProto = new Jua_Obj(this);
    RangeProto = new Jua_Obj(this);
    RangeProto->setProp("next", makeFunc([](jualist& args){
        //todo: 支持浮点数
        auto self = args[0];
        if(!self || self->type != Jua_Val::Obj){
            throw string("Range.next() called on a non-object");
        }
        auto key = args[1];
        if(!key || key->type != Jua_Val::Num){
            auto start = self->getProp("start");
            if(!start || start->type != Jua_Val::Num)
                throw string("Range.next() called on a non-range object");
            key = new Jua_Num(self->vm, start->toInt());
        }
        auto step = self->getProp("step");
        if(!step || step->type != Jua_Val::Num)
            throw string("Range.next() called on a non-range object");
        auto end = self->getProp("end");
        if(!end || end->type != Jua_Val::Num)
            throw string("Range.next() called on a non-range object");
        auto res = new Jua_Obj(self->vm);
        auto done = key->toInt() >= end->toInt();
        res->setProp("done", Jua_Bool::getInst(done));
        if(!done){
            res->setProp("value", key);
            res->setProp("key", new Jua_Num(self->vm, key->toInt() + step->toInt()));
        }
        return res;
    }));
}
void JuaVM::makeGlobal(){
    _G = new Scope(this);
    _G->setProp("String", StringProto);
    _G->setProp("Number", NumberProto);
    _G->setProp("Boolean", BooleanProto);
    _G->setProp("Function", FunctionProto);
    _G->setProp("Array", ArrayProto);
    _G->setProp("Buffer", BufferProto);
    _G->setProp("Range", RangeProto);
    _G->addRef();
    _G->setProp("_G", _G);
    auto print = [this](jualist& args){
        j_stdout(args);
        return Jua_Null::getInst();
    };
    _G->setProp("print", makeFunc(print));
    //auto val = _G->getProp("print");
}
Jua_Val* JuaVM::require(const string& name){
    if(modules.contains(name))return modules[name];
    //todo: 检查循环导入
    const string& script = findModule(name);
    modules[name] = eval(script);
    return modules[name];
}