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
using std::string;
using std::initializer_list;

struct JuaVM;
struct Jua_Val;
struct Jua_Obj;
struct Jua_Bool;
struct Jua_Func;
typedef std::deque<Jua_Val*> jualist;

void d_log(const string&);
void d_log(char c);
void d_log(Jua_Val*);
void d_log(string, Jua_Val*);
void d_log(string, int);

struct Jua_Val{
    JuaVM* vm = nullptr; //指向JuaVM实例
    enum JuaType{Obj, Null, Str, Num, Bool, Func};
    JuaType type;
    size_t id = -1;
    Jua_Obj* proto;
    size_t ref = 0;
    Jua_Val(JuaVM* vm_, JuaType t, Jua_Obj* p = nullptr): vm(vm_), type(t), proto(p){}
    void addRef(){ ref++; }
    void release();
    virtual bool isType(int type_id){
        //用于自定义类型判断（自定义类型应当是 Jua_Obj 的子类）
        //保留的 type_id:
        // 0: 未使用
        // 1: Scope
        // 2: Jua_Array
        // 3: Jua_Buffer
        // 4-15: 未使用
        return false;
    }
    virtual Jua_Val* getOwn(const string& key){
        return nullptr;
    }
    Jua_Val* getOwn(const char* key){
        string str(key);
        return getOwn(str);
    }
    virtual Jua_Val* inheritProp(const string&); //返回jua值或nullptr
    Jua_Val* getProp(const string&);
    Jua_Val* getProp(const char* key){
        string str(key);
        return getProp(str);
    }
    Jua_Func* getMetaMethod(const string&);
    Jua_Func* getMetaMethod(const char* key){
        string str(key);
        return getMetaMethod(str);
    }
    //以下均不会返回 nullptr
    virtual Jua_Bool* hasItem(Jua_Val*);
    virtual Jua_Val* getItem(Jua_Val*);
    virtual void setItem(Jua_Val*, Jua_Val*);
    virtual Jua_Val* call(jualist&);
    Jua_Val* call(initializer_list<Jua_Val*> args){
        jualist list = args;
        return call(list);
    }
    virtual Jua_Val* unm();
    virtual Jua_Val* add(Jua_Val*);
    virtual Jua_Val* sub(Jua_Val*);
    virtual Jua_Val* mul(Jua_Val*);
    virtual Jua_Val* div(Jua_Val*);
    Jua_Bool* eq(Jua_Val*); //子类应当重写 operator==
    virtual Jua_Bool* lt(Jua_Val*);
    virtual Jua_Bool* le(Jua_Val*);
    virtual Jua_Val* range(Jua_Val*);

    virtual bool operator==(Jua_Val* val);
    virtual string toString() = 0; //const string& str = toString()
    virtual string safeToString(){ return toString(); }
    virtual bool toBoolean(){ return true; }
    virtual Jua_Bool* toJuaBool();
    virtual int64_t toInt(); //仅用于数字
    virtual double toNumber(); //仅用于数字
    virtual string getTypeName() = 0;
    virtual void gc();
};

