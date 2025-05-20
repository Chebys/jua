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
struct Jua_Val;
struct Jua_Obj;
struct Jua_Bool;
struct Jua_Func;
typedef std::deque<Jua_Val*> jualist;

void d_log(const string&);
void d_log(char c);
void d_log(Jua_Val*);
void d_log(string, Jua_Val*);

struct Jua_Val{
    enum JuaType{Obj, Null, Str, Num, Bool, Func};
    JuaType type;
    Jua_Obj* proto;
    size_t ref = 0;
    Jua_Val(JuaType t, Jua_Obj* p = nullptr): type(t), proto(p){}
    void addRef(){ ref++; }
    void release();
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
    //以下指针均非空
    virtual Jua_Bool* hasItem(Jua_Val*);
    virtual Jua_Val* getItem(Jua_Val*);
    virtual void setItem(Jua_Val*, Jua_Val*);
    virtual Jua_Val* call(jualist&);
    Jua_Val* call(initializer_list<Jua_Val*> args){
        jualist list = args;
        return call(list);
    }
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
    virtual int64_t toInt(){ //仅用于数字
        throw "type error";
    }
    virtual string getTypeName() = 0;
    virtual void gc();
};

struct Jua_Null: Jua_Val{
    Jua_Null(): Jua_Val(Null){}
    string toString(){ return "null"; }
    bool toBoolean(){ return false; }
    string getTypeName(){ return "null"; }
    void gc(){}
    static Jua_Null* getInst(){
        static Jua_Null* inst = new Jua_Null;
        return inst;
    }
};
struct Jua_Obj: Jua_Val{
    std::unordered_map<string, Jua_Val*> dict;
    Jua_Obj(Jua_Obj* p = nullptr);
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
    Jua_Bool(bool v): Jua_Val(Bool), value(v){};
    string toString(){
        return value ? "true" : "false";
    }
    bool toBoolean(){ return value; }
    string getTypeName(){ return "boolean"; }
    void gc(){}
    static Jua_Bool* inst[2];
    static Jua_Bool* getInst(bool v){
        static Jua_Bool* t = new Jua_Bool(true);
        static Jua_Bool* f = new Jua_Bool(false);
        return v ? t : f;
    }
};
struct Jua_Num: Jua_Val{
    double value;
    Jua_Num(double v): Jua_Val(Num), value(v){};
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
    string getTypeName(){ return "number"; }
};
struct Jua_Str: Jua_Val{
    string value;
    Jua_Str(const string& v): Jua_Val(Str), value(v){} //会进行拷贝
    Jua_Str(): Jua_Val(Str){}
    Jua_Bool* hasItem(Jua_Val*);
    Jua_Val* getItem(Jua_Val*);
    bool operator==(Jua_Val *);
    string toString(){ return value; }
    bool toBoolean(){ return value.size(); }
    string getTypeName(){ return "string"; }
};
struct Jua_Func: Jua_Val{
    Jua_Func(): Jua_Val(Func){};
    string toString(){ return "<function>"; }
    string getTypeName(){ return "function"; }
};

