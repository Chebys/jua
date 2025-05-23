#include "jua-syntax.h"
#include <bitset>
#include <unordered_set>
using std::string;
typedef std::bitset<256> CharSet;
typedef std::unordered_set<string> StrSet;

static CharSet alphaChars = [](){
    CharSet alphaChars;
    for(char c='A'; c<='Z'; c++){
        alphaChars.set(c);
        alphaChars.set(c+'a'-'A');
    }
    return alphaChars;
}();
static CharSet wordChars = [](){
    CharSet wordChars(alphaChars);
    wordChars.set('_');
    for(char c='0'; c<='9'; c++)
        wordChars.set(c);
    return wordChars;
}();
static CharSet hexChars = [](){
    CharSet hexChars;
    for(char c='0'; c<='9'; c++)hexChars.set(c);
    for(char c='A'; c<='F'; c++){
        hexChars.set(c);
        hexChars.set(c+'a'-'A');
    }
    return hexChars;
}();
static CharSet whiteChars = [](){
    const char _white[] = " \t\r\v\f\n";
    CharSet whiteChars;
    for(char c: _white)
        whiteChars.set(c);
    return whiteChars;
}();
static const char _sepchars[] = "()[]{}.,:;?";
static CharSet sepchars = [](){
    CharSet sepchars;
    for(char c: _sepchars)
        sepchars.set(c);
    return sepchars;
}();
static StrSet assigners{"=", "+=", "-=", "*=", "/=", "&&=", "||="};
static StrSet seprators = [](){
    StrSet seprators(assigners);
    for(char c: _sepchars)
        seprators.insert(string(1, c));
    seprators.insert("?.");
    seprators.insert("?:");
    return seprators;
}();
static StrSet keywords{"as", "break", "continue", "case", "else", "false", "for", "fun", "if", "in", "is", "let", "local", "null", "return", "switch", "true", "while"};
static StrSet sym3;
static StrSet sym2;
static CharSet symchars;
static void regSymStr(const string& str){
    if(str.size()==3)
        sym3.insert(str);
    else if(str.size()==2)
        sym2.insert(str);
    for(char c: str)
        if(c<'a'||c>'z')
            symchars.set(c);
}
static bool initSym(){
    for(string str: seprators)
        regSymStr(str);
    /*for(string str: uniOperator)
        regSymStr(str);
    for(string str: binOperator)
        regSymStr(str);*/
    return true;
}
namespace JuaParser{
    static bool _ = initSym();
}
bool validWordStart(char c){
    return 'a'<=c && c<='z' || 'A'<=c && c<='Z' || c=='_';
}

string missing(char c){
    if(c=='\'')return "Missing \"'\"";
    string msg = "Missing '";
    msg.push_back(c);
    msg.push_back('\'');
    return msg;
}
string unexpected(string str){
    string msg = "Unexpected \"";
    msg.append(str);
    msg.push_back('"');
    return msg;
}
string unexpected(string une, string e){
    string msg = unexpected(une);
    msg.append("; Expect \"");
    msg.append(e);
    msg.push_back('"');
    return msg;
}

struct Token{
    enum Type{SEP, UNIOP, BINOP, WORD, NUM, STR, DQ_STR, PAREN, BRAKET, BRACE};
    Type type;
    string str; //用于 STR 时，储存实际值
    bool isKeyword;
    bool isBinop;
    bool isValidVarname;
    Token(Type t, const string& s): type(t), str(s){
        //d_log(s);
        isKeyword = keywords.contains(s);
        isBinop = binOpers.contains(s);
        isValidVarname = type==WORD && !isKeyword;
    }
    static Token* symbol(const string& s){
        //todo
        return new Token(SEP, s);
    }
};
struct TokensReader{
    virtual Token* read() = 0; //读完则报错
    string readStr(){ //读完则报错
        return read()->str;
    }
    virtual Token* preview() = 0; //读完则返回 nullptr
    string previewStr(){ //读完则返回空串
        auto next = preview();
        if(next)return next->str;
        return "";
    }
    void assetStr(const string& str){
        auto token = read();
		if(token->str != str)
			throw unexpected(token->str, str);
    }
    bool skipStr(const string& str){
        auto token = preview();
        if(token && token->str==str){
            read();
            return true;
        }
        return false;
    }
    bool end(){
        return !preview();
    }
    void assetEnd(){
        auto token = preview();
        if(token)throw unexpected(token->str);
    }
};

