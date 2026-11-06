#include "jua-syntax.h"
#include "jua-vm.h"

Value* LiteralStr::calc(Scope* env){
    return new String(env->vm, value);
}
Value* Template::calc(Scope* env){
    string str = strList[0];
    for(size_t i=0; i<exprList.size(); i++){
        auto val = exprList[i]->calc(env);
        str += val->toString();
        str += strList[i+1];
    }
    return new String(env->vm, str);
}
LiteralNum* LiteralNum::eval(const string& str){
    return new LiteralNum(std::stod(str));
}
Value* LiteralNum::calc(Scope* env){
    return new Number(env->vm, value);
}

Value* Keyword::calc(Scope* env){
    switch (type){
        case 'n': return Null::getInst();
        case 't': return Boolean::getInst(true);
        case 'f': return Boolean::getInst(false);
        case 'l': return env;
    }
    throw "Keyword::calc";
}
Keyword* Keyword::null = new Keyword('n');
Keyword* Keyword::t = new Keyword('t');
Keyword* Keyword::f = new Keyword('f');
Keyword* Keyword::local = new Keyword('l');

Value* Varname::calc(Scope* env){
    auto val = env->getProp(str);
    if(!val){
        string msg = "Var not declared: ";
        msg.append(str);
        throw new JuaErrorWithVal(msg, env);
    }
    return val;
}
void Varname::assign(Scope* env, Value* val){
    env->assign(str, val);
}
void Varname::declare(Scope* env, Value* val){
    env->setProp(str, val);
}

void FlexibleList::appendTo(Scope* env, jualist& list){
    for(auto expr: exprs){
        list.push_back(expr->calc(env));
    }
}
jualist FlexibleList::calc(Scope* env){
    jualist list;
    appendTo(env, list);
    return list;
}

void DeclarationItem::addDefault(){
    if(!initval)initval = Keyword::null;
    body->addDefault();
}
void DeclarationItem::assign(Scope* env, Value* val){
    if(!val)val = initval->calc(env);
    body->assign(env, val);
}
void DeclarationItem::declare(Scope* env, Value* val){
    if(!val)val = initval->calc(env);
    body->declare(env, val);
}

void DeclarationList::assign(Scope *env, Value *val){
    auto it = val->getIterator();
    for(auto item: decItems){
        auto v = it->next();
        if(v || item->initval)
            item->assign(env, v);
        else
            throw new JuaError("Missing argument");
    }
    delete it;
}
void DeclarationList::declare(Scope *env, Value *val){
    auto it = val->getIterator();
    for(auto item: decItems){
        auto v = it->next();
        if(v || item->initval)
            item->declare(env, v);
        else
            throw new JuaError("Missing argument");
    }
    delete it;
}
void DeclarationList::rawDeclare(Scope* env, const jualist& vals){
    for(size_t i=0; i<decItems.size(); i++){
        auto item = decItems[i];
        if(i < vals.size())
            item->declare(env, vals[i]);
        else if(item->initval)
            item->declare(env, nullptr);
        else
            throw new JuaError("Missing argument");
    }
}

Value* OptionalPropRef::_calc(Scope* env){
    return expr->calc(env)->getProp(prop);
}
Value* OptionalPropRef::calc(Scope* env){
    auto val = _calc(env);
    if(val)return val;
    return Null::getInst();
}
Value* PropRef::calc(Scope* env){
    auto val = _calc(env);
    if(val)return val;
    throw new JuaError(std::format("no property: {}", prop));
}
void PropRef::assign(Scope* env, Value* val){
    auto tar = expr->calc(env);
    if(tar->type != Value::Obj)throw "not an object";
    auto obj = static_cast<Object*>(tar);
    obj->setProp(prop, val);
}
Value* MethWrapper::calc(Scope* env){
    auto obj = expr->calc(env);
    auto meth = obj->getProp(key);
    if(!meth)throw new JuaError(std::format("no method: {}", key));
    //必须登记捕获值：包装函数逃逸后，接收者不能只由这个 lambda 持有
    return new NativeFunction(env->vm, [obj, meth](jualist& args){
        args.push_front(obj);
        return meth->call(args);
    }, {obj, meth});
}
Value* Subscription::calc(Scope* env){
    auto obj = expr->calc(env);
    //obj 只存在于 C++ 栈上，键的求值可能触发回收
    RootGuard guard(env->vm);
    env->vm->gcRoots.push_back(obj);
    auto key = keyExpr->calc(env);
    return obj->getItem(key);
}
void Subscription::assign(Scope* env, Value* val){
    auto obj = expr->calc(env);
    RootGuard guard(env->vm);
    env->vm->gcRoots.push_back(obj);
    env->vm->gcRoots.push_back(val);
    auto key = keyExpr->calc(env);
    obj->setItem(key, val);
}

Value* Call::calc(Scope* env){
    //d_log("Call");
    auto fn = calee->calc(env);
    auto list = args->calc(env);
    //被调用者与实参只存在于 C++ 栈上，函数体执行期间的回收会误杀它们
    RootGuard guard(env->vm);
    env->vm->gcRoots.push_back(fn);
    for(auto v: list)env->vm->gcRoots.push_back(v);
    return fn->call(list);
}

