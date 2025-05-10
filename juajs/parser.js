import {JuaSyntaxError} from 'jua/value';
import {uniOperator, binOperator} from 'jua/operator';
import {
	UnitaryExpr, BinaryExpr, TernaryExpr, LiteralNum, LiteralStr, Template, Keyword, Varname,
	OptionalPropRef, PropRef, MethWrapper, Subscription, Call, ObjExpr, ArrayExpr, FunExpr, Assignment,
	Declarable, DeclarationItem, DeclarationList, LeftObj,
	ExprStatement, Declaration, Return, Break, Continue, IfStatement, SwitchStatement, CaseBlock, WhileStatement, ForStatement, Block, FunctionBody
} from 'jua/program';

//polyfill
Set.prototype.union ||= function(set){
	let newset = new Set(this);
	for(let e of set)
		newset.add(e);
	return newset;
}

const sepchars = new Set('()[]{}.,:;?');
const assigners = new Set(['=', '+=', '-=', '*=', '/=', '&&=', '||='])
const seprators = sepchars.union(assigners);
seprators.add('?.');
seprators.add('?:');
const validIdStartReg = /[_a-zA-Z]/;
const validIdReg = /[_a-zA-Z0-9]/;
const keywords = new Set(['as', 'break', 'continue', 'case', 'else', 'false', 'for', 'fun', 'if', 'in', 'is', 'let', 'local', 'null', 'return', 'switch', 'true', 'while']);
const sym3 = new Set; //3字符符号
const sym2 = new Set; //2字符符号
const symchars = new Set;
function regSymStr(str){
	if(str.length==3)
		sym3.add(str);
	else if(str.length==2)
		sym2.add(str);
	for(let c of str)
		if(!c.match(/[a-z]/))
			symchars.add(c);
}
for(let str of seprators)
	regSymStr(str);
for(let str in uniOperator)
	regSymStr(str);
for(let str in binOperator)
	regSymStr(str);

