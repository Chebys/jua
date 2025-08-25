#include "jua-value.h"
#include <charconv>
#include <format>
#include "jua-vm.h"

struct ListIterator: JuaIterator{
    jualist& list;
    size_t index = 0;
    ListIterator(jualist& l): list(l) {}
    ListIterator(Jua_Array* arr): list(arr->items) {}
    Jua_Val* next(){
        if(index >= list.size())return nullptr;
        return list[index++];
    }
};
struct CustomIterator: JuaIterator{
    Jua_Val* obj;
    Jua_Func* nextFn;
    Jua_Val* key = Jua_Null::getInst();
    CustomIterator(Jua_Val* o, Jua_Func* fn): obj(o), nextFn(fn){}
    CustomIterator(Jua_Val* o): obj(o){
        nextFn = obj->getMetaMethod("next");
        if(!nextFn)
            throw new JuaTypeError("Object is not iterable");
    }
    Jua_Val* next(){
        auto res = nextFn->call({obj, key});
        if(res->type != Jua_Val::Obj)
            throw new JuaTypeError("iterator.next() must return an object");
        auto done = res->getProp("done");
        if(!done || done->type != Jua_Val::Bool)
            throw new JuaTypeError("iterator.next() must return an object with a boolean 'done' property");
        if(done->toBoolean()){
            return nullptr;
        }
        auto value = res->getProp("value");
        if(!value)
            throw new JuaTypeError("iterator.next() must return an object with a 'value' property when done is false");
        key = res->getProp("key");
        if(!key)
            throw new JuaTypeError("iterator.next() must return an object with a 'key' property when done is false");
        return value;
    }
};