struct Jua_Null: Jua_Val{
    string toString(){ return "null"; }
    bool toBoolean(){ return false; }
    string getTypeName(){ return "null"; }
    void gc(){}
    static Jua_Null* getInst(){
        static Jua_Null* inst = new Jua_Null;
        return inst;
    }
    private:
    Jua_Null(): Jua_Val(nullptr, Null){}
};
struct Jua_Obj: Jua_Val{
    std::unordered_map<string, Jua_Val*> dict;
    Jua_Obj(JuaVM* vm_, Jua_Obj* p = nullptr);
    ~Jua_Obj();
    bool hasOwn(Jua_Val* key);
    Jua_Val* getOwn(size_t hash, const string& key); //todo
    Jua_Val* getOwn(const string& key){
        if(dict.contains(key))return dict[key];
        return nullptr;
    }
    Jua_Val* getOwn(const char* key){
        string str(key);
        return getOwn(str);
    }
    void setProp(const string& key, Jua_Val* val);
    void delProp(const string& key);
    Jua_Bool* hasItem(Jua_Val* key);
    Jua_Val* getItem(Jua_Val* key);
    void setItem(Jua_Val* key, Jua_Val* val);
    bool isPropTrue(const char* key);
    void assignProps(Jua_Obj* obj);
    string toString();
    string safeToString(); //不调用元方法
    string getTypeName(){ return "object"; }
};
struct Jua_Bool: Jua_Val{
    bool value;
    string toString(){
        return value ? "true" : "false";
    }
    bool toBoolean(){ return value; }
    string getTypeName(){ return "boolean"; }
    void gc(){}
    static Jua_Bool* getInst(bool v){
        static Jua_Bool* t = new Jua_Bool(true);
        static Jua_Bool* f = new Jua_Bool(false);
        return v ? t : f;
    }
    private:
    Jua_Bool(bool v): Jua_Val(nullptr, Bool), value(v){}
};
struct Jua_Num: Jua_Val{
    double value;
    Jua_Num(JuaVM* vm_, double v): Jua_Val(vm_, Num), value(v){}
    Jua_Val* unm();
    Jua_Val* add(Jua_Val*);
    Jua_Val* sub(Jua_Val*);
    Jua_Val* mul(Jua_Val*);
    Jua_Val* div(Jua_Val*);
    Jua_Bool* lt(Jua_Val*);
    Jua_Bool* le(Jua_Val*);
    Jua_Val* range(Jua_Val*);
    bool operator==(Jua_Val* val);
    string toString();
    bool toBoolean(){ return value; }
    int64_t toInt(){ return std::round(value); }
    double toNumber(){ return value; }
    string getTypeName(){ return "number"; }
};
struct Jua_Str: Jua_Val{
    string value;
    Jua_Str(JuaVM* vm_, const string& v = "");
    Jua_Bool* hasItem(Jua_Val*);
    Jua_Val* getItem(Jua_Val*);
    Jua_Val* add(Jua_Val* val);
    bool operator==(Jua_Val *);
    string toString(){ return value; }
    bool toBoolean(){ return value.size(); }
    string getTypeName(){ return "string"; }
};
struct Jua_Func: Jua_Val{
    Jua_Func(JuaVM* vm_): Jua_Val(vm_, Func){}
    string toString(){ return "<function>"; }
    string getTypeName(){ return "function"; }
};

struct Scope: Jua_Obj{
    static const int type_id = 1;
    Scope* parent;
    Scope(JuaVM* vm_, Scope* p=nullptr): Jua_Obj(vm_, p), parent(p){ if(p)p->addRef(); }
    Scope(Scope* p): Scope(p->vm, p){} //从父作用域创建新作用域
    ~Scope(){
        if(parent)parent->release();
    }
    bool isType(int type_id) override {
        return type_id == Scope::type_id;
    }
    Jua_Val* inheritProp(const string&) override;
    void assign(const string& key, Jua_Val* val){
        for(auto scope = this; scope; scope = scope->parent){
            if(scope->dict.contains(key)){
                scope->setProp(key, val);
                return;
            }
        }
        throw string("Variable not found: ") + key;
    }
};
struct Jua_NativeFunc: Jua_Func{
    typedef std::function<Jua_Val*(jualist&)> Native; //可返回 nullptr
    Native native;
    Jua_NativeFunc(JuaVM* vm_, Native fn): Jua_Func(vm_), native(fn){}
    Jua_Val* call(jualist& args){
        auto res = native(args);
        if(res)return res;
        return Jua_Null::getInst();
    }
};
struct Jua_Array: Jua_Obj{
    static const int type_id = 2;
    jualist items;
    Jua_Array(JuaVM*, const jualist&);
    bool isType(int type_id) override {
        return type_id == Jua_Array::type_id;
    }
    Jua_Val* getItem(Jua_Val*);
    void setItem(Jua_Val*, Jua_Val*);
};
struct Jua_Buffer: Jua_Obj{
    static const int type_id = 3;
    uint8_t* bytes;
    size_t length;
    Jua_Buffer(JuaVM*, size_t);
    ~Jua_Buffer(){ delete bytes; }
    bool isType(int type_id) override {
        return type_id == Jua_Buffer::type_id;
    }
    Jua_Val* getItem(Jua_Val*);
    void setItem(Jua_Val*, Jua_Val*);
    Jua_Val* read(Jua_Val* start, Jua_Val* end);
    void write(Jua_Val* str, Jua_Val* pos);
};

struct JuaError{
    string message;
    JuaError(string msg): message(msg){}
    JuaError(): JuaError("Unknown JuaError"){}
    virtual string toDebugString(){ return "JuaError"; }
};
struct JuaErrorWithVal: JuaError{
    Jua_Val* val;
    JuaErrorWithVal(string msg, Jua_Val* v): JuaError(msg), val(v){}
    string toDebugString(){
        string str = message;
        str.append("\n\twith value: ");
        str.append(val ? val->safeToString() : "nullptr");
        return str;
    }
};
struct JuaTypeError: JuaError{
    JuaTypeError(string msg): JuaError(msg){}
    string toDebugString(){ return "JuaTypeError: " + message; }
};