Value* ArrayExpr::calc(Scope* env){
    auto arr = new Array(env->vm, {});
    //构建过程中 arr 与已写入的元素都必须保活
    RootGuard guard(env->vm);
    env->vm->gcRoots.push_back(arr);
    list->appendTo(env, arr->items); //避免复制
    return arr;
}
Value* ObjExpr::calc(Scope* env){
    auto obj = new Object(env->vm);
    RootGuard guard(env->vm);
    env->vm->gcRoots.push_back(obj);
    for(auto kv: entries){
        auto key = kv.first->calc(env), val = kv.second->calc(env);
        if(key->type != Value::Str)
            throw new JuaError("object literal key must be a string");
        obj->setProp(key->toString(), val);
    }
    return obj;
}

Value* UnitaryExpr::calc(Scope* env){
    auto val = pri->calc(env);
    return operate(oper, val);
}
Value* BinaryExpr::calc(Scope* env){
    auto leftVal = left->calc(env);
    //右操作数的求值可能触发回收，leftVal 必须保活
    RootGuard guard(env->vm);
    env->vm->gcRoots.push_back(leftVal);
    switch(oper){
        case BinOper::and_:
            return leftVal->toBoolean() ? right->calc(env) : leftVal;
        case BinOper::or_:
            return leftVal->toBoolean() ? leftVal : right->calc(env);
        default:
            return operate(oper, leftVal, right->calc(env));
    }
}
Value* Assignment::calc(Scope* env){
    auto val = right->calc(env);
    left->assign(env, val);
    return val;
}
Value* OperAssignment::calc(Scope* env){
    auto leftVal = left->calc(env);
    RootGuard guard(env->vm);
    env->vm->gcRoots.push_back(leftVal);
    Value* result;
    //逻辑赋值号的短路作用于运算，而不是赋值（见 draft/表达式.md）
    if(type == BinOper::and_){
        result = leftVal->toBoolean() ? right->calc(env) : leftVal;
    }else if(type == BinOper::or_){
        result = leftVal->toBoolean() ? leftVal : right->calc(env);
    }else{
        result = operate(type, leftVal, right->calc(env));
    }
    assignee->assign(env, result);
    return result;
}
Value* TernaryExpr::calc(Scope* env){
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
void LeftObj::assign(Scope* env, Value* val){
    forEach(env, val, DeclarationItem::assign); //& ?
}
void LeftObj::declare(Scope* env, Value* val){
    forEach(env, val, DeclarationItem::declare);
}
void LeftObj::forEach(Scope *env, Value *obj, Callback cb){
    //obj 与取出的 val 在整个解构过程中都必须保活（子声明项可能求初始值并触发回收）
    RootGuard guard(env->vm);
    env->vm->gcRoots.push_back(obj);
    for(auto [keyExpr, decItem]: entries){
        auto key = keyExpr->calc(env);
        if(key->type != Value::Str)
            throw new JuaError("non-string key");
        auto val = obj->getProp(key->toString());
        env->vm->gcRoots.push_back(val);
        if(val || decItem->initval)
            (decItem->*cb)(env, val);
        else
            throw new JuaError(std::format("cannot read property: {}", key->toString()));
    }
}


void Return::exec(Scope* env, Controller* ctrl){
    ctrl->retval = expr ? expr->calc(env) : Null::getInst();
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
    RootGuard guard(env->vm);
    env->vm->gcRoots.push_back(exprVal);
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
    auto it = target->getIterator(env->vm->obj_next);
    RootGuard guard(env->vm);
    env->vm->gcRoots.push_back(target); //迭代目标必须在整个循环期间存活
    size_t base = env->vm->gcRoots.size();
    while(true){
        //每轮重置到“只有 target”的状态，再登记迭代器内部持有的键
        env->vm->gcRoots.resize(base);
        it->rootValues(env->vm);
        Value* value = it->next();
        if(!value)break;
        env->vm->gcRoots.push_back(value);
        declarable->declare(env, value);
        body->exec(new Scope(env), controller);
        controller->continuing = false;
        if(controller->isPending()){
            controller->breaking = false;
            break;
        }
    }
    delete it;
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
    for(auto stmt: statements){
        //安全点：此刻活跃的 jua 值只有 env 与待返回值，二者都作为根传入
        env->vm->safePoint(env, controller->retval);
        if(controller->isPending())break;
        stmt->exec(env, controller);
    }
}
Value* FunctionBody::exec(Scope* env){
    auto controller = new Controller;
    Block::exec(env, controller);
    auto retval = controller->retval;
    if(retval)
        controller->retval = nullptr;
    else
        retval = Null::getInst();
    if(controller->isPending())
        throw new JuaError("unexpected control flow leaving function body");
    delete controller;
    return retval;
}

void Jua_PFunc::trace(VM* vm){
    //闭包持有定义作用域，必须跟着标记，否则 upenv 被回收后调用会访问野指针
    vm->markFrom(upenv);
}