#include "jua-internal.h"

Jua_Val* Jua_Val::inheritProp(const string& key){
    if(!proto)return nullptr;
    if(proto->isPropTrue("isClass")){
        Jua_Val* super = getOwn("super");
        if(!super)return nullptr;
        return super->getProp(key);
    }
    return proto->getProp(key);
}
Jua_Val* Jua_Val::getProp(const string& key){
    auto own = getOwn(key);
    if(own)return own;
    return inheritProp(key);
}
Jua_Func* Jua_Val::getMetaMethod(const string& key){
    if(!proto)return nullptr;
    auto meth = proto->getProp(key);
    if(meth && meth->type==Func)
        return (Jua_Func*)meth;
    return nullptr;
}
Jua_Bool* Jua_Val::hasItem(Jua_Val* key){
    auto fn = getMetaMethod("hasItem");
    if(fn)return fn->call({this, key})->toJuaBool();
    throw "type err";
}
Jua_Val* Jua_Val::getItem(Jua_Val* key){
    auto fn = getMetaMethod("getItem");
    if(fn)return fn->call({this, key});
    throw "type err";
}
void Jua_Val::setItem(Jua_Val* key, Jua_Val* val){
    auto fn = getMetaMethod("setItem");
    if(!fn)throw "type err";
    fn->call({this, key, val});
}
Jua_Val* Jua_Val::call(jualist& args){
    auto fn = getMetaMethod("__call");
    if(!fn)throw "type err";
    args.push_front(this);
    return fn->call(args);
    
}
Jua_Val* Jua_Val::add(Jua_Val* val){
    Jua_Func* fn = getMetaMethod("__add");
    if(fn)return fn->call({this, val});
    throw "type err";
}
Jua_Val* Jua_Val::sub(Jua_Val* val){
    Jua_Func* fn = getMetaMethod("__sub");
    if(fn)return fn->call({this, val});
    throw "type err";
}
Jua_Val* Jua_Val::mul(Jua_Val* val){
    Jua_Func* fn = getMetaMethod("__mul");
    if(fn)return fn->call({this, val});
    throw "type err";
}
Jua_Val* Jua_Val::div(Jua_Val* val){
    Jua_Func* fn = getMetaMethod("__div");
    if(fn)return fn->call({this, val});
    throw "type err";
}
Jua_Bool* Jua_Val::eq(Jua_Val* val){
    return Jua_Bool::getInst(*this == val);
}
Jua_Bool* Jua_Val::lt(Jua_Val* val){
    Jua_Func* fn = getMetaMethod("__lt");
    if(fn)return fn->call({this, val})->toJuaBool();
    throw "type err";
}
Jua_Bool* Jua_Val::le(Jua_Val* val){
    Jua_Func* fn = getMetaMethod("__le");
    if(fn)return fn->call({this, val})->toJuaBool();
    throw "type err";
}
Jua_Val* Jua_Val::range(Jua_Val* val){
    Jua_Func* fn = getMetaMethod("range");
    if(fn)return fn->call({this, val});
    throw "type err";
}
bool Jua_Val::operator==(Jua_Val* val){
    Jua_Func* fn = getMetaMethod("__eq");
    if(fn)return fn->call({this, val})->toBoolean();
    return this == val;
}
Jua_Bool* Jua_Val::toJuaBool(){
    return Jua_Bool::getInst(toBoolean());
}

Jua_Bool* Jua_Obj::hasItem(Jua_Val* key){
    auto fn = getMetaMethod("hasItem");
    if(fn)return fn->call({this, key})->toJuaBool();
    if(key->type!=Str)
        throw "type error";
    return Jua_Bool::getInst(getProp(key->toString()));
}
Jua_Val* Jua_Obj::getItem(Jua_Val* key){
    auto fn = getMetaMethod("getItem");
    if(fn)return fn->call({this, key});
    if(key->type!=Str)
        throw "type error";
    Jua_Val* val = getProp(key->toString());
    return val ? val : Jua_Null::getInst();
}
void Jua_Obj::setItem(Jua_Val* key, Jua_Val* val){
    auto fn = getMetaMethod("setItem");
    if(fn){
        fn->call({this, key, val});
        return;
    }
    if(key->type!=Str)
        throw "type error";
    setProp(key->toString(), val);
}
void Jua_Obj::assignProps(Jua_Obj* obj){
    for(auto& pair : obj->dict){
        dict[pair.first] = pair.second;
    }
}
string Jua_Obj::toString(){
    auto fn = getMetaMethod("toString");
    if(fn)return fn->call({this})->toString();
    return safeToString();
}
string Jua_Obj::safeToString(){
    if(dict.empty())return "{}";
    string str("{");
    auto it = dict.begin();
    str.append(it->first);
    for(it++; it!=dict.end(); it++){
        str.append(", ");
        str.append(it->first);
    }
    str.append("}");
    return str;
}
bool Jua_Obj::isPropTrue(const char* key){
    throw "todo";
}

