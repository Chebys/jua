#pragma once
#include "jua-value.h"
#include "jua-operators.h"
#include <format>
namespace Jua {
struct Expr{
    virtual Value* calc(Scope* env) = 0;
};
struct LeftValue{
    virtual void assign(Scope*, Value*) = 0;
};
struct Declarable: LeftValue{
    virtual void declare(Scope* env, Value* val) = 0;
    virtual void addDefault(){}
};
struct DeclarationItem{
    Declarable* body;
    Expr* initval;
    DeclarationItem(Declarable* d, Expr* e=nullptr): body(d), initval(e){}
    void addDefault();
    void assign(Scope* env, Value* val);
    void declare(Scope* env, Value* val);
};
struct DeclarationList: Declarable{
    //static DeclarationList* fromNames()
    std::deque<DeclarationItem*> decItems;
    DeclarationList(std::deque<DeclarationItem*> items): decItems(items){}
    void assign(Scope* env, Value* val); //仅用于左值数组
    void declare(Scope* env, Value* val); //仅用于左值数组
    void rawDeclare(Scope* env, const jualist&);
};
struct LiteralNum: Expr{
    double value;
    LiteralNum(double v): value(v){}
    static LiteralNum* eval(const std::string& str);
    Value* calc(Scope*);
};
struct LiteralStr: Expr{
    std::string value;
    LiteralStr(std::string v): value(v){}
    Value* calc(Scope*);
};
struct Template: Expr{
    std::vector<std::string> strList;
    std::vector<Expr*> exprList;
    Template(std::vector<std::string>& sl, std::vector<Expr*> el): strList(sl), exprList(el){}
	Value* calc(Scope*);
};
struct Keyword: Expr{
    char type;
    Keyword(char t): type(t){};
    Value* calc(Scope*);
    static Keyword* null;
    static Keyword* t;
    static Keyword* f;
    static Keyword* local;
};
struct Varname: Expr, Declarable{
    std::string str;
    //size_t hash; todo
    Varname(std::string name): str(name){};
    Value* calc(Scope*);
    void assign(Scope* env, Value* val);
    void declare(Scope* env, Value* val);
};
struct OptionalPropRef: Expr{
    Expr* expr;
    std::string prop;
	OptionalPropRef(Expr* e, std::string p): expr(e), prop(p){}
	Value* _calc(Scope* env);
	Value* calc(Scope* env);
};
struct PropRef: OptionalPropRef, LeftValue{
    using OptionalPropRef::OptionalPropRef;
    Value* calc(Scope* env);
	void assign(Scope* env, Value* val);
};
struct MethWrapper: Expr{
    Expr* expr;
    std::string key;
    MethWrapper(Expr* e, std::string n): expr(e), key(n){}
    Value* calc(Scope* env);
};
struct UnitaryExpr: Expr{
    UniOper oper;
    Expr* pri;
    UnitaryExpr(UniOper type, Expr* expr): oper(type), pri(expr){}
    Value* calc(Scope* env);
};
struct BinaryExpr: Expr{
    BinOper oper;
    Expr* left;
    Expr* right;
    BinaryExpr(BinOper type, Expr* l, Expr* r): oper(type), left(l), right(r) {}
	Value* calc(Scope* env);
};
struct Assignment: Expr{
    LeftValue* left;
    Expr* right;
    Assignment(LeftValue* l, Expr* r): left(l), right(r){}
    Value* calc(Scope* env);
};
struct OperAssignment: Expr{
    BinOper type;
    LeftValue* assignee;
    Expr* left;
    Expr* right;
    OperAssignment(BinOper t, Expr* l, Expr* r): type(t), left(l), right(r){
        assignee = dynamic_cast<LeftValue*>(l);
        if(!assignee)
            throw new JuaError("Cannot assign to non-leftvalue", JuaError::Kind::Syntax);
    }
    Value* calc(Scope* env);
};
struct Subscription: Expr, LeftValue{
    Expr* expr;
    Expr* keyExpr;
	Subscription(Expr* e, Expr* k): expr(e), keyExpr(k){}
	Value* calc(Scope* env);
	void assign(Scope* env, Value* val);
};
struct TernaryExpr: Expr{
    Expr* condExpr;
    Expr* trueExpr;
    Expr* falseExpr;
    TernaryExpr(Expr* c, Expr* t, Expr* f): condExpr(c), trueExpr(t), falseExpr(f){}
    Value* calc(Scope*);
};
struct FlexibleList{
    std::vector<Expr*> exprs;
    FlexibleList(std::vector<Expr*>& list): exprs(list){};
    FlexibleList(std::initializer_list<Expr*> list): exprs(list){};
    jualist calc(Scope*);
    void appendTo(Scope*, jualist&);
    bool contains(Scope* env, Value* val){
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
    Value* calc(Scope*);
};

struct ObjExpr: Expr{
    typedef std::vector<std::pair<Expr*, Expr*>> Props;
    Props entries;
	ObjExpr(Props p): entries(p){}
	Value* calc(Scope*);
};
struct ArrayExpr: Expr{
    FlexibleList* list;
    ArrayExpr(FlexibleList* exprs): list(exprs){}
	Value* calc(Scope*);
};
struct LeftObj: Declarable{
    bool auto_nulled = false;
    typedef std::vector<std::pair<Expr*, DeclarationItem*>> Entries;
    Entries entries;
    LeftObj(Entries e): entries(e){}
    void assign(Scope*, Value*);
    void declare(Scope* env, Value* val);
    void addDefault();
    private:
    typedef void (DeclarationItem::*Callback)(Scope*, Value*);
    void forEach(Scope* env, Value* obj, Callback);
};

struct Controller{
    bool breaking = false;
    bool continuing = false;
    Value* retval = nullptr;
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
    Value* exec(Scope*); //不会返回 nullptr
};

struct Jua_PFunc: Function{
    Scope* upenv;
    DeclarationList* decList;
    FunctionBody* body;
    Jua_PFunc(Scope* env, DeclarationList* list, FunctionBody* b):
        Function(env->vm), upenv(env), decList(list), body(b){}
    void trace(VM* vm) override; //标记定义作用域，闭包因此保活
	Value* call(jualist& args){
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
	Value* calc(Scope* env){
		return new Jua_PFunc(env, decList, body);
	}
};

struct JuaSyntaxError: JuaError{
    size_t pos = -1;
    size_t line = -1;
    size_t col = -1;
    JuaSyntaxError(const std::string& msg, size_t p, size_t l, size_t c):
        JuaError(msg), pos(p), line(l), col(c){}
    std::string toDebugString() override {
        if(pos == (size_t)-1) return std::format("JuaSyntaxError: {}", message);
        return std::format("JuaSyntaxError: {} at {}:{}", message, line, col);
    }
};

FunctionBody* parse(const std::string&);

}