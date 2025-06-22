#include "jua-syntax.h"
#include "jua-vm.h"

Jua_Val* LiteralStr::calc(Scope* env){
    return new Jua_Str(env->vm, value);
}
Jua_Val* Template::calc(Scope* env){
    string str = strList[0];
    for(size_t i=0; i<exprList.size(); i++){
        auto val = exprList[i]->calc(env);
        str += val->toString();
        str += strList[i+1];
    }
    return new Jua_Str(env->vm, str);
}
LiteralNum* LiteralNum::eval(const string& str){
    return new LiteralNum(std::stod(str));
}
Jua_Val* LiteralNum::calc(Scope* env){
    return new Jua_Num(env->vm, value);
}

Jua_Val* Keyword::calc(Scope* env){
    switch (type){
        case 'n': return Jua_Null::getInst();
        case 't': return Jua_Bool::getInst(true);
        case 'f': return Jua_Bool::getInst(false);
        case 'l': return env;
    }
    throw "Keyword::calc";
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
void Varname::assign(Scope* env, Jua_Val* val){
    env->assign(str, val);
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
void DeclarationItem::assign(Scope* env, Jua_Val* val){
    if(!val)val = initval->calc(env);
    body->assign(env, val);
}
void DeclarationItem::declare(Scope* env, Jua_Val* val){
    if(!val)val = initval->calc(env);
    body->declare(env, val);
}

void DeclarationList::assign(Scope *env, Jua_Val *val){
    //需要迭代器
    throw "todo: DeclarationList::assign";
}
void DeclarationList::declare(Scope *env, Jua_Val *val){
    throw "todo: DeclarationList::declare";
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
    throw new JuaError(std::format("no property: {}", prop));
}
void PropRef::assign(Scope* env, Jua_Val* val){
    auto tar = expr->calc(env);
    if(tar->type != Jua_Val::Obj)throw "not an object";
    auto obj = static_cast<Jua_Obj*>(tar);
    obj->setProp(prop, val);
}
Jua_Val* MethWrapper::calc(Scope* env){
    auto obj = expr->calc(env);
    auto meth = obj->getProp(key);
    if(!meth)throw new JuaError(std::format("no method: {}", key));
    return new Jua_NativeFunc(env->vm, [obj, meth](jualist& args){
        args.push_front(obj);
        return meth->call(args);
    });
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
    return new Jua_Array(env->vm, list->calc(env));
}
Jua_Val* ObjExpr::calc(Scope* env){
    auto obj = new Jua_Obj(env->vm);
    for(auto kv: entries){
        auto key = kv.first->calc(env), val = kv.second->calc(env);
        if(key->type != Jua_Val::Str)
            throw "non-string key";
        obj->setProp(key->toString(), val);
    }
    return obj;
}

Jua_Val* UnitaryExpr::calc(Scope* env){
    auto val = pri->calc(env);
    return operate(oper, val);
}
Jua_Val* BinaryExpr::calc(Scope* env){
    auto lv = left->calc(env), rv = right->calc(env);
    return operate(oper, lv, rv);
}
Jua_Val* Assignment::calc(Scope* env){
    auto val = right->calc(env);
    left->assign(env, val);
    return val;
}
Jua_Val* OperAssignment::calc(Scope* env){
    auto leftVal = left->calc(env);
    auto rightVal = right->calc(env);
    Jua_Val* result = operate(type, leftVal, rightVal);
    assignee->assign(env, result);
    return result;
}
Jua_Val* TernaryExpr::calc(Scope* env){
    auto cond = condExpr->calc(env);
    if(cond->toBoolean())
        return trueExpr->calc(env);
    else
        return falseExpr->calc(env);
}

void LeftObj::addDefault(){
    if(auto_nulled)return;
    for(auto entry: entries){
        entry.second->addDefault();
    }
    auto_nulled = true;
}
void LeftObj::assign(Scope* env, Jua_Val* val){
    forEach(env, val, DeclarationItem::assign); //& ?
}
void LeftObj::declare(Scope* env, Jua_Val* val){
    forEach(env, val, DeclarationItem::declare);
}
void LeftObj::forEach(Scope *env, Jua_Val *obj, Callback cb){
    for(auto [keyExpr, decItem]: entries){
        auto key = keyExpr->calc(env);
        if(key->type != Jua_Val::Str)
            throw new JuaError("non-string key");
        auto val = obj->getProp(key->toString());
        if(val || decItem->initval)
            (decItem->*cb)(env, val);
        else
            throw new JuaError(std::format("cannot read property: {}", key->toString()));
    }
}


void Return::exec(Scope* env, Controller* ctrl){
    ctrl->retval = expr ? expr->calc(env) : Jua_Null::getInst();
}

IfStmt::IfStmt(Expr* c, Block* b, Block* e): cond(c), body(b), elseBody(e){
    pending_continue = body->pending_continue ? body->pending_continue
                     : elseBody ? elseBody->pending_continue : nullptr;
    pending_break = body->pending_break ? body->pending_break
                     : elseBody ? elseBody->pending_break : nullptr;
}
void IfStmt::exec(Scope* env, Controller* controller){
    if(cond->calc(env)->toBoolean())
        body->exec(new Scope(env), controller);
    else if(elseBody)
        elseBody->exec(new Scope(env), controller);
}
SwitchStmt::SwitchStmt(Expr* e, std::vector<CaseBlock*>& cs, Block* d):
    expr(e), cases(cs), defaultBody(d){
    pending_continue = defaultBody ? defaultBody->pending_continue : nullptr;
    pending_break = defaultBody ? defaultBody->pending_break : nullptr;
    for(auto cb: cases){
        if(!pending_continue)
            pending_continue = cb->body->pending_continue;
        if(!pending_break)
            pending_break = cb->body->pending_break;
    }
}
void SwitchStmt::exec(Scope* env, Controller* controller){
    auto exprVal = expr->calc(env);
    for(auto cb: cases){
        if(cb->cond->contains(env, exprVal)){
            cb->body->exec(new Scope(env), controller);
            return;
        }
    }
    if(defaultBody)
        defaultBody->exec(new Scope(env), controller);
}
void WhileStmt::exec(Scope* env, Controller* controller){
    while(cond->calc(env)->toBoolean()){
        body->exec(new Scope(env), controller);
        controller->continuing = false;
        if(controller->isPending()){
            controller->breaking = false;
            break;
        }
    }
}
void ForStmt::exec(Scope* env, Controller* controller){
    auto target = iterable->calc(env);
    auto next = target->getMetaMethod("next");
    if(!next){
        if(target->type == Jua_Val::Obj)
            next = target->vm->obj_next;
        else
            throw "iterable without next() method must be an object";
    }
    Jua_Val *nextResult, *value, *done, *key = Jua_Null::getInst();
    while(true){
        nextResult = next->call({target, key});
        done = nextResult->getProp("done");
        if(!done)throw "next() must return an value with 'done' property";
        if(done->toBoolean())break;
        value = nextResult->getProp("value");
        if(!value)throw "next() must return an value with 'value' property if not done";
        key = nextResult->getProp("key");
        if(!key)throw "next() must return an value with 'key' property if not done";
        declarable->declare(env, value);
        body->exec(new Scope(env), controller);
        controller->continuing = false;
        if(controller->isPending()){
            controller->breaking = false;
            break;
        }
    }
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
        if(controller->isPending())break;
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
    if(controller->isPending())
        throw "isPending";
    delete controller;
    return retval;
}