struct ListReader: TokensReader{
    std::vector<Token*> tokens;
    size_t pos;
    ListReader(std::vector<Token*> list): tokens(list){
        pos = 0;
    }
    Token* read(){
        if(pos >= tokens.size())throw "Unfinished input";
        return tokens[pos++];
    }
    Token* preview(){
        if(pos >= tokens.size())return nullptr;
        return tokens[pos];
    }
};
struct Enclosure: Token{
    ListReader reader;
    Enclosure(Type t, std::vector<Token*> list): Token(t, "<Enclosure>"), reader(list){}
};
struct StrTmpl: Token{
    std::vector<string> strList;
    std::vector<Token*> tokenList;
	StrTmpl(std::vector<string> sl, std::vector<Token*> tl):
        Token(DQ_STR, "<StrTmpl>"), strList(sl), tokenList(tl){}
};

std::unordered_map<char, char> escape_map{
    {'a', '\a'}, {'b', '\b'}, {'f', '\f'},
    {'n', '\n'}, {'r', '\r'}, {'t', '\t'}, {'v', '\v'}
};

struct ScriptReader: TokensReader{
    string script;
    size_t pos = 0;
    size_t line = 1;
    size_t col = 0;
    Token* cache = nullptr;
    ScriptReader(const string& s): script(s){}
    Token* read();
    Token* preview();
    private:
    char readChar(){
        if(eof())throw "Unfinished input";
        char c = script[pos];
        forward(c=='\n');
        return c;
    }
    Token* doRead();
    void forward(bool newline=false){
        pos++;
        if(newline){
            line++;
            col = 0;
        }else{
            col++;
        }
    }
    void forwardx(size_t len){
        while(len--)forward();
    }
    bool eof(){
        return pos >= script.size();
    }
    bool skipWhite(){
        if(eof())return false;
        char c = script[pos];
        if(whiteChars.test(c)){
            forward(c=='\n');
            return true;
        }
        return false;
    }
    void skipVoid(){
        while(skipWhite());
        //todo
    }
    Token* readWord(char start){
        string word(1, start);
        while(!eof()){
            char c = script[pos];
            if(wordChars.test(c)){
                forward();
                word.push_back(c);
            }else{
                break;
            }
        }
        return new Token(Token::WORD, word);
    }
    bool readSimpleNum(string& start){
        //仅含0-9
        //不保证读入
        //读完时返回true
        while(!eof()){
            char next = script[pos];
            if(next<'0' || next>'9')return false;
            forward();
            start.push_back(next);
        }
        return true;
    }
    string readNum(char start){
        string num(1, start);
        if(eof())return num;
        if(start=='0' && script[pos]=='x'){
            num.push_back('x');
            forward();
            while(!eof()){
                char next = script[pos];
                if('0'<=next&&next<='9'||'a'<=next&&next<='f'||'A'<=next&&next<='F'){
                    forward();
                    num.push_back(next);
                }
                else break;
            }
            return num;
        }
        if(readSimpleNum(num))return num;
        //此时非eof
        if(script[pos]=='.'){
            num.push_back('.');
            forward();
            if(readSimpleNum(num))return num;
        }
        //此时非eof
        if(script[pos]=='e'){
            num.push_back('e');
            forward();
            char next = readChar();
            if(next=='-' || '0'<=next || next<='9')num.push_back(next);
            else throw next;
            readSimpleNum(num);
        }
        return num;
    }
    std::vector<Token*> readUntil(char end){
        //d_log("<Enclosure start>");
        std::vector<Token*> list;
        while(true){
            skipVoid();
            if(eof())throw missing(end);
            if(script[pos]==end){
                forward(); //假设 end 不是 newline
                return list;
            }
            list.push_back(doRead());
        }
    }
    string substr(size_t len){
        //超出末尾则返回剩余子串
        return script.substr(pos, len);
    }
    Token* readSymbol(char start){
        if(start=='(')return new Enclosure(Token::PAREN, readUntil(')'));
        if(start=='[')return new Enclosure(Token::BRAKET, readUntil(']'));
        if(start=='{')return new Enclosure(Token::BRACE, readUntil('}'));
        string str(1, start);
        if(sym3.contains(str.append(substr(2)))){
            forwardx(2);
            return Token::symbol(str);
        }
        str.resize(1);
        if(sym2.contains(str.append(substr(1)))){
            forward();
            return Token::symbol(str);
        }
        str.resize(1);
        return Token::symbol(str);
    }
    char escape(bool allow_newline=false){
        //从 `\` 后开始读取
        char c = readChar();
        if(c=='x' || c=='u')throw "todo: \\x";
        else if('0'<=c && c<='9')throw "todo: \\0";
        else if(c=='\n' && !allow_newline)throw "cannot wrap inside SQ string";
        else if(escape_map.contains(c))return escape_map[c];
        return c;
    }
    string readSQStr(){
        //从单引号之后开始读取
		//返回实际值
        string value;
        while(true){
            char c = readChar();
            if(c=='\'')return value;
            if(c=='\\')value.push_back(escape());
            else if(c=='\n')throw "cannot wrap inside SQ string";
            else value.push_back(c);
        }
    }
    Token* readStrTmpl();
    std::pair<string, Token*> readDQStr();
    string readBQStr(){
        //从反引号之后开始读取
		//返回实际值
        string value;
        while(true){
            char c = readChar();
            if(c=='`')return value;
            value.push_back(c);
        }
    }
};
Token* ScriptReader::read(){
    if(cache){
        auto _c = cache;
        cache = nullptr;
        return _c;
    }
    auto token = doRead();
    if(!token)throw "Unfinished input";
    return token;
}
Token* ScriptReader::preview(){
    if(!cache)cache = doRead();
    return cache;
}
Token* ScriptReader::doRead(){
    skipVoid();
    if(eof())return nullptr;
    char c = readChar();
    if(symchars.test(c))
        return readSymbol(c);
    else if(validWordStart(c))
        return readWord(c);
    else if('0'<=c && c<='9')
        return new Token(Token::NUM, readNum(c));
    else if(c=='\'')
        return new Token(Token::STR, readSQStr());
    else if(c=='"')return readStrTmpl();
    else if(c=='`')return new Token(Token::STR, readBQStr());
    else{
        d_log(c);
        throw "todo: doRead";
    }
}
Token* ScriptReader::readStrTmpl(){
    std::vector<string> slist;
    std::vector<Token*> tlist;
    while(true){
        auto pair = readDQStr();
        slist.push_back(pair.first);
        if(pair.second)
            tlist.push_back(pair.second);
        else
            return new StrTmpl(slist, tlist);
    }
}
std::pair<string, Token*> ScriptReader::readDQStr(){
    string str;
    while(true){
        char c = readChar();
        if(c=='"')return {str, nullptr};
        if(c=='$'){
            char next = readChar();
            if(next=='{'){
                auto tl = readUntil('}');
                return {str, new Enclosure(Token::BRACE, tl)};
            }
            if(validWordStart(next)){
                return {str, readWord(next)};
            }
            throw "Invalid char";
        }
        if(c=='\\')str.push_back(escape(true));
        else str.push_back(c);
    }
}

