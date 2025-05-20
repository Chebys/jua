#include "jua-internal.h"

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

}
void JuaVM::makeGlobal(){
    _G = new Scope;
    _G->addRef();
    _G->setProp("_G", _G);
    auto print = [this](jualist& args){
        j_stdout(args);
        return Jua_Null::getInst();
    };
    _G->setProp("print", new Jua_NativeFunc(print));
    //auto val = _G->getProp("print");
}
Jua_Val* JuaVM::require(const string& name){
    if(modules.contains(name))return modules[name];
    //todo: 检查循环导入
    const string& script = findModule(name);
    modules[name] = eval(script);
    return modules[name];
}