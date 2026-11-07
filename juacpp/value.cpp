#include "jua-value.h"
#include <charconv>
#include <format>
#include <cstring>
#include <algorithm>
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
    // list 属于某个 Array，由调用者（ForStmt）负责把该 Array 登记为根
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
    void rootValues(VM* vm){
        vm->gcRoots.push_back(obj);
        vm->gcRoots.push_back(key);
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
        // key 是可选的（见 draft/抽象操作.md 的 {done, value?, key?}）。
        // 有状态的迭代器可以自己保存进度，此时不必返回 key。
        auto k = res->getProp("key");
        if(k)key = k;
        return value;
    }
};

// ---- 构造与 GC 钩子 ----

Value::Value(VM* vm_, JuaType t, Object* p): vm(vm_), type(t), proto(p){
    if(vm)vm->registerValue(this);
}
Function::Function(VM* vm_): Value(vm_, Func, vm_->FunctionProto){}

void Object::trace(VM* vm){
    vm->markFrom(proto);
    for(auto& pair: dict)vm->markFrom(pair.second);
}
void Scope::trace(VM* vm){
    // parent 与 proto 指向同一个父作用域，markFrom 幂等，无需去重
    vm->markFrom(parent);
    vm->markFrom(proto);
    for(auto& pair: dict)vm->markFrom(pair.second);
}
void Array::trace(VM* vm){
    vm->markFrom(proto);
    for(auto& item: items)vm->markFrom(item);
}
void NativeFunction::trace(VM* vm){
    vm->markFrom(proto);
    for(auto v: captures)vm->markFrom(v);
}
Value* NativeFunction::call(jualist& args){
    //实参表只存在于 C++ 栈上，原生函数体里可能间接调到 jua 函数并触发回收
    RootGuard guard(vm);
    for(auto a: args)vm->gcRoots.push_back(a);
    auto res = native(args);
    if(res)return res;
    return Null::getInst();
}

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
Value* Value::mod(Value* val){
    Function* fn = getMetaMethod("__mod");
    if(fn)return fn->call({this, val});
    throw new JuaTypeError("Object is not modulo-able");
}
Value* Value::pow(Value* val){
    Function* fn = getMetaMethod("__pow");
    if(fn)return fn->call({this, val});
    throw new JuaTypeError("Object is not exponentiable");
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
    Function* fn = getMetaMethod("__range");
    if(fn)return fn->call({this, val});
    throw new JuaTypeError("Object is not rangeable");
}
bool Value::operator==(Value* val){
    Function* fn = getMetaMethod("__eq");
    if(fn)return fn->call({this, val})->toBoolean();
    return this == val;
}
bool Value::isInst(Value* klass){
    //自身原型就是该类，或沿 super 链能找到该类
    for(Object* p = proto; p; ){
        if(p == klass)return true;
        auto super = p->getOwn("super");
        if(!super || super->type != Value::Obj)return false;
        p = static_cast<Object*>(super);
    }
    return false;
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

Object::Object(VM* vm_, Object* p): Value(vm_, Obj, p){}
bool Object::hasOwn(Value* key){
    if(key->type != Str)throw new JuaError("non-string key");
    return dict.contains(key->toString());
}
void Object::setProp(const string& key, Value* val){
    dict[key] = val;
}
void Object::delProp(const string& key){
    dict.erase(key);
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
    //规范要求“严格属性访问”，即只看自身属性
    return getOwn(key) == Boolean::getInst(true);
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
Value* Number::mod(Value* val){
    if(val->type!=Num)throw new JuaTypeError("try to mod non-number");
    auto num = static_cast<Number*>(val);
    return new Number(vm, std::fmod(value, num->value));
}
Value* Number::pow(Value* val){
    if(val->type!=Num)throw new JuaTypeError("try to pow non-number");
    auto num = static_cast<Number*>(val);
    return new Number(vm, std::pow(value, num->value));
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
    char s[64];
    auto res = std::to_chars(s, s+64, value);
    return {s, res.ptr};
}

size_t correctIndex(Value* num, size_t len){
    if(!len)throw new JuaRangeError("index on an empty value");
    int64_t i = num->toInt() % (int64_t)len;
    if(i < 0)i += (int64_t)len;
    return (size_t)i;
}

String::String(VM* vm, const string& v): Value(vm, Str, vm->StringProto), value(v){}
Boolean* String::hasItem(Value* val){
    if(val->type!=Str)return Boolean::getInst(false);
    return Boolean::getInst(value.find(val->toString()) != string::npos);
}
Value* String::getItem(Value* key){
    size_t i = correctIndex(key, value.length());
    return new String(vm, string(1, value[i]));
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
    if(parent)return parent->getProp(key); //父作用域由 trace() 保活
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
JuaIterator* Array::getIterator(Function*){
    return new ListIterator(items);
}

Buffer::Buffer(VM* vm, size_t len):Object(vm, vm->BufferProto), length(len), owned(true){
    bytes = new uint8_t[len ? len : 1]();
    setProp("length", new Number(vm, (double)len));
}
Buffer::Buffer(VM* vm, uint8_t* data, size_t len, bool owned):
    Object(vm, vm->BufferProto), bytes(data), length(len), owned(owned){
    setProp("length", new Number(vm, (double)len));
}
Value* Buffer::getItem(Value* key){
    size_t i = correctIndex(key, length);
    return new Number(vm, bytes[i]);
}
void Buffer::setItem(Value* key, Value* val){
    size_t i = correctIndex(key, length);
    bytes[i] = (uint8_t)val->toInt();
}
Value* Buffer::read(Value* _start, Value* _end){
    //注意：start/end 是“边界”，应当钳制而不是像下标那样取模回绕
    int64_t len = (int64_t)length;
    auto clamp = [len](Value* v, int64_t dflt) -> int64_t {
        if(!v)return dflt;
        int64_t i = v->toInt();
        if(i < 0)i += len;
        if(i < 0)i = 0;
        if(i > len)i = len;
        return i;
    };
    int64_t start = clamp(_start, 0);
    int64_t end = clamp(_end, len);
    if(start>=end)throw new JuaRangeError("Buffer.read: invalid range");
    size_t n = (size_t)(end-start);
    auto str = new String(vm);
    str->value.resize(n);
    if(n)memcpy(&str->value[0], bytes+start, n);
    return str;
}
void Buffer::write(Value* _str, Value* _pos){
    if(!_str || _str->type!=Str)
        throw new JuaTypeError("value must be a string");
    std::string& str = static_cast<String*>(_str)->value;
    int64_t pos = 0;
    if(_pos){
        pos = _pos->toInt();
        if(pos < 0)pos += (int64_t)length;
        if(pos < 0)pos = 0;
    }
    if(pos + (int64_t)str.size() > (int64_t)length)
        throw new JuaRangeError("Buffer.write: out of range");
    if(str.size())memcpy(bytes+pos, str.data(), str.size());
}

string JuaError::toDebugString(){
    if(!message.size())return "JuaError";
    return std::format("JuaError: {}", message);
}