Expr* parsePrimaryTail(Expr*, TokensReader&);
Expr* parseExpr(TokensReader&);
Expr* parseFunc(TokensReader& reader);
Expr* parseObj(TokensReader& reader);
Stmts parseStatements(TokensReader& reader);

FlexibleList* parseFlexExprList(TokensReader& reader){
    //可空，可尾随逗号，读完 reader
    //todo: *
    std::vector<Expr*> exprs;
    while(true){
        if(reader.end())return new FlexibleList(exprs);
        exprs.push_back(parseExpr(reader));
        if(reader.end())return new FlexibleList(exprs);
        reader.assetStr(",");
    }
}
Expr* parsePrimary(TokensReader& reader){
    auto start = reader.read();
    switch (start->type){
    case Token::WORD:{
        if(start->isValidVarname)
            return parsePrimaryTail(new Varname(start->str), reader);
        if(start->str=="null")
            return parsePrimaryTail(Keyword::null, reader);
        if(start->str=="true")
            return parsePrimaryTail(Keyword::t, reader);
        if(start->str=="false")
            return parsePrimaryTail(Keyword::f, reader);
        if(start->str=="local")
            return parsePrimaryTail(Keyword::local, reader);
        if(start->str=="fun"){
            auto func = parseFunc(reader);
            return parsePrimaryTail(func, reader);
        }
        throw unexpected(start->str);
    }
    case Token::NUM:{
        return parsePrimaryTail(LiteralNum::eval(start->str), reader);
    }
    case Token::STR:{
        return parsePrimaryTail(new LiteralStr(start->str), reader);
    }
    case Token::DQ_STR:{
        auto dqstr = static_cast<StrTmpl*>(start);
        std::vector<Expr*> exprs;
        for(auto token: dqstr->tokenList){
            if(token->isValidVarname){
                exprs.push_back(new Varname(token->str));
            }else if(token->type==Token::BRACE){
                auto cl = static_cast<Enclosure*>(token);
                exprs.push_back(parseExpr(cl->reader));
                cl->reader.assetEnd();
            }
        }
        return new Template(dqstr->strList, exprs);
    }
    case Token::PAREN:{
        auto cl = static_cast<Enclosure*>(start);
        auto expr = parseExpr(cl->reader);
        cl->reader.assetEnd();
        return parsePrimaryTail(expr, reader);
    }
    case Token::BRACE:{
        auto cl = static_cast<Enclosure*>(start);
        auto expr = parseObj(cl->reader);
        return parsePrimaryTail(expr, reader);
    }
    case Token::BRAKET:{
        auto cl = static_cast<Enclosure*>(start);
        ArrayExpr* arr;
        if(cl->reader.end())
            arr = new ArrayExpr({});
        else{
            auto list = parseFlexExprList(cl->reader);
            cl->reader.assetEnd();
            arr = new ArrayExpr(list);
        }
        return parsePrimaryTail(arr, reader);
    }
    case Token::UNIOP:{
        throw "todo: UNIOP";
    }
    default:
        throw unexpected(start->str);
    }
}
Expr* parsePrimaryTail(Expr* start, TokensReader& reader){
    auto next = reader.preview();
    if(!next)return start;
    switch (next->type){
        case Token::SEP:
            if(next->str=="."){ //属性引用
                reader.read();
                auto id = reader.read();
                if(id->type!=Token::WORD)throw "expect property name";
                auto expr = new PropRef(start, id->str);
                return parsePrimaryTail(expr, reader);
            }else if(next->str=="?."){
                reader.read();
                auto id = reader.read();
                if(id->type!=Token::WORD)throw "expect property name";
                auto expr = new OptionalPropRef(start, id->str);
                return parsePrimaryTail(expr, reader);
            }else if(next->str==":"){ //方法包装
                throw "todo: 方法包装";
            }
            return start;
        case Token::PAREN:{
            reader.read();
            auto cl = static_cast<Enclosure*>(next);
            auto args = parseFlexExprList(cl->reader);
            auto call = new Call(start, args);
            return parsePrimaryTail(call, reader);
        }
        case Token::BRAKET:{
            reader.read();
            auto cl = static_cast<Enclosure*>(next);
			auto key = parseExpr(cl->reader);
			cl->reader.assetEnd();
			auto expr = new Subscription(start, key);
			return parsePrimaryTail(expr, reader);
        }
        default:
            return start;
    }
}
Expr* parseExpr(TokensReader& reader){
    //todo
    auto start = parsePrimary(reader);
    auto next = reader.preview();
    if(!next)return start;
    if(next->isBinop){
        reader.read();
        //todo: 优先级
        return new BinaryExpr(binOpers[next->str], start, parseExpr(reader));
    }else if(next->str == "="){
        reader.read();
        throw "todo: Assignment";
        //return new Assignment();
    }
    return start;
}

