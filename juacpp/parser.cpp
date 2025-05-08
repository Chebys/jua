#include "jua-internal.h"
//#include "operators.h"
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

struct Token{
    enum Type{SEP, UNIOP, BINOP, WORD, NUM, STR, DQ_STR, PAREN, BRAKET, BRACE};
    Type type;
    string str; //用于 STR 时，储存实际值
    bool isKeyword;
    bool isBinop;
    bool isValidVarname;
    Token(Type t, const string& s): type(t), str(s){
        //todo
        //d_log(s);
        isKeyword = keywords.contains(s);
        isBinop = false;
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
			throw "Unexpected token";
    }
    bool end(){
        return !preview();
    }
    bool assetEnd(){
        auto token = preview();
        if(token)throw "Unexpected token";
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
struct StrTmpl;

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
    string readWord(char start){
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
        return word;
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
                char next = readChar();
                if('0'<=next&&next<='9'||'a'<=next&&next<='f'||'A'<=next&&next<='F')num.push_back(next);
                else break;
            }
            return num;
        }
        if(readSimpleNum(num))return num;
        //此时非eof
        if(script[pos]=='.'){
            forward();
            if(readSimpleNum(num))return num;
        }
        //此时非eof
        if(script[pos]=='e'){
            forward();
            char next = readChar();
            if(next=='-' || '0'<=next || next<='9')num.push_back(next);
            else throw next;
            readSimpleNum(num);
        }
        return num;
    }
    std::vector<Token*> readUntil(char end){
        std::vector<Token*> list;
        while(true){
            skipVoid();
            if(eof())throw "Missing end";
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
    string readSQStr(){
        //从单引号之后开始读取
		//返回实际值
        string value;
        while(true){
            char c = readChar();
            if(c=='\'')return value;
            if(c=='\\'){
                char next = readChar();
                if(next=='x' || next=='u')throw "todo: \\x";
                else if('0'<=next && next<='9')throw "todo: \\0";
                else if(next=='\n')throw "cannot wrap inside SQ string";
                else if(escape_map.contains(next))value.push_back(escape_map[next]);
                else value.push_back(next);
            }else if(c=='\n')throw "cannot wrap inside SQ string";
            else value.push_back(c);
        }
    }
    Token* readStrTmpl(){
        throw "todo: readStrTmpl";
    }
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
    else if('a'<=c && c<='z' || 'A'<=c && c<='Z' || c=='_')
        return new Token(Token::WORD, readWord(c));
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


Expression* parsePrimaryTail(Expression*, TokensReader*);
Expression* parseExpr(TokensReader* reader){
    auto start = reader->read();
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
        throw start->str;
    }
    case Token::NUM:{
        return parsePrimaryTail(LiteralNum::eval(start->str), reader);
    }
    case Token::STR:{
        return parsePrimaryTail(new LiteralStr(start->str), reader);
    }
    
    default:
        throw "???";
        break;
    }
}
Expression* parsePrimaryTail(Expression* start, TokensReader* reader){
    auto next = reader->preview();
    if(!next)return start;
    switch (next->type){
        case Token::PAREN:{
            reader->read();
            auto cl = static_cast<Enclosure*>(next);
            auto arg = parseExpr(&cl->reader);
            //todo
            auto args = new FlexibleList({arg});
            auto call = new Call(start, args);
            return parsePrimaryTail(call, reader);
        }
        default:
            return start;
    }
}
Statement* parseStatement(TokensReader* reader){
    //若为空语句或读完，则返回 nullptr
    auto start = reader->preview();
    if(!start || start->str==";")return nullptr;
    if(start->isKeyword){
        throw "todo: parseStatement";
    }
    auto expr =  parseExpr(reader);
    return new ExprStatement(expr);
}
std::vector<Statement*> parseStatements(TokensReader* reader){
    std::vector<Statement*> stmts;
    while(true){
        if(reader->end())return stmts;
        auto stmt = parseStatement(reader);
        if(stmt)stmts.push_back(stmt);
    }
}

FunctionBody* parse(const string& script){
    ScriptReader reader(script);
    auto stmts = parseStatements(&reader);
    return new FunctionBody(stmts);
}