//根据DeepSeek的说法，这是"内聚式AST"或"自执行AST"
//AST是静态的，所有节点均不应保存运行时的状态
import {
	Scope, Jua_Val, Jua_Null, Jua_Num, Jua_Str, Jua_Bool, Jua_Obj, Jua_Func, Jua_NativeFunc, Jua_Array,
	TrackInfo, JuaError, JuaSyntaxError, JuaRefError, JuaTypeError
} from 'jua/value';
import {uniOperator, binOperator} from 'jua/operator';

function Get2Operator(type){
	let oper = binOperator[type];
	if(!oper)throw new JuaSyntaxError('no operator: '+type);
	return oper;
}

//todo: 错误捕获以表达式为单位（或某些语句）
//primary setSource 时需要对子表达式进行 copySrc
//捕获者不对错误进行二次包装，而是记录错误栈（或许应当从前往后而不是从后往前记录）
//不捕获子表达式计算过程产生的错误，否则错误栈会十分冗长。仅捕获最终计算产生的错误
class Expression{ //抽象类
	str = null; //用于word 需要吗？？？
	setSource(file, n){
		this.src = new TrackInfo(this, file, n);
	}
	copySrc(expr){
		if(this.src)
			return;
		let {fileName, lineNumber} = expr.src;
		this.setSource(fileName, lineNumber);
	}
	toString(){
		return this.constructor.name;
	}
	calc(env){ //返回jua值；需要追踪
		throw new JuaSyntaxError('uncalculatable: '+this.constructor.name, {cause:this});
	}
}
class LeftValue{ //抽象类
	assign(env, val){
		throw new Error("pure virtual function");
	}
}
class Declarable extends LeftValue{
	//抽象类，直接用于 DeclarationItem，间接用于变量声明、函数参数、左值数组、for循环；一定是左值
	addDefault(){
		//仅用于解构左值
	}
	declare(env, val){ //val非空
		throw 'pure virtual function';
	}
}
class DeclarationItem{ //不是Declarable
	constructor(declarable, defval=null){
		this.declarable = declarable;
		this.defval = defval;
	}
	addDefault(){
		this.defval ||= Keyword.null;
		this.declarable.addDefault();
	}
	declare(env, arg){ //用于变量声明时，arg必空；其他情况arg可空。调用前检查默认值。
		this.declarable.declare(env, arg||this.defval.calc(env));
	}
	assign(env, val){ //目前仅用于左值数组，val可空。调用前检查默认值。
		this.declarable.assign(env, val||this.defval.calc(env));
	}
}
class DeclarationList extends Declarable{
	//todo: *args
	//3种作用：变量声明、函数参数、左值数组
	static fromNames(names){
		return new DeclarationList(names.map(name=>{
			let id = new Varname(name);
			return new DeclarationItem(id);
		}));
	}
	constructor(decItems, restArgname=''){
		super();
		this.decItems = decItems;
		this.restArgname = restArgname; //todo
	}
	toVarDecList(){
		for(let item of this.decItems)
			item.defval ||= Keyword.null;
		return this;
	}
	assign(env, list){ //仅用于左值数组，list非空
		this.forEach(env, list[Symbol.iterator](), (item, val)=>item.assign(env, val));
	}
	declare(env, list){ //仅用于左值数组，list非空
		this.forEach(env, list[Symbol.iterator](), (item, val)=>item.declare(env, val));
	}
	rawDeclare(env, args=[]){ //用于变量声明、函数参数
		function tarckDeclare(decItem, val){
			return env.trackExpr(decItem, ()=>{
				if(val || decItem.defval)
					decItem.declare(env, val);
				else
					throw new JuaTypeError('Missing argument: '+decItem.declarable, {cause:decItem});
			});
		}
		this.forEach(env, args[Symbol.iterator](), tarckDeclare);
	}
	forEach(env, iterator, fn){ //iterator instanceof JuaIterator
		//对每个DeclarationItem和取得的值调用 fn(item, val)
		for(let item of this.decItems){
			let {value, done} = iterator.next();
			fn(item, done ? null : value);
		}
	}
}
class LiteralNum extends Expression{
	static eval(str){
		return new this(eval(str));
	}
	constructor(val){
		super();
		this.val = val;
	}
	calc(env){
		return new Jua_Num(this.val);
	}
}
class LiteralStr extends Expression{
	static eval(str){
		return new this(eval(str));
	}
	constructor(val){
		super();
		this.val = val;
	}
	calc(env){
		return new Jua_Str(this.val);
	}
}
class Template extends Expression{
	constructor(strlist, exprlist){
		super();
		this.strlist = strlist; //经过反转义
		this.exprlist = exprlist;
	}
	calc(env){
		let strlist = [this.strlist[0]];
		for(let i=0; i<this.exprlist.length; i++){
			let val = this.exprlist[i].calc(env);
			strlist.push(val.toString());
			strlist.push(this.strlist[i+1]);
		}
		return new Jua_Str(strlist.join(''));
	}
}
class Keyword extends Expression{
	constructor(str){
		super();
		this.str = str;
	}
	calc(env){
		switch(this.str){
			case 'true': return Jua_Bool.true;
			case 'false': return Jua_Bool.false;
			case 'null': return Jua_Null.inst;
			case 'local': return env;
			default: throw new Error(this.str); //不应该
		}
	}
	static true = new this('true');
	static false = new this('false');
	static null = new this('null');
	static local = new this('local');
}
class Varname extends Expression{ //Declarable
	constructor(identifier){
		super();
		this.str = identifier;
	}
	toString(){
		return `Varname('${this.str}')`;
	}
	calc(env){
		let val = env.getProp(this.str);
		return env.trackExpr(this, ()=>{
			if(val)
				return val;
			throw new JuaRefError(`Variable '${this.str}' is not declared`, {
				obj: env,
				prop: this.str
			});
		});
	}
	assign(env, val){
		env.assign(this.str, val);
	}
	declare(env, val){
		env.setProp(this.str, val)
	}
	addDefault(){} //dummy
}
class OptionalPropRef extends Expression{ //LeftValue
	constructor(expr, prop){
		//prop为js字符串
		super();
		this.expr = expr;
		this.prop = prop;
	}
	setSource(...args){
		super.setSource(...args);
		this.expr.copySrc(this);
	}
	_calc(env){
		return this.expr.calc(env).getProp(this.prop);
	}
	calc(env){
		return this._calc(env) || Jua_Null.inst;
	}
}
class PropRef extends OptionalPropRef{
	calc(env){
		let val = this._calc(env);
		return env.trackExpr(this, ()=>{
			if(val)
				return val;
			throw new JuaRefError('no property: '+this.prop);
		});
	}
	assign(env, val){
		this.expr.calc(env).setProp(this.prop, val);
	}
}
class MethWrapper extends Expression{
	constructor(expr, method){
		//method为js字符串
		super();
		this.expr = expr;
		this.method = method;
	}
	setSource(...args){
		super.setSource(...args);
		this.expr.copySrc(this);
	}
	calc(env){
		let obj = this.expr.calc(env);
		let meth = obj.getProp(this.method);
		return env.trackExpr(this, ()=>{
			if(!meth)throw new JuaRefError('missing method: '+this.method);
			return new Jua_NativeFunc((...args) => meth.call([obj, ...args]));
		});
	}
}
class UnitaryExpr extends Expression{
	constructor(type, expr){
		super();
		this.type = type;
		this.expr = expr;
	}
	setSource(...args){
		super.setSource(...args);
		this.expr.copySrc(this);
	}
	calc(env){
		let operator = uniOperator[this.type];
		if(!operator)throw new JuaSyntaxError('no operator: '+this.type);
		let val = this.expr.calc(env);
		return env.trackExpr(this, ()=>operator(val));
	}
}
class BinaryExpr extends Expression{
	constructor(type, left, right){
		super();
		this.type = type;
		this.left = left;
		this.right = right;
		//this.operator = Get2Operator(type)
	}
	setSource(...args){
		super.setSource(...args);
		this.left.copySrc(this);
		this.right.copySrc(this);
	}
	calc(env){
		let operator = Get2Operator(this.type);
		if(operator.circuited) //若子表达式无误，则短路运算无误，因此无需追踪
			return operator.fn(env, this.left, this.right);
		let lv = this.left.calc(env), rv = this.right.calc(env);
		return env.trackExpr(this, ()=>operator.fn(lv, rv));
	}
}
class Assignment extends Expression{
	//不算 BinaryExpr，因为 left 不一定是表达式
	constructor(type, left, right){
		super();
		//left 为左值
		this.type = type;
		this.left = left;
		this.right = right;
		this.operator = type=='=' ? null : Get2Operator(type.slice(0,-1));
	}
	calc(env){
		if(this.operator){
			let {circuited, fn} = this.operator;
			if(circuited){
				//todo: 短路时不进行赋值
				let val = fn(env, this.left, this.right);
				return env.trackExpr(this, ()=>{
					this.left.assign(env, val);
					return val;
				});
			}else{
				let lv = this.left.calc(env), rv = this.right.calc(env);
				return env.trackExpr(this, ()=>{
					let val = fn(lv, rv);
					this.left.assign(env, val);
					return val;
				});
			}
		}else{
			let val = this.right.calc(env);
			return env.trackExpr(this, ()=>{
				this.left.assign(env, val);
				return val;
			});
		}
	}
}
class Subscription extends BinaryExpr{ //LeftValue
	constructor(expr, key){
		super('subscript', expr, key);
	}
	calc(env){
		let obj = this.left.calc(env);
		let key = this.right.calc(env);
		return env.trackExpr(this, ()=>obj.getItem(key));
	}
	assign(env, val){
		let obj = this.left.calc(env);
		let key = this.right.calc(env);
		obj.setItem(key, val||Jua_Null.inst);
	}
}
class TernaryExpr extends Expression{ //if(cond) v1 else v2
	constructor(cond, expr, elseExpr){
		super();
		this.cond = cond;
		this.expr = expr;
		this.elseExpr = elseExpr;
	}
	setSource(...args){
		super.setSource(...args);
		this.cond.copySrc(this);
		this.expr.copySrc(this);
		this.elseExpr.copySrc(this);
	}
	calc(env){
		if(this.cond.calc(env).toBoolean())
			return this.expr.calc(env);
		else
			return this.elseExpr.calc(env);
	}
}
class FlexibleList{
	//todo
}
class Call extends Expression{
	constructor(callee, args){
		super();
		this.callee = callee;
		this.args = args;
		//todo: starred
	}
	setSource(...args){
		super.setSource(...args);
		this.callee.copySrc(this);
		//args 在 parseExpr 时已 setSource
	}
	calc(env){
		let fn = this.callee.calc(env);
		let args = this.args.map(expr=>expr.calc(env));
		return env.trackExpr(this, ()=>fn.call(args));
	}
}
class ObjExpr extends Expression{
	constructor(entries){
		super();
		this.entries = entries;
	}
	calc(env){
		let obj = new Jua_Obj;
		for(let kv of this.entries){
			let [key, val] = kv.map(e=>e.calc(env));
			if(!(key instanceof Jua_Str))
				env.trackExpr(this, ()=>{
					throw new JuaTypeError('non-string: '+key);
				});
			obj.setProp(key.value, val);
		}
		return obj;
	}
}
class LeftObj extends Declarable{
	auto_nulled = false;
	constructor(entries){
		super();
		this.entries = entries; //每一项均为[key:右值, DeclarationItem]
	}
	addDefault(){
		if(this.auto_nulled)
			return;
		for(let [_, item] of this.entries){
			item.addDefault();
		}
		this.auto_nulled = true;
	}
	forEach(env, obj, fn){
		//obj可以不是Jua_Obj
		//对每个声明项和取得的目标属性调用 fn(decItem, val)
		//若既没有目标属性也没有默认值则 JuaRefError
		for(let [keyExpr, decItem] of this.entries){
			let key = keyExpr.calc(env);
			if(!(key instanceof Jua_Str))
				throw new JuaTypeError('non-string: '+key);
			let val = obj.getProp(key.value);
			if(val || decItem.defval)
				fn(decItem, val);
			else
				throw new JuaRefError('cannot read property: '+key.value);
		}
	}
	assign(env, obj){
		this.forEach(env, obj, (item, val)=>item.assign(env, val));
	}
	declare(env, obj){
		this.forEach(env, obj, (item, val)=>item.declare(env, val));
	}
}
class ArrayExpr extends Expression{
	constructor(exprs){
		super();
		this.exprs = exprs;
	}
	calc(env){
		//todo: StarExpr
		let vals = this.exprs.map(e=>e.calc(env));
		return new Jua_Array(vals);
	}
}
class FunExpr extends Expression{
	constructor(decList, stmts){
		super();
		this.decList = decList; //DeclarationList
		this.stmts = stmts;
	}
	calc(env){
		let body = new FunctionBody(this.stmts);
		return new Jua_PFunc(env, this.decList, body);
	}
}
//复合语句的构造函数需要检查 pending_continue, pending_break
class Statement{
	pending_continue = null;
	pending_break = null;
	constructor(type){
		this.type = type; //或许不需要
	}
	exec(env, controller){
		throw 'pure virtual function';
	}
}
class ExprStatement extends Statement{
	constructor(expr){
		super('expr');
		this.expr = expr;
	}
	exec(env){
		this.expr.calc(env);
	}
}
class Declaration extends Statement{
	constructor(list){ //list为 DeclarationList
		super('let');
		this.list = list.toVarDecList();
	}
	exec(env){
		this.list.rawDeclare(env);
	}
}
class Return extends Statement{
	constructor(expr){
		super('return');
		this.expr = expr; //可空
	}
	exec(env, controller){
		if(this.expr)
			controller.return(this.expr.calc(env));
		else
			controller.return(Jua_Null.inst);
	}
}
class Break extends Statement{
	constructor(){
		super('break');
		this.pending_break = this;
	}
	exec(_, controller){
		controller.break();
	}
}
class Continue extends Statement{
	constructor(){
		super('continue');
		this.pending_continue = this;
	}
	exec(_, controller){
		controller.continue();
	}
}
class IfStatement extends Statement{
	constructor(cond, block, elseBlock){ //elseBlock可为假
		super('if');
		this.cond = cond;
		this.block = block;
		this.elseBlock = elseBlock; //可空；else if... 等价于 else{ if... }
		this.pending_continue = block.pending_continue || elseBlock?.pending_continue || null;
		this.pending_break = block.pending_break || elseBlock?.pending_break || null;
	}
	exec(env, controller){
		if(this.cond.calc(env).toBoolean())
			this.block.exec(new Scope(env), controller);
		else
			this.elseBlock?.exec(new Scope(env), controller);
	}
}
class SwitchStatement extends Statement{
	constructor(expr, caseBlocks, elseBlock){
		super();
		this.expr = expr;
		this.caseBlocks = caseBlocks;
		this.elseBlock = elseBlock; //可空
		let blocks = caseBlocks.map(cb=>cb.block);
		if(elseBlock)
			blocks.push(elseBlock);
		for(let block of blocks){
			this.pending_continue ||= block.pending_continue;
			this.pending_break ||= block.pending_break;
		}
	}
	exec(env, controller){
		let val = this.expr.calc(env);
		for(let caseblock of this.caseBlocks)
			if(caseblock.match(env, val)){
				caseblock.block.exec(new Scope(env), controller);
				return;
			}
		this.elseBlock?.exec(new Scope(env), controller);
	}
}
class CaseBlock{
	constructor(exprs, block){
		this.exprs = exprs;
		this.block = block;
	}
	match(env, val){
		for(let expr of this.exprs){
			let cval = expr.calc(env);
			if(val.equalTo(cval))
				return true;
		}
	}
}
class WhileStatement extends Statement{
	constructor(cond, block){
		super('while');
		this.cond = cond;
		this.block = block;
	}
	exec(env, controller){
		while(this.cond.calc(env).toBoolean()){
			this.block.exec(new Scope(env), controller);
			controller.resolve('continue');
			if('break' in controller.pending){
				controller.resolve('break');
				break;
			}
		}
	}
}
class ForStatement extends Statement{
	constructor(declarable, iterable, block){
		super('for');
		this.declarable = declarable;
		this.iterable = iterable;
		this.block = block;
	}
	exec(env, controller){
		let iterable = this.iterable.calc(env);
		let iterator;
		env.trackExpr(this.iterable, ()=>{
			iterator = iterable[Symbol.iterator]();
		});
		while(true){
			let {value, done} = env.trackExpr(this.iterable, ()=>iterator.next());
			if(done)break;
			let local = new Scope(env);
			env.trackExpr(this.declarable, ()=>this.declarable.declare(local, value));
			this.block.exec(local, controller);
			controller.resolve('continue');
			if('break' in controller.pending){
				controller.resolve('break');
				break;
			}
		}
	}
}