Jua_Val* Jua_Num::add(Jua_Val* val){
    if(val->type!=Num)throw "type error";
    auto num = static_cast<Jua_Num*>(val);
    return new Jua_Num(value + num->value);
}
Jua_Val* Jua_Num::sub(Jua_Val* val){
    if(val->type!=Num)throw "type error";
    auto num = static_cast<Jua_Num*>(val);
    return new Jua_Num(value - num->value);
}
Jua_Val* Jua_Num::mul(Jua_Val* val){
    if(val->type!=Num)throw "type error";
    auto num = static_cast<Jua_Num*>(val);
    return new Jua_Num(value * num->value);
}
Jua_Val* Jua_Num::div(Jua_Val* val){
    if(val->type!=Num)throw "type error";
    auto num = static_cast<Jua_Num*>(val);
    return new Jua_Num(value / num->value);
}
Jua_Bool* Jua_Num::lt(Jua_Val* val){
    if(val->type!=Num)throw "type error";
    auto num = static_cast<Jua_Num*>(val);
    return Jua_Bool::getInst(value < num->value);
}
Jua_Bool* Jua_Num::le(Jua_Val* val){
    if(val->type!=Num)throw "type error";
    auto num = static_cast<Jua_Num*>(val);
    return Jua_Bool::getInst(value <= num->value);
}
Jua_Val* Jua_Num::range(Jua_Val* val){
    if(val->type!=Num)throw "type error";
    auto num = static_cast<Jua_Num*>(val);
    throw "todo";
}
bool Jua_Num::operator==(Jua_Val* val){
    if(!val || val->type!=Num)
        return false;
    return value == static_cast<Jua_Num*>(val)->value;
}
string Jua_Num::toString(){
    return std::to_string(value); //todo: 去除尾随0
}

size_t correctIndex(Jua_Val* num, size_t len){
    if(num->type!=Jua_Val::Num)throw "type error";
    size_t index = num->toInt();
    return index % len;
}

Jua_Bool* Jua_Str::hasItem(Jua_Val* val){
    if(val->type!=Str)return Jua_Bool::getInst(false);
    return Jua_Bool::getInst(value.find(val->toString()) != string::npos);
}
Jua_Val* Jua_Str::getItem(Jua_Val* key){
    size_t i = correctIndex(key, value.length());
    return new Jua_Str({value[i]});
}
bool Jua_Str::operator==(Jua_Val* val){
    if(!val || val->type!=Str)
        return false;
    return value == static_cast<Jua_Str*>(val)->value;
}

Jua_Val* Scope::inheritProp(const string& key){
    if(parent)return parent->getProp(key);
    return nullptr;
}

Jua_Val* Jua_Array::getItem(Jua_Val* key){
    size_t i = correctIndex(key, items.size());
    return items[i];
}
void Jua_Array::setItem(Jua_Val* key, Jua_Val* val){
    size_t i = correctIndex(key, items.size());
    items[i] = val;
}

Jua_Val* Jua_Buffer::getItem(Jua_Val* key){
    size_t i = correctIndex(key, length);
    return new Jua_Num(bytes[i]);
}
void Jua_Buffer::setItem(Jua_Val* key, Jua_Val* val){
    size_t i = correctIndex(key, length);
    bytes[i] = val->toInt();
}
Jua_Val* Jua_Buffer::read(Jua_Val* _start, Jua_Val* _end){
    size_t start = correctIndex(_start, length);
    size_t end = correctIndex(_end, length);
    if(!end)end = length;
    if(start>=end)throw "range error";
    size_t len = end-start;
    auto str = new Jua_Str();
    str->value.resize(len);
    memcpy(&str->value[0], bytes+start, len);
    return str;
}
void Jua_Buffer::write(Jua_Val* _str, Jua_Val* _pos){
    if(!_str || _str->type!=Str)
        throw "type error";
    std::string& str = static_cast<Jua_Str*>(_str)->value;
    size_t pos = correctIndex(_pos, length);
    size_t len = str.size();
    if(pos+len > length)
        throw "out of range";
    memcpy(bytes+pos, &str[0], len);
}