struct Scope: Jua_Obj{
    Scope* parent;
    Scope(Scope* p=nullptr): parent(p){ if(p)p->addRef(); }
    Jua_Val* inheritProp(const string&) override;
};
struct Jua_NativeFunc: Jua_Func{
    typedef std::function<Jua_Val*(jualist&)> Native; //可返回空
    Native native;
    Jua_NativeFunc(Native fn): native(fn){}
    Jua_Val* call(jualist& args){
        return native(args);
    }
};
struct Jua_Array: Jua_Obj{
    jualist items;
    Jua_Array(const jualist& list): items(list){}
    Jua_Val* getItem(Jua_Val*);
    void setItem(Jua_Val*, Jua_Val*);
};
struct Jua_Buffer: Jua_Obj{
    uint8_t* bytes;
    size_t length;
    Jua_Buffer(size_t len): length(len){
        bytes = new uint8_t[len];
    }
    ~Jua_Buffer(){ delete bytes; }
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

struct Expression{
    virtual Jua_Val* calc(Scope* env) = 0;
};
struct LeftValue{
    virtual void assign(Scope* env, Jua_Val* val) = 0;
};
struct Declarable: LeftValue{
    virtual void declare(Scope* env, Jua_Val* val) = 0;
    virtual void addDefault(){}
};
struct DeclarationItem{
    Declarable* body;
    Expression* initval;
    DeclarationItem(Declarable* d, Expression* e=nullptr): body(d), initval(e){}
    void addDefault();
    void assign(Scope* env, Jua_Val* val);
    void declare(Scope* env, Jua_Val* val);
};
struct DeclarationList{
    //static DeclarationList* fromNames()
    std::deque<DeclarationItem*> decItems;
    DeclarationList(std::deque<DeclarationItem*> items): decItems(items){}
    void assign(Scope* env, Jua_Val* val);
    void declare(Scope* env, Jua_Val* val);
    void rawDeclare(Scope* env, const jualist&);
};
struct LiteralNum: Expression{
    double value;
    LiteralNum(double v): value(v){}
    static LiteralNum* eval(const string& str);
    Jua_Val* calc(Scope*);
};
struct LiteralStr: Expression{
    string value;
    LiteralStr(string v): value(v){}
    Jua_Val* calc(Scope*);
};
struct Template: Expression{
    std::vector<string> strList;
    std::vector<Expression*> exprList;
    Template(std::vector<string>& sl, std::vector<Expression*> el): strList(sl), exprList(el){}
	Jua_Val* calc(Scope*);
};
struct Keyword: Expression{
    char type;
    Keyword(char t): type(t){};
    Jua_Val* calc(Scope*);
    static Keyword* null;
    static Keyword* t;
    static Keyword* f;
    static Keyword* local;
};
struct Varname: Expression, Declarable{
    string str;
    //size_t hash; todo
    Varname(string name): str(name){};
    Jua_Val* calc(Scope*);
    void assign(Scope* env, Jua_Val* val);
    void declare(Scope* env, Jua_Val* val);
};
struct PropRef;
struct MethWrapper;
struct UnitaryExpr;
struct BinaryExpr;
struct Assignment;
struct Subscription;
struct TernaryExpr;
struct FlexibleList{
    std::vector<Expression*> exprs;
    FlexibleList(std::vector<Expression*>& list): exprs(list){};
    FlexibleList(initializer_list<Expression*> list): exprs(list){};
    jualist calc(Scope*);
};
struct Call: Expression{
    Expression* calee;
    FlexibleList* args;
    Call(Expression* e, FlexibleList* l): calee(e), args(l){}
    Jua_Val* calc(Scope*);
};

struct ObjExpr: Expression{
    typedef std::vector<std::pair<Expression*, Expression*>> Props;
    Props entries;
	ObjExpr(Props p): entries(p){}
	Jua_Val* calc(Scope*);
};
struct ArrayExpr: Expression{
    FlexibleList* list;
    ArrayExpr(FlexibleList* exprs): list(exprs){}
	Jua_Val* calc(Scope*);
};

struct Controller{
    bool isPending = false;
    Jua_Val* retval = nullptr;
};

struct Statement{
    Statement* pending_continue = nullptr;
    Statement* pending_break = nullptr;
    virtual void exec(Scope*, Controller*) = 0;
};
struct ExprStatement: Statement{
    Expression* expr;
    ExprStatement(Expression* e): expr(e){}
    void exec(Scope* env, Controller*){
        expr->calc(env);
    }
};
struct Declaration: Statement{
    DeclarationList* list;
    Declaration(DeclarationList* l){
        for(auto item: l->decItems){
            if(!item->initval)item->initval = Keyword::null;
        }
        list = l;
    }
    void exec(Scope* env, Controller*){
        list->rawDeclare(env, {});
    }
};
struct Return: Statement{
    Expression* expr;
    Return(Expression* e=nullptr): expr(e){}
    void exec(Scope*, Controller*);
};

typedef std::vector<Statement*> Stmts;

struct Block{
    Statement* pending_continue = nullptr;
    Statement* pending_break = nullptr;
    Stmts statements;
    Block(Stmts stmts);
    void exec(Scope* env, Controller* controller);
};
struct FunctionBody: Block{
    FunctionBody(Stmts stmts): Block(stmts){
        if(pending_continue)
            throw pending_continue;
        if(pending_break)
            throw pending_break;
    }
    Jua_Val* exec(Scope*);
};

struct Jua_PFunc: Jua_Func{
    Scope* upenv;
    DeclarationList* decList;
    FunctionBody* body;
    Jua_PFunc(Scope* env, DeclarationList* list, FunctionBody* b):
        upenv(env), decList(list), body(b){}
	Jua_Val* call(jualist& args){
		auto env = new Scope(upenv);
		decList->rawDeclare(env, args);
		return body->exec(env);
	}
};

struct FunExpr: Expression{
    DeclarationList* decList;
    FunctionBody* body;
	FunExpr(DeclarationList* dl, Stmts stmts):
        decList(dl), body(new FunctionBody(stmts)){}
	Jua_Val* calc(Scope* env){
		return new Jua_PFunc(env, decList, body);
	}
};

FunctionBody* parse(const string&);

struct JuaVM{
    std::unordered_map<string, Jua_Val*> modules;
    Scope* _G;
    JuaVM();
    void run(const string&);
    Jua_Val* eval(const string&); //不捕获错误

    protected:
    virtual void initBuiltins();
    virtual void makeGlobal();
    virtual string findModule(const string& name) = 0;
    virtual void j_stdout(const jualist&){};
    virtual void j_stderr(JuaError*){};

    private:
    Jua_Val* require(const string& name);
};