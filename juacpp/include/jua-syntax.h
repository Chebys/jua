#include "jua-value.h"
#include "jua-operators.h"
#include <format>

struct Expr{
    virtual Jua_Val* calc(Scope* env) = 0;
};
struct LeftValue{
    virtual void assign(Scope*, Jua_Val*) = 0;
};
struct Declarable: LeftValue{
    virtual void declare(Scope* env, Jua_Val* val) = 0;
    virtual void addDefault(){}
};
struct DeclarationItem{
    Declarable* body;
    Expr* initval;
    DeclarationItem(Declarable* d, Expr* e=nullptr): body(d), initval(e){}
    void addDefault();
    void assign(Scope* env, Jua_Val* val);
    void declare(Scope* env, Jua_Val* val);
};
struct DeclarationList: Declarable{
    //static DeclarationList* fromNames()
    std::deque<DeclarationItem*> decItems;
    DeclarationList(std::deque<DeclarationItem*> items): decItems(items){}
    void assign(Scope* env, Jua_Val* val); //仅用于左值数组
    void declare(Scope* env, Jua_Val* val); //仅用于左值数组
    void rawDeclare(Scope* env, const jualist&);
};
struct LiteralNum: Expr{
    double value;
    LiteralNum(double v): value(v){}
    static LiteralNum* eval(const string& str);
    Jua_Val* calc(Scope*);
};
struct LiteralStr: Expr{
    string value;
    LiteralStr(string v): value(v){}
    Jua_Val* calc(Scope*);
};
struct Template: Expr{
    std::vector<string> strList;
    std::vector<Expr*> exprList;
    Template(std::vector<string>& sl, std::vector<Expr*> el): strList(sl), exprList(el){}
	Jua_Val* calc(Scope*);
};
struct Keyword: Expr{
    char type;
    Keyword(char t): type(t){};
    Jua_Val* calc(Scope*);
    static Keyword* null;
    static Keyword* t;
    static Keyword* f;
    static Keyword* local;
};
struct Varname: Expr, Declarable{
    string str;
    //size_t hash; todo
    Varname(string name): str(name){};
    Jua_Val* calc(Scope*);
    void assign(Scope* env, Jua_Val* val);
    void declare(Scope* env, Jua_Val* val);
};
struct OptionalPropRef: Expr{
    Expr* expr;
    string prop;
	OptionalPropRef(Expr* e, string p): expr(e), prop(p){}
	Jua_Val* _calc(Scope* env);
	Jua_Val* calc(Scope* env);
};
struct PropRef: OptionalPropRef, LeftValue{
    using OptionalPropRef::OptionalPropRef;
    Jua_Val* calc(Scope* env);
	void assign(Scope* env, Jua_Val* val);
};
struct MethWrapper: Expr{
    Expr* expr;
    string key;
    MethWrapper(Expr* e, string n): expr(e), key(n){}
    Jua_Val* calc(Scope* env);
};
struct UnitaryExpr: Expr{
    UniOper oper;
    Expr* pri;
    UnitaryExpr(UniOper type, Expr* expr): oper(type), pri(expr){}
    Jua_Val* calc(Scope* env);
};
struct BinaryExpr: Expr{
    BinOper oper;
    Expr* left;
    Expr* right;
    BinaryExpr(BinOper type, Expr* l, Expr* r): oper(type), left(l), right(r) {}
	Jua_Val* calc(Scope* env);
};
struct Assignment: Expr{
    LeftValue* left;
    Expr* right;
    Assignment(LeftValue* l, Expr* r): left(l), right(r){}
    Jua_Val* calc(Scope* env);
};
struct OperAssignment: Expr{
    BinOper type;
    LeftValue* assignee;
    Expr* left;
    Expr* right;
    OperAssignment(BinOper t, Expr* l, Expr* r): type(t), left(l), right(r){
        assignee = dynamic_cast<LeftValue*>(l);
        if(!assignee)
            throw "Cannot assign to non-leftvalue";
    }
    Jua_Val* calc(Scope* env);
};
struct Subscription: Expr, LeftValue{
    Expr* expr;
    Expr* keyExpr;
	Subscription(Expr* e, Expr* k): expr(e), keyExpr(k){}
	Jua_Val* calc(Scope* env);
	void assign(Scope* env, Jua_Val* val);
};
struct TernaryExpr: Expr{
    Expr* condExpr;
    Expr* trueExpr;
    Expr* falseExpr;
    TernaryExpr(Expr* c, Expr* t, Expr* f): condExpr(c), trueExpr(t), falseExpr(f){}
    Jua_Val* calc(Scope*);
};
struct FlexibleList{
    std::vector<Expr*> exprs;
    FlexibleList(std::vector<Expr*>& list): exprs(list){};
    FlexibleList(initializer_list<Expr*> list): exprs(list){};
    jualist calc(Scope*);
    void appendTo(Scope*, jualist&);
    bool contains(Scope* env, Jua_Val* val){
        for(auto expr: exprs){
            if(*expr->calc(env) == val) return true;
        }
        return false;
    }
};
struct Call: Expr{
    Expr* calee;
    FlexibleList* args;
    Call(Expr* e, FlexibleList* l): calee(e), args(l){}
    Jua_Val* calc(Scope*);
};