std::pair<Expr*, Expr*> parseProp(TokensReader& reader){
    Expr *key, *val;
	auto start = reader.read();
	if(start->type==Token::WORD){
		key = new LiteralStr(start->str);
		auto next = reader.preview();
		if(next && next->str=="="){
			reader.read();
			val = parseExpr(reader);
		}else if(next && next->type==Token::PAREN){
			val = parseFunc(reader);
		}else if(start->isValidVarname){
			val = new Varname(start->str);
		}else{
			throw "Invalid Varname";
		}
	}else if(start->type==Token::BRAKET){
        auto cl = static_cast<Enclosure*>(start);
		key = parseExpr(cl->reader);
		cl->reader.assetEnd();
		auto next = reader.preview();
		if(!next)
			throw "Unfinished input";
		if(next->str=="="){
			reader.read();
			val = parseExpr(reader);
		}else if(next->type==Token::PAREN){
			val = parseFunc(reader);
		}else{
			throw unexpected(next->str);
		}
	}
	return {key, val};
}
Expr* parseObj(TokensReader& reader){
    //读完 reader
	ObjExpr::Props entries;
	while(true){
		//此时刚开始读取或读完上一个逗号
		if(reader.end()){ //允许尾随逗号
			return new ObjExpr(entries);
		}
		entries.push_back(parseProp(reader));
		if(reader.end())
			return new ObjExpr(entries);
		reader.assetStr(",");
	}
}
Expr* parseFunc(TokensReader& reader){
    //从 Enclosure 开始读取
	auto _args = reader.read();
	if(_args->type != Token::PAREN)
		throw missing('(');
    auto args = static_cast<Enclosure*>(_args);
	DeclarationList* decList;
	if(args->reader.end()){
		decList = new DeclarationList({});
	}else{
        throw "todo: parseFlexDecList";
		//decList = parseFlexDecList(args.reader);
		args->reader.assetEnd();
	}
	Stmts stmts;
	auto next = reader.read();
	if(next->type == Token::BRACE){
        auto cl = static_cast<Enclosure*>(next);
		stmts = parseStatements(cl->reader);
	}else if(next->str == "="){
		auto expr = parseExpr(reader);
        throw "todo: fun()=0";
		//stmts = Stmts(new Return(expr));
	}else{
		throw unexpected(next->str);
	}
	return new FunExpr(decList, stmts);
}

