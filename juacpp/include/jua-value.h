#pragma once
#define JUA_DEBUG
#include <string>
#include <deque>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <cstddef>
#include <cmath>
#include <functional>
static_assert(
    sizeof(char)==1 &&
    sizeof(float)==4 &&
    sizeof(double)==8,
    "Incapable platform"
);
namespace Jua {

struct VM;
struct Value;
struct Object;
struct Boolean;
struct Function;
typedef std::deque<Value*> jualist;
struct JuaIterator;

void d_log(const std::string&);
void d_log(char c);
void d_log(Value*);
void d_log(std::string, Value*);
void d_log(std::string, int);

struct Value{
    VM* vm = nullptr; //指向JuaVM实例
    enum JuaType{Obj, Null, Str, Num, Bool, Func};
    JuaType type;
    size_t id = -1;
    Object* proto;
    Value(VM* vm_, JuaType t, Object* p = nullptr): vm(vm_), type(t), proto(p){}
    virtual bool isType(int type_id){
        //用于自定义类型判断（自定义类型应当是 Object 的子类）
        //保留的 type_id:
        // 0: 未使用
        // 1: Scope
        // 2: Array
        // 3: Buffer
        // 4: Coroutine
        // 5-15: 未使用
        return false;
    }
    virtual Value* getOwn(const std::string& key){
        return nullptr;
    }
    Value* getOwn(const char* key){
        std::string str(key);
        return getOwn(str);
    }
    virtual Value* inheritProp(const std::string&); //返回jua值或nullptr
    Value* getProp(const std::string&);
    Value* getProp(const char* key){
        std::string str(key);
        return getProp(str);
    }
    Function* getMetaMethod(const std::string&);
    Function* getMetaMethod(const char* key){
        std::string str(key);
        return getMetaMethod(str);
    }
    //以下均不会返回 nullptr
    virtual Boolean* hasItem(Value*);
    virtual Value* getItem(Value*);
    virtual void setItem(Value*, Value*);
    virtual Value* call(jualist&);
    Value* call(std::initializer_list<Value*> args){
        jualist list = args;
        return call(list);
    }
    virtual Value* unm();
    virtual Value* add(Value*);
    virtual Value* sub(Value*);
    virtual Value* mul(Value*);
    virtual Value* div(Value*);
    Boolean* eq(Value*); //子类应当重写 operator==
    virtual Boolean* lt(Value*);
    virtual Boolean* le(Value*);
    virtual Value* range(Value*);