struct ObjExpr: Expr{
    typedef std::vector<std::pair<Expr*, Expr*>> Props;
    Props entries;
	ObjExpr(Props p): entries(p){}
	Jua_Val* calc(Scope*);
};
struct ArrayExpr: Expr{
    FlexibleList* list;
    ArrayExpr(FlexibleList* exprs): list(exprs){}
	Jua_Val* calc(Scope*);
};
struct LeftObj: Declarable{
    bool auto_nulled = false;
    typedef std::vector<std::pair<Expr*, DeclarationItem*>> Entries;
    Entries entries;
    LeftObj(Entries e): entries(e){}
    void assign(Scope*, Jua_Val*);
    void declare(Scope* env, Jua_Val* val);
    void addDefault();
    private:
    typedef void (DeclarationItem::*Callback)(Scope*, Jua_Val*);
    void forEach(Scope* env, Jua_Val* obj, Callback);
};

struct Controller{
    bool breaking = false;
    bool continuing = false;
    Jua_Val* retval = nullptr;
    bool isPending() {
        return breaking || continuing || retval;
    }
};

struct Statement{
    Statement* pending_continue = nullptr;
    Statement* pending_break = nullptr;
    virtual void exec(Scope*, Controller*) = 0;
};
struct ExprStatement: Statement{
    Expr* expr;
    ExprStatement(Expr* e): expr(e){}
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
    Expr* expr;
    Return(Expr* e=nullptr): expr(e){}
    void exec(Scope*, Controller*);
};
struct Break: Statement{
    Break(){
        pending_break = this;
    }
    void exec(Scope*, Controller* controller){
        controller->breaking = true;
    }
};
struct Continue: Statement{
    Continue(){
        pending_continue = this;
    }
    void exec(Scope*, Controller* controller){
        controller->continuing = true;
    }
};

struct Block;
struct IfStmt: Statement{
    Expr* cond;
    Block* body;
    Block* elseBody;
    IfStmt(Expr* c, Block* b, Block* e);
    void exec(Scope*, Controller*);
};
struct CaseBlock{
    FlexibleList* cond;
    Block* body;
    CaseBlock(FlexibleList* c, Block* b): cond(c), body(b){}
};
struct SwitchStmt: Statement{
    Expr* expr;
    std::vector<CaseBlock*> cases;
    Block* defaultBody = nullptr;
    SwitchStmt(Expr* e, std::vector<CaseBlock*>& cs, Block* d);
    void exec(Scope*, Controller*);
};
struct WhileStmt: Statement{
    Expr* cond;
    Block* body;
    WhileStmt(Expr* c, Block* b): cond(c), body(b){}
    void exec(Scope*, Controller*);
};
struct ForStmt: Statement{
    Declarable*  declarable;
    Expr* iterable;
    Block* body;
    ForStmt(Declarable* d, Expr* i, Block* b):
        declarable(d), iterable(i), body(b){}
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
    Jua_Val* exec(Scope*); //不会返回 nullptr
};

struct Jua_PFunc: Jua_Func{
    Scope* upenv;
    DeclarationList* decList;
    FunctionBody* body;
    Jua_PFunc(Scope* env, DeclarationList* list, FunctionBody* b):
        Jua_Func(env->vm), upenv(env), decList(list), body(b){}
	Jua_Val* call(jualist& args){
		auto env = new Scope(upenv);
		decList->rawDeclare(env, args);
		return body->exec(env);
	}
};

struct FunExpr: Expr{
    DeclarationList* decList;
    FunctionBody* body;
	FunExpr(DeclarationList* dl, Stmts stmts):
        decList(dl), body(new FunctionBody(stmts)){}
	Jua_Val* calc(Scope* env){
		return new Jua_PFunc(env, decList, body);
	}
};

struct JuaSyntaxError: JuaError{
    size_t pos = -1;
    size_t line = -1;
    size_t col = -1;
    JuaSyntaxError(const string& msg, size_t p, size_t l, size_t c):
        JuaError(msg), pos(p), line(l), col(c){}
    string toDebugString() override {
        if(pos == -1) return std::format("JuaSyntaxError: {}", message);
        return std::format("JuaSyntaxError: {} at {}:{}", message, line, col);
    }
};

FunctionBody* parse(const string&);