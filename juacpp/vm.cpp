#include "jua-internal.h"
JuaVM::JuaVM(): JuaVM(string()){}
JuaVM::JuaVM(const string& s): script(s){
    initBuiltins();
    _G = new Scope;
    makeGlobal();
}

void JuaVM::run(){
    try{
        auto val = eval(script);
        val->gc();
    }catch(JuaError* e){
        j_stderr(e);
    }
}

Jua_Val* JuaVM::eval(const string& script){
    //返回非空指针，可能需要垃圾回收
    auto body = parse(script);
    return body->exec(new Scope(_G));
}
void JuaVM::initBuiltins(){
    //pass
}
void JuaVM::makeGlobal(){
    _G->ref++;
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