class Block{
	pending_continue = null;
	pending_break = null;
	constructor(statements){
		this.statements = statements;
		for(let stmt of statements){
			this.pending_continue ||= stmt.pending_continue;
			this.pending_break ||= stmt.pending_break;
		}
	}
	exec(env, controller){ //env是新产生的作用域
		for(let stmt of this.statements)
			if(controller.isPending) break;
			else stmt.exec(env, controller);
		//todo: 垃圾回收
	}
}
class FunctionBody extends Block{
	constructor(statements){
		super(statements);
		let pending_expr = this.pending_continue || this.pending_break;
		if(pending_expr)
			throw JuaSyntaxError('Invalid statement', {cause:pending_expr});
	}
	exec(env){
		//返回jua值
		let controller = new Controller;
		super.exec(env, controller);
		let res = controller.pending.return;
		controller.resolve('return');
		if(controller.isPending)
			throw new Error('怎么会？'); //应该在语法阶段检测
		return res || Jua_Null.inst;
	}
}
class Jua_PFunc extends Jua_Func{
	constructor(upenv, decList, body){
		super();
		this.upenv = upenv;
		this.decList = decList;
		this.body = body;
	}
	call(args=[]){
		let env = new Scope(this.upenv);
		this.decList.rawDeclare(env, args);
		return this.body.exec(env);
	}
}
class Controller{
	pending = Object.create(null);
	get isPending(){
		for(let _ in this.pending)
			return true;
		return false;
	}
	continue(){
		this.pending.continue = true;
	}
	break(){
		this.pending.break = true;
	}
	return(value){
		this.pending.return = value;
	}
	resolve(key){
		delete this.pending[key];
	}
}

export {
	LiteralNum, LiteralStr, Template, Keyword, Varname,
	OptionalPropRef, PropRef, MethWrapper, Subscription, Call, ObjExpr, ArrayExpr, FunExpr,
	Assignment, UnitaryExpr, BinaryExpr, TernaryExpr,
	Declarable, DeclarationItem, DeclarationList, LeftObj,
	ExprStatement, Declaration, Return, Break, Continue, IfStatement, SwitchStatement, CaseBlock, WhileStatement, ForStatement,
	Block, FunctionBody, Jua_PFunc
};