class Token{
	//括号不能单独作为 Token，而是使用 Enclosure 子类
	//关于 type:
	//word 优先
	//非 word 运算符优先视作一元运算符
	//使用 isBinop 判断二元运算符
	constructor(type, str){ //str 非空
		//console.log(str) //方便早期调试
		this.type = type;
		this.str = str;
		this.isKeyword = keywords.has(str);
		this.isBinop = this.str in binOperator;
	}
	static symbol(str){
		if(seprators.has(str))
			return new Token('separator', str);
		if(str in uniOperator)
			return new Token('uniop', str);
		if(str in binOperator)
			return new Token('binop', str);
		throw new Error(str);
	}
	get isValidVarname(){
		return this.type=='word' && !this.isKeyword;
	}
	toString(){
		return this.str;
	}
}
class Enclosure extends Token{ //括号包围的token序列
	constructor(type, tokens){ //以一对括号作为type
		super(type, '<Enclosure>');
		this.reader = new ListReader(tokens);
	}
}
class StrTmpl extends Token{
	constructor(strList, tokenList){
		super('dq_str', '<StrTmpl>');
		//asset(strList.length == tokenList.length+1)
		this.strList = strList; //序列中为 readDQStr 返回的字符串
		this.tokenList = tokenList; //可能为 'word' 或 '{}'
	}
}
class TokensReader{ //抽象类
	//潜在问题：未必有源码位置信息
	read(errmsg){ //读完时报错 todo: 记录位置
		throw new Error('pure virtual function');
	}
	readStr(errmsg){
		return this.read(errmsg).str;
	}
	preview(){ //读完时，返回 null
		throw new Error('pure virtual function');
	}
	previewStr(){ //读完时，返回 undefined
		return this.preview()?.str;
	}
	assetStr(str){
		let token = this.read();
		if(token.str!=str)
			throw new JuaSyntaxError(`Unexpected token: '${token.str}'; Expect '${str}'`);
	}
	skipStr(str){
		let token = this.preview();
		if(token?.str == str){
			this.read();
			return true;
		}
		return false;
	}
	end(){
		return !this.preview();
	}
	assetEnd(){
		let token = this.preview();
		if(token)
			throw new JuaSyntaxError('Unexpected token: '+token);
	}
}
//可优化：读取字面量同时进行语法分析
class ScriptReader extends TokensReader{
	constructor(script, fileName){
		super();
		this.script = script;
		this.fileName = fileName;
		this.pos = 0; //接下来要读取的位置
		this.line = 1; //当前行，从 1 开始
		this.col = 0; //当前行已读完的列
		this.cache = null;
	}
	read(errmsg='Unfinished input'){
		let cache = this.cache;
		if(cache){
			this.cache = null;
			return cache;
		}
		let token = this.doRead(errmsg);
		if(!token)
			throw new JuaSyntaxError(errmsg);
		return token;
	}
	preview(){ //仍可能推进
		if(!this.cache)
			this.cache = this.doRead();
		return this.cache;
	}
	//private:
	doRead(){
		//忽略 cache
		//读完时，返回 null
		this.skipVoid();
		if(this.eof())
			return null;
		//保证有有意义的字符
		let c = this.script[this.pos];
		this.forward();
		if(symchars.has(c)){
			return this.readSymbol(c);
		}else if(validIdStartReg.test(c)){
			return new Token('word', c + this.match(/\w*/y));
		}else if(/[0-9]/.test(c)){
			return new Token('literal_num', this.readNum(c));
		}else if(c=="'"){
			return new Token('sq_str', this.readSQStr());
		}else if(c=='"'){
			return this.readStrTmpl();
		}else if(c=='`'){
			let str = this.match(/[^`]*`/y);
			if(!str)throw new JuaSyntaxError('Unfinished back quote string');
			return new Token('bq_str', '`' + str);
		}else{
			throw new JuaSyntaxError('Unrecognized: '+c);
		}
	}
	skipWhite(){ //跳过单个空白符。成功则返回true
		let c = this.script[this.pos];
		if(!/\s/.test(c))
			return false;
		this.pos++;
		if(c=='\n'){
			this.line++;
			this.col = 0;
		}else{
			this.col++;
		}
		return true;
	}
	skipVoid(){ //跳过所有能跳过的东西
		while(this.skipWhite());
		while(this.substr(2)=='//'){
			this.readComment();
			while(this.skipWhite());
		}
	}
	eof(){
		//不同于end，不会忽略空白符
		return this.pos>=this.script.length;
	}
	forward(len=1){ //不换行
		this.pos += len;
		this.col += len;
	}
	substr(len){
		return this.script.slice(this.pos, this.pos+len);
	}
	readComment(){ //会读完换行符
		let start = this.pos;
		this.pos = this.script.indexOf('\n', start);
		if(this.pos == -1){
			this.pos = this.script.length;
			this.col += this.pos-start;
		}else{
			this.pos++;
			this.line++;
			this.col = 0;
		}
		return this.script.slice(start, this.pos);
	}
	readSymbol(startchar){
		//优先匹配长符号
		//返回Token
		let str;
		if(sym3.has(str = startchar+this.substr(2))){
			this.forward(2);
			return Token.symbol(str);
		}else if(sym2.has(str = startchar+this.substr(1))){
			this.forward();
			return Token.symbol(str);
		}else if(sepchars.has(startchar)){
			if(startchar=='(')
				return new Enclosure('()', this.readUntil(')'));
			if(startchar=='[')
				return new Enclosure('[]', this.readUntil(']'));
			if(startchar=='{')
				return new Enclosure('{}', this.readUntil('}'));
			return new Token('separator', startchar);
		}else{
			return Token.symbol(startchar);
		}
	}
	readUntil(endStr){
		//读完endStr
		//返回Token序列（不含endStr）
		let list = [];
		while(true){
			this.skipVoid();
			if(this.eof())
				throw new JuaSyntaxError(`Missing '${endStr}'`);
			if(this.script[this.pos]==endStr){
				this.forward();
				return list;
			}
			list.push(this.doRead());
		}
	}
	match(reg){
		//reg 需要带有'y'标志
		//会 forward
		//匹配失败则返回空串
		if(!reg.sticky)throw new Error;
		reg.lastIndex = this.pos;
		let res = reg.exec(this.script);
		if(!res)return '';
		let str = res[0];
		this.pos += str.length;
		for(let c of str)
			if(c=='\n'){
				this.line++;
				this.col = 0;
			}else{
				this.col++
			}
		return str;
	}
	readNum(startchar){
		if(startchar=='0' && this.substr(1)=='x')
			return startchar + this.match(/x[0-9a-f]+/yi);
		let str = startchar + this.match(/[0-9]*(\.[0-9]+)?/y);
		if(this.script[this.pos]=='e')
			str += this.match(/e-?[0-9]+/y);
		return str;
	}
	readSQStr(){
		//从单引号之后开始读取
		//返回值包含前引号
		let str = this.match(/([^\\\n']|\\.)*'/y);
		if(!str)throw new JuaSyntaxError('Unfinished input');
		return "'" + str;
	}
	readStrTmpl(){
		//从 " 之后开始读取
		let slist=[], tlist=[];
		while(true){
			let [str, token] = this.readDQStr();
			slist.push(str);
			if(token)
				tlist.push(token);
			else
				return new StrTmpl(slist, tlist);
		}
	}
	readDQStr(){
		//从 " 或 } 之后开始读取，到插值表达式或模板结束为止
		//返回[String, Enclosure?]
		//返回的字符串未经过反转义、不含两端双引号、不含插值标志，可能为空
		let str = this.match(/([^\\$"]|\\.)*/y); //可以换行
		if(this.eof())
			throw new JuaSyntaxError('Unfinished input');
		let c = this.script[this.pos];
		this.forward();
		if(c=='"')
			return [str, null];
		if(c!='$')
			throw new Error('Unreachable');
		if(this.eof())
			throw new JuaSyntaxError('Unfinished input');
		c = this.script[this.pos];
		this.forward();
		if(c=='{'){
			let encl = new Enclosure('{}', this.readUntil('}'));
			return [str, encl];
		}else if(validIdStartReg.test(c)){
			let word = c + this.match(/[_a-zA-Z0-9]*/y);
			return [str, new Token('word', word)];
		}else{
			throw new JuaSyntaxError('invalid char: '+c);
		}
	}
}
class ListReader extends TokensReader{
	constructor(tokens){
		super();
		this.tokens = tokens;
		this.pos = 0;
	}
	read(errmsg='Unfinished input'){
		if(this.pos>=this.tokens.length)
			throw new JuaSyntaxError(errmsg);
		return this.tokens[this.pos++];
	}
	preview(){
		if(this.pos>=this.tokens.length)
			return null;
		return this.tokens[this.pos];
	}
}

function parseStatements(reader){
	//总是读完reader
	//忽略空语句
	let stmts = [];
	while(true){
		if(reader.end())
			return stmts;
		let stmt = parseStatement(reader);
		if(stmt)stmts.push(stmt);
	}
}
function parseStatement(reader){
	//保证 reader 推进（除非读完）
	//若为空语句，则返回 0
	//若读完，则返回 null
	let start = reader.preview();
	if(!start)return null;
	if(start.str==';'){
		reader.read();
		return 0;
	}
	if(start.isKeyword)
		switch(start.str){
			case 'return':{
				reader.read();
				let expr = parseExpr(reader); //todo: 省略返回值
				reader.skipStr(';');
				return new Return(expr);
			}
			case 'break':{
				reader.read();
				reader.skipStr(';');
				return new Break;
			}
			case 'continue':{
				reader.read();
				reader.skipStr(';');
				return new Continue;
			}
			case 'let':{
				reader.read();
				let list = parseDecList(reader);
				reader.skipStr(';');
				return new Declaration(list);
			}
			case 'fun':{
				reader.read();
				let name = reader.read();
				if(!name.isValidVarname)throw new JuaSyntaxError('Missing function name'); //不能仅仅是表达式
				let func = parseFunc(reader);
				let left = new Varname(name.str);
				let assignment = new DeclarationItem(left, func);
				return new Declaration(new DeclarationList([assignment]));
			}
			case 'if':{
				reader.read();
				let cond = parseCond(reader);
				let block = parseBlockOrStatement(reader);
				let elseBlock;
				if(reader.previewStr() == 'else'){
					reader.read();
					elseBlock = parseBlockOrStatement(reader);
				}
				return new IfStatement(cond, block, elseBlock);
			}
			case 'switch':{
				reader.read();
				let expr = parseClosedExpr(reader);
				let caseBlocks = [];
				while(true){
					let nextStr = reader.previewStr();
					if(nextStr=='case'){
						reader.read();
						let parened = reader.read();
						if(parened.type != '()')throw new JuaSyntaxError("Missing '('");
						let exprs = parseFlexExprList(parened.reader);
						if(exprs.length==0)
							throw new JuaSyntaxError('Missing caseExpression');
						let block = parseBlockOrStatement(reader);
						caseBlocks.push(new CaseBlock(exprs, block));
					}else if(nextStr=='else'){
						if(caseBlocks.length==0)
							throw new JuaSyntaxError('switch without case');
						reader.read();
						let elseBlock = parseBlockOrStatement(reader);
						return new SwitchStatement(expr, caseBlocks, elseBlock);
					}else{
						if(caseBlocks.length==0)
							throw new JuaSyntaxError('switch without case');
						return new SwitchStatement(expr, caseBlocks);
					}
				}
			}
			case 'while':{
				reader.read();
				let cond = parseCond(reader);
				let block = parseBlockOrStatement(reader);
				return new WhileStatement(cond, block);
			}
			case 'for':{
				reader.read();
				return parseForStmt(reader);
			}
		}
	//表达式语句
	let expr = parseExpr(reader);
	reader.skipStr(';');
	return new ExprStatement(expr);
}
function parseCond(reader){
	let head = reader.read();
	let expr;
	if(head.type=='()'){
		expr = parseExpr(head.reader);
		head.reader.assetEnd();
	}else if(head.str=='!'){
		expr =  new UnitaryExpr('!', parseClosedExpr(reader));
	}else{
		throw new JuaSyntaxError('Unexpected token: '+head);
	}
	return expr;
}
function parseForStmt(reader){
	//从 (...) 开始读取
	let head = reader.read();
	if(head.type != '()')
		throw new JuaSyntaxError("Missing '('");
	let declarable = parseDeclarable(head.reader);
	head.reader.assetStr('in');
	let iterable = parseExpr(head.reader);
	head.reader.assetEnd();
	let block = parseBlockOrStatement(reader);
	return new ForStatement(declarable, iterable, block);
}
function parseBlockOrStatement(reader){ //总是返回Block
	let stmts, next = reader.preview();
	if(next?.type == '{}'){
		reader.read();
		stmts = parseStatements(next.reader);
	}else{ //单条语句
		let stmt = parseStatement(reader);
		if(stmt)
			stmts = [stmt];
		else if(stmt===0)
			stmts = [];
		else
			throw new JuaSyntaxError('Unfinished input');
	}
	return new Block(stmts);
}
function parseClosedExpr(reader, type='()'){ //括号包围的表达式，从 reader 读取一个 Enclosure
	let parened = reader.read();
	if(parened.type != type)
		throw new JuaSyntaxError(`Missing '${type}'`);
	let expr = parseExpr(parened.reader);
	parened.reader.assetEnd();
	return expr;
}
function parseExpr(reader){
	let head, headToken = reader.preview();
	if(headToken.type == '[]'){
		reader.read();
		if(reader.previewStr() == '='){
			let left = parseFlexDecList(headToken.reader);
			reader.read();
			return new Assignment('=', left, parseExpr(reader));
		}
		let arr = new ArrayExpr(parseFlexExprList(headToken.reader));
		head = parsePrimaryTail(arr, reader);
	}else if(headToken.type == '{}'){
		reader.read();
		if(reader.previewStr() == '='){
			let left = parseLeftObj(headToken.reader);
			reader.read();
			return new Assignment('=', left, parseExpr(reader));
		}
		head = parsePrimaryTail(parseObj(headToken.reader), reader);
	}else{
		head = parsePrimary(reader);
	}
	//非解构表达式必定以初等表达式开头
	let next = reader.preview();
	if(next){
		if(assigners.has(next.str)){
			reader.read();
			let expr = parseExpr(reader);
			//todo: 检查 LeftValue
			return new Assignment(next.str, head, expr);
		}
		if(next.isBinop)
			return parseBinExpr(head, reader);
	}
	return head;
}
function parseBinExpr(head, reader){
	let exprstack=[head], operstack=[];
	//exprstack 的长度永远比 operstack 多 1
	//operstack 中为 binOperator 对象，其中的优先级严格递增
	//参见 王道计算机考研 数据结构
	function CombineExpr(priority=0){ //结合优先级不低于priority的运算符
		for(let i=operstack.length-1; i>=0; i--){
			let oper = operstack[i];
			if(oper.priority < priority)break;
			operstack.pop();
			let right = exprstack.pop(),
				left = exprstack.pop(),
				expr = new BinaryExpr(oper.str, left, right);
			exprstack.push(expr);
		}
	}
	while(true){
		let next = reader.preview();
		if(!next?.isBinop)break;
		reader.read();
		let oper = binOperator[next.str];
		CombineExpr(oper.priority);
		operstack.push(oper);
		let pri = parsePrimary(reader);
		//pri.setSource(reader.fileName, nline);
		exprstack.push(pri);
	}
	CombineExpr();
	return exprstack[0];
}
function parsePrimary(reader){ //初等表达式，可以是一元运算符+初等表达式
	let value = reader.read();
	let str = value.str;
	switch(value.type){
		case 'word':
			if(!value.isKeyword)
				return parsePrimaryTail(new Varname(str), reader);
			if(str == 'fun'){ //函数表达式
				let func = parseFunc(reader);
				return parsePrimaryTail(func, reader);
			}else if(str == 'true'){
				return parsePrimaryTail(Keyword.true, reader);
			}else if(str == 'false'){
				return parsePrimaryTail(Keyword.false, reader);
			}else if(str == 'null'){
				return parsePrimaryTail(Keyword.null, reader);
			}else if(str == 'if'){
				let cond = parseCond(reader);
				let expr = parseExpr(reader);
				if(reader.readStr()!='else')throw new JuaSyntaxError("Missing 'else'");
				let elseExpr = parseExpr(reader);
				return new TernaryExpr(cond, expr, elseExpr);
			}else if(str == 'local'){
				return parsePrimaryTail(Keyword.local, reader);
			}
			throw new JuaSyntaxError('Unexpected: '+str);
		case 'literal_num':
			return parsePrimaryTail(LiteralNum.eval(str), reader);
		case 'sq_str':
			return parsePrimaryTail(LiteralStr.eval(str), reader);
		case 'dq_str':{
			let strlist = value.strList.map(str=>eval(`"${str}"`)), //todo: 允许换行？？？
				exprs = [];
			for(let token of value.tokenList){
				if(token.isValidVarname){
					exprs.push(new Varname(token.str));
				}else if(token.type=='{}'){
					exprs.push(parseExpr(token.reader));
					token.reader.assetEnd();
				}
			}
			return new Template(strlist, exprs);
		}
		case 'bq_str':
			return new LiteralStr(value.str.slice(1,-1));
		case '()':{
			let expr = parseExpr(value.reader);
			value.reader.assetEnd();
			return parsePrimaryTail(expr, reader);
		}
		case '[]':{
			let expr;
			if(value.reader.end())
				expr = new ArrayExpr([]);
			else{
				expr = parseFlexExprList(value.reader);
				value.reader.assetEnd();
			}
			return parsePrimaryTail(expr, reader);
		}
		case '{}':{
			let expr = parseObj(value.reader);
			console.log(reader.preview())
			return parsePrimaryTail(expr, reader);
		}
		case 'uniop':
			return new UnitaryExpr(str, parsePrimary(reader));
		default:
			throw new JuaSyntaxError('Unexpected token: '+str);
	}
	
}
function parsePrimaryTail(head, reader){
	//head 为 Expression
	let value = reader.preview();
	if(!value)return head;
	switch(value.type){
		case 'separator':
			if(value.str=='.'){ //属性引用
				reader.read();
				let id = reader.read();
				if(id.type!='word')throw new JuaSyntaxError('expect property name: '+id);
				let expr = new PropRef(head, id.str);
				return parsePrimaryTail(expr, reader);
			}else if(value.str=='?.'){
				reader.read();
				let id = reader.read();
				if(id.type!='word')throw new JuaSyntaxError('expect property name: '+id);
				let expr = new OptionalPropRef(head, id.str);
				return parsePrimaryTail(expr, reader);
			}else if(value.str==':'){ //方法包装
				reader.read();
				let name = reader.read();
				if(name.type!='word')throw new JuaSyntaxError('expect property name: '+name);
				let expr = new MethWrapper(head, name.str);
				return parsePrimaryTail(expr, reader);
			}
			return head;
		case '()': {
			reader.read();
			let next=reader.preview(), expr;
			if(next?.type=='{}'){ //尾随函数
				reader.read();
				let declist = parseFlexDecList(value.reader); //不可空
				if(!value.reader.end())
					throw new JuaSyntaxError('Unexpected token: '+value.reader.readStr());
				let stmts = parseStatements(next.reader);
				let func = new FunExpr(declist, stmts);
				expr = new Call(head, [func]);
			}else{ //普通函数调用
				expr = new Call(head, parseFlexExprList(value.reader));
			}
			return parsePrimaryTail(expr, reader);
		}
		case '[]':{
			reader.read();
			let key = parseExpr(value.reader);
			value.reader.assetEnd();
			let expr = new Subscription(head, key);
			return parsePrimaryTail(expr, reader);
		}
		case '{}':{
			reader.read();
			let stmts = parseStatements(value.reader);
			let func = new FunExpr(new DeclarationList([]), stmts);
			let expr = new Call(head, [func]);
			return parsePrimaryTail(expr, reader);
		}
		case 'sq_str': case 'dq_str': case 'bq_str':
			return new Call(head, [parseExpr(reader)]);
		default:
			return head;
	}
}
function parseFlexExprList(reader){
	//可空，允许尾随逗号
	//读完 reader
	//todo: 返回值需要专门的类
	let list = [], starlist = [], starred = false;
	while(true){
		if(reader.end())
			return list;
		if(reader.previewStr()=='*'){
			starred = true;
			this.read();
			starlist.push(parseExpr(reader));
		}else if(starred){
			throw new JuaSyntaxError("Expect '*'");
		}else{
			list.push(parseExpr(reader));
		}
		if(reader.end())
			return list;
		reader.assetStr(',');
	}
}
function parseFlexDecList(reader){
	//读完 reader，可空
	//返回 DeclarationList
	let items = [];
	while(true){
		//此时刚开始读取或读完上一个逗号
		if(reader.end())
			return new DeclarationList(items);
		if(reader.previewStr()=='*'){
			todo
		}
		items.push(parseDecItem(reader));
		if(reader.end())
			return new DeclarationList(items);
		reader.assetStr(',');
	}
}
function parseDecList(reader){
	//不能包含星号表达式，非空
	let items = [];
	while(true){
		items.push(parseDecItem(reader));
		if(reader.previewStr()==',')
			reader.read();
		else
			return new DeclarationList(items);
	}
}
function parseDeclarable(reader){
	let next = reader.read();
	if(next.isValidVarname)
		return new Varname(next.str);
	if(next.type=='[]')
		return parseFlexDecList(next.reader);
	if(next.type=='{}')
		return parseLeftObj(next.reader);
	throw new JuaSyntaxError;
}
function parseDecItem(reader){
	let declarable = parseDeclarable(reader),
		defval = null,
		auto_null = false,
		next = reader.previewStr();
	if(next=='?'){
		reader.read();
		auto_null = true;
	}else if(next=='='){
		reader.read();
		defval = parseExpr(reader);
	}
	let item = new DeclarationItem(declarable, defval); //todo: src
	if(auto_null)item.addDefault();
	return item;
}
function parseObj(reader){
	//读完 reader
	//返回ObjExpr
	let entries = [];
	while(true){
		//此时刚开始读取或读完上一个逗号
		if(reader.end()){ //允许尾随逗号
			return new ObjExpr(entries);
		}
		entries.push(parseProp(reader));
		if(reader.end())
			return new ObjExpr(entries);
		reader.assetStr(',');
	}
}
function parseProp(reader){
	let key, val;
	let start = reader.read();
	if(start.type=='word'){
		key = new LiteralStr(start.str);
		let next = reader.preview();
		if(next?.str == '='){
			reader.read();
			val = parseExpr(reader);
		}else if(next?.type == '()'){
			val = parseFunc(reader);
		}else if(start.isValidVarname){
			val = new Varname(start.str);
		}else{
			throw new JuaSyntaxError;
		}
	}else if(start.type=='[]'){
		key = parseExpr(start.reader);
		start.reader.assetEnd();
		let next = reader.preview();
		if(!next)
			throw new JuaSyntaxError;
		if(next.str=='='){
			reader.read();
			val = parseExpr(reader);
		}else if(next.type=='()'){
			val = parseFunc(reader);
		}else{
			throw new JuaSyntaxError('Unexpect token: '+next.str);
		}
	}
	return [key, val];
}
function parseLeftObj(reader){
	//不可空，允许尾随逗号
	//读完 reader
	let entries = [];
	while(true){
		if(reader.end()){
			if(!entries.length)
				throw new JuaSyntaxError('LeftObj cannot be empty');
			return new LeftObj(entries);
		}
		entries.push(parseLeftProp(reader));
		if(reader.end())
			return new LeftObj(entries);
		reader.assetStr(',');
	}
}
function parseLeftProp(reader){
	let key, decItem;
	let next = reader.read();
	if(next.type=='word'){
		let name = next.str;
		key = new LiteralStr(name);
		next = reader.preview();
		if(next){
			if(next.str=='?'){
				reader.read();
				decItem = new DeclarationItem(new Varname(name), Keyword.null);
			}else if(next.str=='='){
				reader.read();
				decItem = new DeclarationItem(new Varname(name), parseExpr(reader));
			}else if(next.str=='as'){
				reader.read();
				decItem = parseDecItem(reader);
			}else{
				decItem = new DeclarationItem(new Varname(name));
			}
		}else{
			decItem = new DeclarationItem(new Varname(name));
		}
	}else if(next.type=='[]'){
		key = parseExpr(next.reader);
		next.reader.assetEnd();
		reader.assetStr('as');
		decItem = parseDecItem(reader);
	}else{
		throw new JuaSyntaxError('Unexpected token: '+next.str);
	}
	return [key, decItem];
}
function parseFunc(reader){
	//从 Enclosure 开始读取
	let args = reader.read();
	if(args.type != '()')
		throw new JuaSyntaxError("Missing '('");
	let decList;
	if(args.reader.end()){
		decList = new DeclarationList([]);
	}else{
		decList = parseFlexDecList(args.reader);
		args.reader.assetEnd();
	}
	let stmts;
	let next = reader.read();
	if(next.type == '{}'){
		stmts = parseStatements(next.reader);
	}else if(next.str == '='){
		let expr = parseExpr(reader);
		stmts = [new Return(expr)];
	}else{
		throw new JuaSyntaxError(next.str);
	}
	return new FunExpr(decList, stmts);
}

export default function parse(script, {fileName}={}){
	let reader = new ScriptReader(script, fileName);
	let statements = parseStatements(reader);
	if(globalThis.JUA_DEBUG)console.log('statements', statements)
	return new FunctionBody(statements);
}