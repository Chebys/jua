#include "jua-value.h"
#include "jua-operators.h"

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
struct DeclarationList{
    //static DeclarationList* fromNames()
    std::deque<DeclarationItem*> decItems;
    DeclarationList(std::deque<DeclarationItem*> items): decItems(items){}
    void assign(Scope* env, Jua_Val* val);
    void declare(Scope* env, Jua_Val* val);
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
struct MethWrapper;
struct UnitaryExpr;
struct BinaryExpr: Expr{
    BinOper oper;
    Expr* left;
    Expr* right;
    BinaryExpr(BinOper type, Expr* l, Expr* r): oper(type), left(l), right(r){}
	Jua_Val* calc(Scope* env);
};
struct Assignment: Expr{
    //todo
};
struct Subscription: Expr, LeftValue{
    Expr* expr;
    Expr* keyExpr;
	Subscription(Expr* e, Expr* k): expr(e), keyExpr(k){}
	Jua_Val* calc(Scope* env);
	void assign(Scope* env, Jua_Val* val);
};
struct TernaryExpr;
struct FlexibleList{
    std::vector<Expr*> exprs;
    FlexibleList(std::vector<Expr*>& list): exprs(list){};
    FlexibleList(initializer_list<Expr*> list): exprs(list){};
    jualist calc(Scope*);
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

struct FunExpr: Expr{
    DeclarationList* decList;
    FunctionBody* body;
	FunExpr(DeclarationList* dl, Stmts stmts):
        decList(dl), body(new FunctionBody(stmts)){}
	Jua_Val* calc(Scope* env){
		return new Jua_PFunc(env, decList, body);
	}
};

FunctionBody* parse(const string&);