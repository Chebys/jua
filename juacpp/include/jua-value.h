#pragma once
// 定义 JUA_DEBUG 后 d_log 会向 stdout 打印调试信息（默认关闭）
//#define JUA_DEBUG
#include <string>
#include <deque>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <cstddef>
#include <cmath>
#include <functional>
#include <utility>
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

// 值。所有值都由 new 分配，由 VM 的标记-清除回收（见 gc.cpp）。
// 不参与回收的值（Null / Boolean 单例）的 vm 为 nullptr。
struct Value{
    VM* vm = nullptr; //指向JuaVM实例
    enum JuaType{Obj, Null, Str, Num, Bool, Func};
    JuaType type;
    size_t id = (size_t)-1;
    Object* proto;
    bool marked = false; //GC 标记位
    Value(VM* vm_, JuaType t, Object* p = nullptr);
    virtual ~Value() = default;
    virtual bool isType(int){
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
    //把本值持有的所有强引用交给 GC。只在标记阶段调用。
    virtual void trace(VM*){}
    virtual Value* getOwn(const std::string&){
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
    virtual Value* mod(Value*); // %
    virtual Value* pow(Value*); // ^
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
    virtual bool isInst(Value* klass); //用于 is 运算符：判断原型/父类链
    virtual std::string toString() = 0; //const std::string& str = toString()
    virtual std::string safeToString(){ return toString(); }
    virtual bool toBoolean(){ return true; }
    virtual Boolean* toJuaBool();
    virtual int64_t toInt(); //仅用于数字
    virtual double toNumber(); //仅用于数字
    virtual std::string getTypeName() = 0;
};

struct Null: Value{
    std::string toString(){ return "null"; }
    bool toBoolean(){ return false; }
    std::string getTypeName(){ return "null"; }
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
    void trace(VM* vm) override;
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
    Value* mod(Value*);
    Value* pow(Value*);
    Boolean* lt(Value*);
    Boolean* le(Value*);
    Value* range(Value*);
    bool operator==(Value* val);
    std::string toString();
    bool toBoolean(){ return value; }
    int64_t toInt(){ return std::llround(value); }
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
    std::string getTypeName(){ return "string"; }
};
struct Function: Value{
    Function(VM* vm_);
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
    void trace(VM* vm) override;
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
    //原生函数捕获的 jua 值。GC 必须能看到它们，否则 obj:meth 逃逸后接收者会被回收。
    std::vector<Value*> captures;
    NativeFunction(VM* vm_, Native fn): Function(vm_), native(fn){}
    NativeFunction(VM* vm_, Native fn, std::vector<Value*> caps):
        Function(vm_), native(fn), captures(std::move(caps)){}
    Value* call(jualist& args); //在 value.cpp 中实现：需要把实参登记为 GC 根
    void trace(VM* vm) override;
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
    void trace(VM* vm) override;
};
struct Buffer: Object{
    static const int type_id = 3;
    uint8_t* bytes;
    size_t length;
    bool owned = false; //是否拥有 bytes 的内存
    Buffer(VM*, uint8_t*, size_t, bool owned=false);
    Buffer(VM*, size_t);
    ~Buffer(){
        if(owned && bytes) delete[] bytes;
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
    virtual ~JuaIterator() = default;
    virtual Value* next() = 0; //迭代完成时返回 nullptr
    //把自己当前持有的值登记进 GC 根集合（在 next() 之前调用）
    virtual void rootValues(VM*){} //把自己当前持有的值压入 VM::gcRoots
};
// Jua 错误。所有抛出点都必须抛指针（JuaError*）：宿主与 try() 只 catch 指针，
// 按值抛出会落到 catch(...) 并丢失消息（历史 bug，见 改进清单 附C #12）。
struct JuaError{
    enum class Kind{Error, Client, Syntax, Type, Reference, Range};
    std::string message;
    Kind kind = Kind::Error;
    JuaError(std::string msg, Kind k = Kind::Error): message(std::move(msg)), kind(k){}
    JuaError(): JuaError("Unknown JuaError"){}
    virtual ~JuaError() = default;
    virtual std::string toDebugString();
    static const char* kindName(Kind k){
        switch(k){
            case Kind::Client:    return "client";
            case Kind::Syntax:    return "syntax";
            case Kind::Type:      return "type";
            case Kind::Reference: return "reference";
            case Kind::Range:     return "range";
            default:              return "error";
        }
    }
    const char* kindName() const { return kindName(kind); }
};
struct JuaErrorWithVal: JuaError{
    Value* val;
    JuaErrorWithVal(std::string msg, Value* v): JuaError(msg, Kind::Reference), val(v){}
    std::string toDebugString(){
        std::string str = message;
        str.append("\n\twith value: ");
        str.append(val ? val->safeToString() : "nullptr");
        return str;
    }
};
struct JuaTypeError: JuaError{
    JuaTypeError(std::string msg): JuaError(msg, Kind::Type){}
    std::string toDebugString(){ return "JuaTypeError: " + message; }
};
struct JuaRangeError: JuaError{
    JuaRangeError(std::string msg): JuaError(msg, Kind::Range){}
    std::string toDebugString(){ return "JuaRangeError: " + message; }
};
struct JuaRefError: JuaError{
    JuaRefError(std::string msg): JuaError(msg, Kind::Reference){}
    std::string toDebugString(){ return "JuaRefError: " + message; }
};

}

// 本仓库的各实现文件假定这些名字在全局作用域可见。
using namespace Jua;
using std::string;
