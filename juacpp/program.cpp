#include "jua-internal.h"

Jua_Val* LiteralStr::calc(Scope*){
    return new Jua_Str(value);
}
LiteralNum* LiteralNum::eval(const string& str){
    return new LiteralNum(std::stod(str));
}
Jua_Val* LiteralNum::calc(Scope*){
    return new Jua_Num(value);
}

Jua_Val* Keyword::calc(Scope* env){
    switch (type){
        case 'n': return Jua_Null::getInst();
        case 't': return Jua_Bool::getInst(true);
        case 'f': return Jua_Bool::getInst(false);
        case 'l': return env;
    }
}
Keyword* Keyword::null = new Keyword('n');
Keyword* Keyword::t = new Keyword('t');
Keyword* Keyword::f = new Keyword('f');
Keyword* Keyword::local = new Keyword('l');

Jua_Val* Varname::calc(Scope* env){
    auto val = env->getProp(str);
    if(!val){
        string msg = "Var not declared: ";
        msg.append(str);
        throw new JuaErrorWithVal(msg, env);
    }
    return val;
}
void Varname::assign(Scope*, Jua_Val*){
    throw "todo";
}
void Varname::declare(Scope*, Jua_Val*){
    throw "todo";
}

jualist FlexibleList::calc(Scope* env){
    jualist list;
    for(auto expr: exprs){
        list.push_back(expr->calc(env));
    }
    return list;
}
Jua_Val* Call::calc(Scope* env){
    auto fn = calee->calc(env);
    auto list = args->calc(env);
    return fn->call(list);
}

Jua_Val* FunctionBody::exec(Scope* env){
    auto controller = new Controller;
    Block::exec(env, controller);
    auto retval = controller->retval;
    if(retval)
        controller->retval = nullptr;
    else
        retval = Jua_Null::getInst();
    if(controller->isPending)
        throw "isPending";
    delete controller;
    return retval;
}