void Jua_Val::release(){
    ref--;
    gc();
}
Jua_Val* Jua_Val::inheritProp(const string& key){
    if(!proto)return nullptr;
    if(proto->isPropTrue("__class")){
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
    throw new JuaTypeError("Object is not subscriptable");
}
Jua_Val* Jua_Val::getItem(Jua_Val* key){
    auto fn = getMetaMethod("getItem");
    if(fn)return fn->call({this, key});
    throw new JuaTypeError("Object is not subscriptable");
}
void Jua_Val::setItem(Jua_Val* key, Jua_Val* val){
    auto fn = getMetaMethod("setItem");
    if(!fn)throw new JuaTypeError("Object is not subscriptable");
    fn->call({this, key, val});
}
Jua_Val* Jua_Val::call(jualist& args){
    auto fn = getMetaMethod("__call");
    if(!fn)throw new JuaTypeError("Object is not callable");
    args.push_front(this);
    return fn->call(args);
    
}
Jua_Val* Jua_Val::unm(){
    Jua_Func* fn = getMetaMethod("__unm");
    if(fn)return fn->call({this});
    throw new JuaTypeError("Object is not unary negatable");
}
Jua_Val* Jua_Val::add(Jua_Val* val){
    Jua_Func* fn = getMetaMethod("__add");
    if(fn)return fn->call({this, val});
    throw new JuaTypeError("Object is not addable");
}
Jua_Val* Jua_Val::sub(Jua_Val* val){
    Jua_Func* fn = getMetaMethod("__sub");
    if(fn)return fn->call({this, val});
    throw new JuaTypeError("Object is not subtractable");
}
Jua_Val* Jua_Val::mul(Jua_Val* val){
    Jua_Func* fn = getMetaMethod("__mul");
    if(fn)return fn->call({this, val});
    throw new JuaTypeError("Object is not multiplicable");
}
Jua_Val* Jua_Val::div(Jua_Val* val){
    Jua_Func* fn = getMetaMethod("__div");
    if(fn)return fn->call({this, val});
    throw new JuaTypeError("Object is not dividable");
}
Jua_Bool* Jua_Val::eq(Jua_Val* val){
    return Jua_Bool::getInst(*this == val);
}
Jua_Bool* Jua_Val::lt(Jua_Val* val){
    Jua_Func* fn = getMetaMethod("__lt");
    if(fn)return fn->call({this, val})->toJuaBool();
    throw new JuaTypeError("Object is not comparable");
}
Jua_Bool* Jua_Val::le(Jua_Val* val){
    Jua_Func* fn = getMetaMethod("__le");
    if(fn)return fn->call({this, val})->toJuaBool();
    throw new JuaTypeError("Object is not comparable");
}
Jua_Val* Jua_Val::range(Jua_Val* val){
    Jua_Func* fn = getMetaMethod("range");
    if(fn)return fn->call({this, val});
    throw new JuaTypeError("Object is not rangeable");
}
bool Jua_Val::operator==(Jua_Val* val){
    Jua_Func* fn = getMetaMethod("__eq");
    if(fn)return fn->call({this, val})->toBoolean();
    return this == val;
}
Jua_Bool* Jua_Val::toJuaBool(){
    return Jua_Bool::getInst(toBoolean());
}
int64_t Jua_Val::toInt(){
    throw new JuaTypeError("toInt() called on a non-number value");
}
double Jua_Val::toNumber(){
    throw new JuaTypeError("toNumber() called on a non-number value");
}
JuaIterator* Jua_Val::getIterator(Jua_Func* next){
    auto nextFn = getMetaMethod("next");
    if(!nextFn){
        if(!next)
            throw new JuaTypeError("Object is not iterable");
        nextFn = next;
    }
    return new CustomIterator(this, nextFn);
}
void Jua_Val::collectItems(jualist& list){
    auto it = getIterator();
    Jua_Val* val;
    while((val = it->next()) != nullptr){
        list.push_back(val);
    }
    delete it;
}

void Jua_Val::gc(){
    //todo
    if(!ref){
        //d_log("gc()");
        //delete this;
    }
}

Jua_Obj::Jua_Obj(JuaVM* vm_, Jua_Obj* p): Jua_Val(vm_, Obj, p){ if(p)p->addRef(); }
Jua_Obj::~Jua_Obj(){ if(proto)proto->release(); }
bool Jua_Obj::hasOwn(Jua_Val* key){
    if(key->type != Str)throw new JuaError("non-string key");
    return dict.contains(key->toString());
}
void Jua_Obj::setProp(const string& key, Jua_Val* val){
    if(dict.contains(key)){
        dict[key]->release();
    }
    dict[key] = val;
    val->addRef();
}
void Jua_Obj::delProp(const string& key){
    if(dict.contains(key)){
        dict[key]->release();
        dict.erase(key);
    }
}
Jua_Bool* Jua_Obj::hasItem(Jua_Val* key){
    auto fn = getMetaMethod("hasItem");
    if(fn)return fn->call({this, key})->toJuaBool();
    if(key->type!=Str)
        throw new JuaTypeError("Object.hasItem: key must be a string");
    return Jua_Bool::getInst(getProp(key->toString()));
}
Jua_Val* Jua_Obj::getItem(Jua_Val* key){
    auto fn = getMetaMethod("getItem");
    if(fn)return fn->call({this, key});
    if(key->type!=Str)
        throw new JuaTypeError("Object.getItem: key must be a string");
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
        throw new JuaTypeError("Object.setItem: key must be a string");
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
    return getProp(key) == Jua_Bool::getInst(true);
}

Jua_Num::Jua_Num(JuaVM* vm, double v): Jua_Val(vm, Num, vm->NumberProto), value(v){}
Jua_Val* Jua_Num::unm(){
    return new Jua_Num(vm, -value);
}
Jua_Val* Jua_Num::add(Jua_Val* val){
    if(val->type!=Num)throw new JuaTypeError("try to add non-number");
    auto num = static_cast<Jua_Num*>(val);
    return new Jua_Num(vm, value + num->value);
}
Jua_Val* Jua_Num::sub(Jua_Val* val){
    if(val->type!=Num)throw new JuaTypeError("try to sub non-number");
    auto num = static_cast<Jua_Num*>(val);
    return new Jua_Num(vm, value - num->value);
}
Jua_Val* Jua_Num::mul(Jua_Val* val){
    if(val->type!=Num)throw new JuaTypeError("try to mul non-number");
    auto num = static_cast<Jua_Num*>(val);
    return new Jua_Num(vm, value * num->value);
}
Jua_Val* Jua_Num::div(Jua_Val* val){
    if(val->type!=Num)throw new JuaTypeError("try to div non-number");
    auto num = static_cast<Jua_Num*>(val);
    return new Jua_Num(vm, value / num->value);
}
Jua_Bool* Jua_Num::lt(Jua_Val* val){
    if(val->type!=Num)throw new JuaTypeError("try to compare non-number");
    auto num = static_cast<Jua_Num*>(val);
    return Jua_Bool::getInst(value < num->value);
}
Jua_Bool* Jua_Num::le(Jua_Val* val){
    if(val->type!=Num)throw new JuaTypeError("try to compare non-number");
    auto num = static_cast<Jua_Num*>(val);
    return Jua_Bool::getInst(value <= num->value);
}
Jua_Val* Jua_Num::range(Jua_Val* val){
    if(val->type!=Num)throw new JuaTypeError("try to create range with non-number");
    auto num = static_cast<Jua_Num*>(val);
    auto range = new Jua_Obj(vm, vm->RangeProto);
    range->setProp("start", new Jua_Num(vm, value));
    range->setProp("step", new Jua_Num(vm, 1));
    range->setProp("end", new Jua_Num(vm, num->value));
    return range;
}
bool Jua_Num::operator==(Jua_Val* val){
    if(!val || val->type!=Num)
        return false;
    return value == static_cast<Jua_Num*>(val)->value;
}
string Jua_Num::toString(){
    char s[32];
    auto res = std::to_chars(s, s+32, value);
    return {s, res.ptr};
}

size_t correctIndex(Jua_Val* num, size_t len){
    return num->toInt() % len;
}

Jua_Str::Jua_Str(JuaVM* vm, const string& v): Jua_Val(vm, Str, vm->StringProto), value(v){}
Jua_Bool* Jua_Str::hasItem(Jua_Val* val){
    if(val->type!=Str)return Jua_Bool::getInst(false);
    return Jua_Bool::getInst(value.find(val->toString()) != string::npos);
}
Jua_Val* Jua_Str::getItem(Jua_Val* key){
    size_t i = correctIndex(key, value.length());
    return new Jua_Str(vm, {value[i]});
}
Jua_Val* Jua_Str::add(Jua_Val* val){
    if(val->type!=Str)throw new JuaTypeError("try to add non-string");
    auto str = static_cast<Jua_Str*>(val);
    return new Jua_Str(vm, value + str->value);
}
bool Jua_Str::operator==(Jua_Val* val){
    if(!val || val->type!=Str)
        return false;
    return value == static_cast<Jua_Str*>(val)->value;
}

Jua_Val* Scope::inheritProp(const string& key){
    if(parent)return parent->getProp(key); //指针可能已经失效？？？
    return nullptr;
}

Jua_Array::Jua_Array(JuaVM* vm, const jualist& list): Jua_Obj(vm, vm->ArrayProto), items(list){}
Jua_Val* Jua_Array::getItem(Jua_Val* key){
    size_t i = correctIndex(key, items.size());
    return items[i];
}
void Jua_Array::setItem(Jua_Val* key, Jua_Val* val){
    size_t i = correctIndex(key, items.size());
    items[i] = val;
}
JuaIterator* Jua_Array::getIterator(Jua_Func* next){
    return new ListIterator(items);
}

Jua_Buffer::Jua_Buffer(JuaVM* vm, size_t len):Jua_Obj(vm, vm->BufferProto), length(len){
    bytes = new uint8_t[len];
}
Jua_Val* Jua_Buffer::getItem(Jua_Val* key){
    size_t i = correctIndex(key, length);
    return new Jua_Num(vm, bytes[i]);
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
    auto str = new Jua_Str(vm);
    str->value.resize(len);
    memcpy(&str->value[0], bytes+start, len);
    return str;
}
void Jua_Buffer::write(Jua_Val* _str, Jua_Val* _pos){
    if(!_str || _str->type!=Str)
        throw new JuaTypeError("value must be a string");
    std::string& str = static_cast<Jua_Str*>(_str)->value;
    size_t pos = _pos ? correctIndex(_pos, length) : 0;
    size_t len = str.size();
    if(pos+len > length)
        throw "out of range";
    memcpy(bytes+pos, &str[0], len);
}

string JuaError::toDebugString(){
    if(!message.size())return "JuaError";
    return std::format("JuaError: {}", message);
}

