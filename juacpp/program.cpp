#include "jua-syntax.h"

Jua_Val* LiteralStr::calc(Scope*){
    return new Jua_Str(value);
}
Jua_Val* Template::calc(Scope* env){
    string str = strList[0];
    for(size_t i=0; i<exprList.size(); i++){
        auto val = exprList[i]->calc(env);
        str += val->toString();
        str += strList[i+1];
    }
    return new Jua_Str(str);
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
    throw "todo: Varname::assign";
}
void Varname::declare(Scope* env, Jua_Val* val){
    env->setProp(str, val);
}

jualist FlexibleList::calc(Scope* env){
    jualist list;
    for(auto expr: exprs){
        list.push_back(expr->calc(env));
    }
    return list;
}

void DeclarationItem::addDefault(){
    if(!initval)initval = Keyword::null;
    body->addDefault();
}
void DeclarationItem::declare(Scope* env, Jua_Val* val){
    if(!val)val = initval->calc(env);
    body->declare(env, val);
}

void DeclarationList::rawDeclare(Scope* env, const jualist& vals){
    for(size_t i=0; i<decItems.size(); i++){
        auto item = decItems[i];
        if(i < vals.size())
            item->declare(env, vals[i]);
        else if(item->initval)
            item->declare(env, nullptr);
        else
            throw "Missing argument";
    }
}

Jua_Val* OptionalPropRef::_calc(Scope* env){
    return expr->calc(env)->getProp(prop);
}
Jua_Val* OptionalPropRef::calc(Scope* env){
    auto val = _calc(env);
    if(val)return val;
    return Jua_Null::getInst();
}
Jua_Val* PropRef::calc(Scope* env){
    auto val = _calc(env);
    if(val)return val;
    throw "no property";
}
void PropRef::assign(Scope* env, Jua_Val* val){
    auto tar = expr->calc(env);
    if(tar->type != Jua_Val::Obj)throw "not an object";
    auto obj = static_cast<Jua_Obj*>(tar);
    obj->setProp(prop, val);
}
Jua_Val* Subscription::calc(Scope* env){
    auto obj = expr->calc(env);
    auto key = keyExpr->calc(env);
    return obj->getItem(key);
}
void Subscription::assign(Scope* env, Jua_Val* val){
    auto obj = expr->calc(env);
    auto key = keyExpr->calc(env);
    obj->setItem(key, val);
}

Jua_Val* Call::calc(Scope* env){
    //d_log("Call");
    auto fn = calee->calc(env);
    auto list = args->calc(env);
    return fn->call(list);
}

Jua_Val* ArrayExpr::calc(Scope* env){
    return new Jua_Array(list->calc(env));
}
Jua_Val* ObjExpr::calc(Scope* env){
    auto obj = new Jua_Obj;
    for(auto kv: entries){
        auto key = kv.first->calc(env), val = kv.second->calc(env);
        if(key->type != Jua_Val::Str)
            throw "non-string key";
        obj->setProp(key->toString(), val);
    }
    return obj;
}

Jua_Val* BinaryExpr::calc(Scope* env){
    auto lv = left->calc(env), rv = right->calc(env);
    return operate(oper, lv, rv);
}

void Return::exec(Scope* env, Controller* ctrl){
    ctrl->retval = expr ? expr->calc(env) : Jua_Null::getInst();
}


Block::Block(Stmts stmts): statements(stmts){
    for(auto stmt: stmts){
        if(!pending_continue)
            pending_continue = stmt->pending_continue;
        if(!pending_break)
            pending_break = stmt->pending_break;
    }
}
void Block::exec(Scope* env, Controller* controller){ //env是新产生的作用域
    //d_log("Block::exec");
    for(auto stmt: statements)
        if(controller->isPending)break;
        else stmt->exec(env, controller);
    env->gc();
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