Declarable* parseDeclarable(TokensReader& reader){
    auto next = reader.read();
    if(next->isValidVarname)return new Varname(next->str);
    throw "todo: parseDeclarable";
}
DeclarationItem* parseDecItem(TokensReader& reader){
    Declarable* declarable = parseDeclarable(reader);
	Expr* defval = nullptr;
	bool auto_null = false;
	string next = reader.previewStr();
	if(next=="?"){
		reader.read();
		auto_null = true;
	}else if(next=="="){
		reader.read();
		defval = parseExpr(reader);
	}
	DeclarationItem* item = new DeclarationItem(declarable, defval); //todo: src
	if(auto_null)item->addDefault();
	return item;
}
DeclarationList* parseDecList(TokensReader& reader){
    std::deque<DeclarationItem*> items;
    while(true){
		items.push_back(parseDecItem(reader));
		if(reader.previewStr()==",")
			reader.read();
		else
			return new DeclarationList(items);
	}
}

Statement* parseStatement(TokensReader& reader){
    //若为空语句或读完，则返回 nullptr
    auto start = reader.preview();
    if(!start || start->str==";")return nullptr;
    if(start->isKeyword){
        if(start->str=="return"){
            reader.read();
            if(reader.skipStr(";"))return new Return;
            auto expr = parseExpr(reader);
            reader.skipStr(";");
            return new Return(expr);
        }
        if(start->str=="let"){
            reader.read();
            auto list = parseDecList(reader);
            reader.skipStr(";");
            return new Declaration(list);
        }
        throw unexpected(start->str);
    }
    auto expr =  parseExpr(reader);
    return new ExprStatement(expr);
}
Stmts parseStatements(TokensReader& reader){
    Stmts stmts;
    while(true){
        if(reader.end())return stmts;
        auto stmt = parseStatement(reader);
        if(stmt)stmts.push_back(stmt);
    }
}

FunctionBody* parse(const string& script){
    ScriptReader reader(script);
    auto stmts = parseStatements(reader);
    return new FunctionBody(stmts);
}