    virtual JuaIterator* getIterator(Function* next=nullptr); //优先使用元方法 "next"
    virtual void collectItems(jualist&); //仅用于可迭代对象
    virtual bool operator==(Value* val);
    virtual std::string toString() = 0; //const std::string& str = toString()
    virtual std::string safeToString(){ return toString(); }
    virtual bool toBoolean(){ return true; }
    virtual Boolean* toJuaBool();
    virtual int64_t toInt(); //仅用于数字
    virtual double toNumber(); //仅用于数字
    virtual std::string getTypeName() = 0;
    virtual void gc();
};

struct Null: Value{
    std::string toString(){ return "null"; }
    bool toBoolean(){ return false; }
    std::string getTypeName(){ return "null"; }
    void gc(){}
    static Null* getInst(){
        static Null* inst = new Null;
        return inst;
    }
    private:
    Null(): Value(nullptr, Value::Null){}
};
struct Object: Value{
    std::unordered_map<std::string, Value*> dict;
    Object(VM* vm_, Object* p = nullptr);
    ~Object();
    bool hasOwn(Value* key);
    Value* getOwn(size_t hash, const std::string& key); //todo
    Value* getOwn(const std::string& key){
        if(dict.contains(key))return dict[key];
        return nullptr;
    }
    Value* getOwn(const char* key){
        std::string str(key);
        return getOwn(str);
    }
    void setProp(const std::string& key, Value* val);
    void delProp(const std::string& key);
    Boolean* hasItem(Value* key);
    Value* getItem(Value* key);
    void setItem(Value* key, Value* val);
    bool isPropTrue(const char* key);
    void assignProps(Object* obj);
    std::string toString();
    std::string safeToString(); //不调用元方法
    std::string getTypeName(){ return "object"; }
};
struct Boolean: Value{
    bool value;
    std::string toString(){
        return value ? "true" : "false";
    }
    bool toBoolean(){ return value; }
    std::string getTypeName(){ return "boolean"; }
    void gc(){}
    static Boolean* getInst(bool v){
        static Boolean* t = new Boolean(true);
        static Boolean* f = new Boolean(false);
        return v ? t : f;
    }
    private:
    Boolean(bool v): Value(nullptr, Bool), value(v){}
};
struct Number: Value{
    double value;
    Number(VM* vm, double v);
    Value* unm();
    Value* add(Value*);
    Value* sub(Value*);
    Value* mul(Value*);
    Value* div(Value*);
    Boolean* lt(Value*);
    Boolean* le(Value*);
    Value* range(Value*);
    bool operator==(Value* val);
    std::string toString();
    bool toBoolean(){ return value; }
    int64_t toInt(){ return std::round(value); }
    double toNumber(){ return value; }
    std::string getTypeName(){ return "number"; }
};
struct String: Value{
    std::string value;
    String(VM* vm_, const std::string& v = "");
    Boolean* hasItem(Value*);
    Value* getItem(Value*);
    Value* add(Value* val);
    bool operator==(Value *);
    std::string toString(){ return value; }
    bool toBoolean(){ return value.size(); }
    std::string getTypeName(){ return "std::string"; }
};
struct Function: Value{
    Function(VM* vm_): Value(vm_, Func){}
    std::string toString(){ return "<function>"; }
    std::string getTypeName(){ return "function"; }
};

struct Scope: Object{
    static const int type_id = 1;
    Scope* parent;
    Scope(VM* vm_, Scope* p=nullptr): Object(vm_, p), parent(p){}
    Scope(Scope* p): Scope(p->vm, p){} //从父作用域创建新作用域
    bool isType(int type_id) override {
        return type_id == Scope::type_id;
    }
    Value* inheritProp(const std::string&) override;
    void assign(const std::string& key, Value* val){
        for(auto scope = this; scope; scope = scope->parent){
            if(scope->dict.contains(key)){
                scope->setProp(key, val);
                return;
            }
        }
        throw std::string("Variable not found: ") + key;
    }
};
struct NativeFunction: Function{
    typedef std::function<Value*(jualist&)> Native; //可返回 nullptr
    Native native;
    NativeFunction(VM* vm_, Native fn): Function(vm_), native(fn){}
    Value* call(jualist& args){
        auto res = native(args);
        if(res)return res;
        return Null::getInst();
    }
};
struct Array: Object{
    static const int type_id = 2;
    jualist items;
    Array(VM*, const jualist&);
    bool isType(int type_id) override {
        return type_id == Array::type_id;
    }
    Value* getItem(Value*);
    void setItem(Value*, Value*);
    JuaIterator* getIterator(Function* next=nullptr) override;
    void collectItems(jualist& list) override {
        for(auto& item : items){
            list.push_back(item);
        }
    }
};
struct Buffer: Object{
    static const int type_id = 3;
    uint8_t* bytes;
    size_t length;
    bool shared = false;
    Buffer(VM*, uint8_t*, size_t, bool shared=true);
    Buffer(VM*, size_t);
    ~Buffer(){
        if(!shared) delete bytes;
    }
    bool isType(int type_id) override {
        return type_id == Buffer::type_id;
    }
    Value* getItem(Value*);
    void setItem(Value*, Value*);
    Value* read(Value* start=nullptr, Value* end=nullptr);
    void write(Value* str, Value* pos=nullptr);
};
struct Coroutine: Object{ //抽象类
    static const int type_id = 4;
    Coroutine(VM* vm_): Object(vm_){}
    bool isType(int type_id) override {
        return type_id == Coroutine::type_id;
    }
    virtual Value* resume(jualist&) = 0;
    std::string toString(){
        return "<coroutine>";
    }
};

struct JuaIterator{
    virtual Value* next() = 0; //迭代完成时返回 nullptr
};
struct JuaError{
    std::string message;
    JuaError(std::string msg): message(msg){}
    JuaError(): JuaError("Unknown JuaError"){}
    virtual std::string toDebugString();
};
struct JuaErrorWithVal: JuaError{
    Value* val;
    JuaErrorWithVal(std::string msg, Value* v): JuaError(msg), val(v){}
    std::string toDebugString(){
        std::string str = message;
        str.append("\n\twith value: ");
        str.append(val ? val->safeToString() : "nullptr");
        return str;
    }
};
struct JuaTypeError: JuaError{
    JuaTypeError(std::string msg): JuaError(msg){}
    std::string toDebugString(){ return "JuaTypeError: " + message; }
};

}