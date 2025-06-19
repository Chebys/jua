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
    ObjectProto = new Jua_Obj(this);
    ArrayProto = new Jua_Obj(this);
    BufferProto = new Jua_Obj(this);
    RangeProto = new Jua_Obj(this);
    RangeProto->setProp("next", makeFunc([](jualist& args){
        //todo: 支持浮点数
        if(args.size() < 2) throw "Range.next() requires 2 arguments";
        auto self = args[0];
        if(self->type != Jua_Val::Obj){
            throw string("Range.next() called on a non-object");
        }
        auto key = args[1];
        size_t index;
        if(key->type == Jua_Val::Num){
            auto step = self->getProp("step");
            if(!step || step->type != Jua_Val::Num)
                throw string("Range.next() called on a non-range object");
            index = key->toInt() + step->toInt();
        }else{
            auto start = self->getProp("start");
            if(!start || start->type != Jua_Val::Num)
                throw string("Range.next() called on a non-range object");
            index = start->toInt();
        }
        
        auto end = self->getProp("end");
        if(!end || end->type != Jua_Val::Num)
            throw string("Range.next() called on a non-range object");
        auto res = new Jua_Obj(self->vm);
        bool done = index >= end->toInt();
        res->setProp("done", Jua_Bool::getInst(done));
        if(!done){
            auto value = new Jua_Num(self->vm, index);
            res->setProp("value", value);
            res->setProp("key", value);
        }
        return res;
    }));

    obj_next = makeFunc([](jualist& args){
        if(args.size() < 2) throw "Range.next() requires 2 arguments";
        auto self = args[0];
        if(self->type != Jua_Val::Obj){
            throw string("Range.next() called on a non-object");
        }
        auto obj = static_cast<Jua_Obj*>(self);
        auto key = args[1];
        std::unordered_map<string, Jua_Val*>::iterator it;
        if(key->type == Jua_Val::Str){
            string str_key = key->toString();
            it = obj->dict.find(str_key);
            it++;
        }else{
            it = obj->dict.begin();
        }
        auto res = new Jua_Obj(self->vm);
        if(it == obj->dict.end()){
            res->setProp("done", Jua_Bool::getInst(true));
        }else{
            auto value = new Jua_Str(self->vm, it->first);
            res->setProp("done", Jua_Bool::getInst(false));
            res->setProp("key", value);
            res->setProp("value", value);
        }
        return res;
    });
    ObjectProto->setProp("next", obj_next);
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