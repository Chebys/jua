#include "jua-value.h"
#include <charconv>
#include <format>
#include "jua-vm.h"
using namespace Jua;

struct ListIterator: JuaIterator{
    jualist& list;
    size_t index = 0;
    ListIterator(jualist& l): list(l) {}
    ListIterator(Array* arr): list(arr->items) {}
    Value* next(){
        if(index >= list.size())return nullptr;
        return list[index++];
    }
};
struct CustomIterator: JuaIterator{
    Value* obj;
    Function* nextFn;
    Value* key = Null::getInst();
    CustomIterator(Value* o, Function* fn): obj(o), nextFn(fn){}
    CustomIterator(Value* o): obj(o){
        nextFn = obj->getMetaMethod("next");
        if(!nextFn)
            throw new JuaTypeError("Object is not iterable");
    }
    Value* next(){
        auto res = nextFn->call({obj, key});
        if(res->type != Value::Obj)
            throw new JuaTypeError("iterator.next() must return an object");
        auto done = res->getProp("done");
        if(!done || done->type != Value::Bool)
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

Value* Value::inheritProp(const string& key){
    if(!proto)return nullptr;
    if(proto->isPropTrue("__class")){
        Value* super = getOwn("super");
        if(!super)return nullptr;
        return super->getProp(key);
    }
    return proto->getProp(key);
}
Value* Value::getProp(const string& key){
    auto own = getOwn(key);
    if(own)return own;
    return inheritProp(key);
}
Function* Value::getMetaMethod(const string& key){
    if(!proto)return nullptr;
    auto meth = proto->getProp(key);
    if(meth && meth->type==Func)
        return (Function*)meth;
    return nullptr;
}
Boolean* Value::hasItem(Value* key){
    auto fn = getMetaMethod("hasItem");
    if(fn)return fn->call({this, key})->toJuaBool();
    throw new JuaTypeError("Object is not subscriptable");
}
Value* Value::getItem(Value* key){
    auto fn = getMetaMethod("getItem");
    if(fn)return fn->call({this, key});
    throw new JuaTypeError("Object is not subscriptable");
}
void Value::setItem(Value* key, Value* val){
    auto fn = getMetaMethod("setItem");
    if(!fn)throw new JuaTypeError("Object is not subscriptable");
    fn->call({this, key, val});
}
Value* Value::call(jualist& args){
    auto fn = getMetaMethod("__call");
    if(!fn)throw new JuaTypeError("Object is not callable");
    args.push_front(this);
    return fn->call(args);
    
}
Value* Value::unm(){
    Function* fn = getMetaMethod("__unm");
    if(fn)return fn->call({this});
    throw new JuaTypeError("Object is not unary negatable");
}
Value* Value::add(Value* val){
    Function* fn = getMetaMethod("__add");
    if(fn)return fn->call({this, val});
    throw new JuaTypeError("Object is not addable");
}
Value* Value::sub(Value* val){
    Function* fn = getMetaMethod("__sub");
    if(fn)return fn->call({this, val});
    throw new JuaTypeError("Object is not subtractable");
}
Value* Value::mul(Value* val){
    Function* fn = getMetaMethod("__mul");
    if(fn)return fn->call({this, val});
    throw new JuaTypeError("Object is not multiplicable");
}
Value* Value::div(Value* val){
    Function* fn = getMetaMethod("__div");
    if(fn)return fn->call({this, val});
    throw new JuaTypeError("Object is not dividable");
}
Boolean* Value::eq(Value* val){
    return Boolean::getInst(*this == val);
}
Boolean* Value::lt(Value* val){
    Function* fn = getMetaMethod("__lt");
    if(fn)return fn->call({this, val})->toJuaBool();
    throw new JuaTypeError("Object is not comparable");
}
Boolean* Value::le(Value* val){
    Function* fn = getMetaMethod("__le");
    if(fn)return fn->call({this, val})->toJuaBool();
    throw new JuaTypeError("Object is not comparable");
}
Value* Value::range(Value* val){
    Function* fn = getMetaMethod("range");
    if(fn)return fn->call({this, val});
    throw new JuaTypeError("Object is not rangeable");
}
bool Value::operator==(Value* val){
    Function* fn = getMetaMethod("__eq");
    if(fn)return fn->call({this, val})->toBoolean();
    return this == val;
}
Boolean* Value::toJuaBool(){
    return Boolean::getInst(toBoolean());
}
int64_t Value::toInt(){
    throw new JuaTypeError("toInt() called on a non-number value");
}
double Value::toNumber(){
    throw new JuaTypeError("toNumber() called on a non-number value");
}
JuaIterator* Value::getIterator(Function* next){
    auto nextFn = getMetaMethod("next");
    if(!nextFn){
        if(!next)
            throw new JuaTypeError("Object is not iterable");
        nextFn = next;
    }
    return new CustomIterator(this, nextFn);
}
void Value::collectItems(jualist& list){
    auto it = getIterator();
    Value* val;
    while((val = it->next()) != nullptr){
        list.push_back(val);
    }
    delete it;
}

void Value::gc(){
    //todo
    if(!ref){
        //d_log("gc()");
        //delete this;
    }
}

Object::Object(VM* vm_, Object* p): Value(vm_, Obj, p){ if(p)p->addRef(); }
Object::~Object(){ if(proto)proto->release(); }
bool Object::hasOwn(Value* key){
    if(key->type != Str)throw new JuaError("non-string key");
    return dict.contains(key->toString());
}
void Object::setProp(const string& key, Value* val){
    if(dict.contains(key)){
        dict[key]->release();
    }
    dict[key] = val;
    val->addRef();
}
void Object::delProp(const string& key){
    if(dict.contains(key)){
        dict[key]->release();
        dict.erase(key);
    }
}
Boolean* Object::hasItem(Value* key){
    auto fn = getMetaMethod("hasItem");
    if(fn)return fn->call({this, key})->toJuaBool();
    if(key->type!=Str)
        throw new JuaTypeError("Object.hasItem: key must be a string");
    return Boolean::getInst(getProp(key->toString()));
}
Value* Object::getItem(Value* key){
    auto fn = getMetaMethod("getItem");
    if(fn)return fn->call({this, key});
    if(key->type!=Str)
        throw new JuaTypeError("Object.getItem: key must be a string");
    Value* val = getProp(key->toString());
    return val ? val : Null::getInst();
}
void Object::setItem(Value* key, Value* val){
    auto fn = getMetaMethod("setItem");
    if(fn){
        fn->call({this, key, val});
        return;
    }
    if(key->type!=Str)
        throw new JuaTypeError("Object.setItem: key must be a string");
    setProp(key->toString(), val);
}
void Object::assignProps(Object* obj){
    for(auto& pair : obj->dict){
        dict[pair.first] = pair.second;
    }
}
string Object::toString(){
    auto fn = getMetaMethod("toString");
    if(fn)return fn->call({this})->toString();
    return safeToString();
}
string Object::safeToString(){
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
bool Object::isPropTrue(const char* key){
    return getProp(key) == Boolean::getInst(true);
}

Number::Number(VM* vm, double v): Value(vm, Num, vm->NumberProto), value(v){}
Value* Number::unm(){
    return new Number(vm, -value);
}
Value* Number::add(Value* val){
    if(val->type!=Num)throw new JuaTypeError("try to add non-number");
    auto num = static_cast<Number*>(val);
    return new Number(vm, value + num->value);
}
Value* Number::sub(Value* val){
    if(val->type!=Num)throw new JuaTypeError("try to sub non-number");
    auto num = static_cast<Number*>(val);
    return new Number(vm, value - num->value);
}
Value* Number::mul(Value* val){
    if(val->type!=Num)throw new JuaTypeError("try to mul non-number");
    auto num = static_cast<Number*>(val);
    return new Number(vm, value * num->value);
}
Value* Number::div(Value* val){
    if(val->type!=Num)throw new JuaTypeError("try to div non-number");
    auto num = static_cast<Number*>(val);
    return new Number(vm, value / num->value);
}
Boolean* Number::lt(Value* val){
    if(val->type!=Num)throw new JuaTypeError("try to compare non-number");
    auto num = static_cast<Number*>(val);
    return Boolean::getInst(value < num->value);
}
Boolean* Number::le(Value* val){
    if(val->type!=Num)throw new JuaTypeError("try to compare non-number");
    auto num = static_cast<Number*>(val);
    return Boolean::getInst(value <= num->value);
}
Value* Number::range(Value* val){
    if(val->type!=Num)throw new JuaTypeError("try to create range with non-number");
    auto num = static_cast<Number*>(val);
    auto range = new Object(vm, vm->RangeProto);
    range->setProp("start", new Number(vm, value));
    range->setProp("step", new Number(vm, 1));
    range->setProp("end", new Number(vm, num->value));
    return range;
}
bool Number::operator==(Value* val){
    if(!val || val->type!=Num)
        return false;
    return value == static_cast<Number*>(val)->value;
}
string Number::toString(){
    char s[32];
    auto res = std::to_chars(s, s+32, value);
    return {s, res.ptr};
}

size_t correctIndex(Value* num, size_t len){
    return num->toInt() % len;
}

String::String(VM* vm, const string& v): Value(vm, Str, vm->StringProto), value(v){}
Boolean* String::hasItem(Value* val){
    if(val->type!=Str)return Boolean::getInst(false);
    return Boolean::getInst(value.find(val->toString()) != string::npos);
}
Value* String::getItem(Value* key){
    size_t i = correctIndex(key, value.length());
    return new String(vm, {value[i]});
}
Value* String::add(Value* val){
    if(val->type!=Str)throw new JuaTypeError("try to add non-string");
    auto str = static_cast<String*>(val);
    return new String(vm, value + str->value);
}
bool String::operator==(Value* val){
    if(!val || val->type!=Str)
        return false;
    return value == static_cast<String*>(val)->value;
}

Value* Scope::inheritProp(const string& key){
    if(parent)return parent->getProp(key); //指针可能已经失效？？？
    return nullptr;
}

Array::Array(VM* vm, const jualist& list): Object(vm, vm->ArrayProto), items(list){}
Value* Array::getItem(Value* key){
    size_t i = correctIndex(key, items.size());
    return items[i];
}
void Array::setItem(Value* key, Value* val){
    size_t i = correctIndex(key, items.size());
    items[i] = val;
}
JuaIterator* Array::getIterator(Function* next){
    return new ListIterator(items);
}

Buffer::Buffer(VM* vm, size_t len):Object(vm, vm->BufferProto), length(len){
    bytes = new uint8_t[len];
}
Buffer::Buffer(VM* vm, uint8_t* data, size_t len, bool shared):
    Object(vm, vm->BufferProto), bytes(data), length(len), shared(shared){}
Value* Buffer::getItem(Value* key){
    size_t i = correctIndex(key, length);
    return new Number(vm, bytes[i]);
}
void Buffer::setItem(Value* key, Value* val){
    size_t i = correctIndex(key, length);
    bytes[i] = val->toInt();
}
Value* Buffer::read(Value* _start, Value* _end){
    size_t start = _start ? correctIndex(_start, length) : 0;
    size_t end = _end ? correctIndex(_end, length) : length;
    if(start>=end)throw "range error";
    size_t len = end-start;
    auto str = new String(vm);
    str->value.resize(len);
    memcpy(&str->value[0], bytes+start, len);
    return str;
}
void Buffer::write(Value* _str, Value* _pos){
    if(!_str || _str->type!=Str)
        throw new JuaTypeError("value must be a string");
    std::string& str = static_cast<String*>(_str)->value;
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

