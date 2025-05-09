class Jua_Val{
	id = -1;
	constructor(type, proto){
		this.type = type; //'number', 'string', 'boolean', 'function', 'object'
		this.proto = proto || null; //若为真，则必为 Jua_Obj
	}
	getOwn(key){ //返回jua值或null
		return null;
	}
	inheritProp(key){ //返回jua值或null
		if(!this.proto)return null;
		if(this.proto.isPropTrue('__class'))
			return this.getOwn('super')?.getProp(key);
		return this.proto.getProp(key);
	}
	getProp(key){ //返回jua值或null
		return this.getOwn(key) || this.inheritProp(key);
	}
	setProp(key){
		throw new JuaTypeError('cannot setProp: '+this.type);
	}
	isInst(klass){
		if(!this.proto)return false;
		return this.proto == klass || this.proto.isSubclass(klass);
	}
	isSubclass(klass){ //不能相同
		if(!this.proto)return false;
		if(!this.proto.isPropTrue('__class'))return false;
		let base = this.getOwn('super');
		if(!base)return false;
		return base == klass || base.isSubclass(klass);
	}
	getMetaMethod(key){ //返回Jua_Func或null
		if(!this.proto)return null;
		let meth = this.proto.getProp(key);
		return meth instanceof Jua_Func ? meth : null;
	}
	//基类调用元方法，内置元方法调用子类方法
	hasItem(key){
		let hasFn = this.getMetaMethod('hasItem');
		if(hasFn)
			return hasFn.call([this, key]);
		throw 'Unable to hasItem: '+this;
	}
	getItem(key){ //key为jua值；总是返回jua值
		let getFn = this.getMetaMethod('getItem');
		if(getFn)
			return getFn.call([this, key]);
		throw 'Unable to getItem: '+this;
	}
	setItem(key, val){
		let setFn = this.getMetaMethod('setItem');
		if(setFn)
			return setFn.call([this, key, val]);
		throw 'Unable to setItem: '+this;
	}
	call(args){
		let callFn = this.getMetaMethod('__call');
		if(callFn)
			return callFn.call([this, ...args])
		throw 'Unable to call '+this;
	}
	add(val){
		let addFn = this.getMetaMethod('__add')||val.getMetaMethod('__add');
		if(addFn)
			return addFn.call([this, val]);
		throw 'Unable to add '+this;
	}
	sub(val){
		let subFn = this.getMetaMethod('__sub')||val.getMetaMethod('__sub');
		if(subFn)
			return subFn.call([this, val]);
		throw 'Unable to sub '+this;
	}
	mul(val){
		let mulFn = this.getMetaMethod('__mul')||val.getMetaMethod('__mul');
		if(mulFn)
			return mulFn.call([this, val]);
		throw 'Unable to mul '+this;
	}
	div(val){
		let divFn = this.getMetaMethod('__div')||val.getMetaMethod('__div');
		if(divFn)
			return divFn.call([this, val]);
		throw 'Unable to div '+this;
	}
	eq(val){ //保证成功；子类应当重写 equalTo
		return this.equalTo(val) ? Jua_Bool.true : Jua_Bool.false;
	}
	lt(val){
		let ltFn = this.getMetaMethod('__lt');
		if(ltFn)
			return ltFn.call([this, val]);
		throw 'Unable to lt '+this;
	}
	le(val){
		let leFn = this.getMetaMethod('__le');
		if(leFn)
			return leFn.call([this, val]);
		throw 'Unable to le '+this;
	}
	range(val){
		let rangeFn = this.getMetaMethod('range');
		if(rangeFn)
			return rangeFn.call([this, val]);
		throw 'Unable to range';
	}
	[Symbol.iterator](){ //返回JuaIterator，可能报错
		return new CustomIterator(this);
	}
	toJSON(){ //返回js值，可能报错
		throw new JuaTypeError('not JSON serializable');
	}
	//以下返回js值且保证成功（除非执行元方法过程出错）
	equalTo(val){
		let eqFn = this.getMetaMethod('__eq');
		if(eqFn)
			return eqFn.call([this, val]).toBoolean();
		return this == val;
	}
	toString(){
		throw new Error('pure virtual function');
	}
	toBoolean(){
		return true;
	}
	toInt(){ //todo: 仅用于数字
		return 0;
	}
}

class Jua_Null extends Jua_Val{
	constructor(){ //私有构造函数
		super('null');
	}
	toString(){
		return 'null';
	}
	toBoolean(){
		return false;
	}
	static inst = new this;
	toJSON(){
		return null;
	}
}
class Jua_Obj extends Jua_Val{
	static proto = new this;
	dict = Object.create(null);
	constructor(proto, source={}){ //source的属性为jua值
		super('object', proto);
		for(let k in source)this.setProp(k, source[k]);
	}
	//key为js字符串
	hasOwn(key){ //返回js值
		return key in this.dict;
	}
	getOwn(key){
		return this.dict[key] || null;
	}
	setProp(key, val){ //val必须为jua值
		this.dict[key] = val;
	}
	delProp(key){
		delete this.dict[key];
	}
	hasItem(key){
		let hasFn = this.getMetaMethod('hasItem');
		if(hasFn)
			return hasFn.call([this, key]);
		if(key.type!='string')
			throw 'Expect string as key for ordinary object';
		return this.getProp(key.toString()) ? Jua_Bool.true : Jua_Bool.false;
	}
	getItem(key){
		let getFn = this.getMetaMethod('getItem');
		if(getFn)
			return getFn.call([this, key]);
		if(!(key.type=='string'))
			throw new JuaTypeError('key must be a string');
		let val = this.getProp(key.value);
		if(!val)
			throw new JuaRefError(this+' has no property: '+key.value);
		return val;
	}
	setItem(key, val){
		let setFn = this.getMetaMethod('setItem');
		if(setFn)
			return setFn.call([this, key, val]);
		if(key.type=='string')
			this.setProp(key.value, val);
		else
			throw new JuaTypeError('key must be a string');
	}
	[Symbol.iterator](){
		let iterFn = this.getMetaMethod('iter'); // || Jua_Obj.proto.getOwn('iter') 不建议使用，性能较低
		if(iterFn)
			return new CustomIterator(this, iterFn);
		let list = [];
		for(let k in this.dict){
			list.push(new Jua_Str(k));
		}
		return new ListIterator(list);
	}
	isPropTrue(key){ //仅自身属性
		return this.getOwn(key) == Jua_Bool.true;
	}
	assignProps(obj){ //obj为Jua_Obj
		for(let k in obj.dict)
			this.dict[k] = obj.dict[k];
	}
	toString(){
		let toString = this.getMetaMethod('toString');
		if(toString){
			let res = toString.call([this]);
			if(res instanceof Jua_Str)
				return res.value;
		}
		return `{${Object.keys(this.dict).join(', ')}}`;
	}
	toJSON(){
		//todo: 覆盖
		let toJSON = this.getMetaMethod('toJSON');
		if(toJSON)
			return toJSON.call([this]).toJSON();
		return this.dict;
	}
}
class Jua_Bool extends Jua_Val{
	static proto = new Jua_Obj;
	constructor(value=false){ //私有构造函数
		super('boolean', Jua_Bool.proto);
		this.value=value;
	}
	toString(){
		return String(this.value);
	}
	toBoolean(){
		return this.value;
	}
	toJSON(){
		return this.value;
	}
	static true = new this(true);
	static false = new this(false);
}
class Jua_Num extends Jua_Val{
	static proto = new Jua_Obj;
	static rangeProto = new Jua_Obj;
	static NaN = new Jua_Num(NaN);
	constructor(value=0){
		super('number', Jua_Num.proto);
		this.value=value;
	}
	add(val){
		if(!(val instanceof Jua_Num))
			throw val+' is not a number';
		return new Jua_Num(this.value + val.value);
	}
	sub(val){
		if(!(val instanceof Jua_Num))
			throw val+' is not a number';
		return new Jua_Num(this.value - val.value);
	}
	mul(val){
		if(!(val instanceof Jua_Num))
			throw val+' is not a number';
		return new Jua_Num(this.value * val.value);
	}
	div(val){
		if(!(val instanceof Jua_Num))
			throw val+' is not a number';
		return new Jua_Num(this.value / val.value);
	}
	lt(val){
		if(!(val instanceof Jua_Num))
			throw val+' is not a number';
		return this.value < val.value ? Jua_Bool.true : Jua_Bool.false;
	}
	le(val){
		if(!(val instanceof Jua_Num))
			throw val+' is not a number';
		return this.value <= val.value ? Jua_Bool.true : Jua_Bool.false;
	}
	range(val){
		if(!(val instanceof Jua_Num))
			throw val+' is not a number';
		let iter = new Jua_Obj(Jua_Num.rangeProto);
		iter.setProp('start', this);
		iter.setProp('end', val);
		return iter;
	}
	equalTo(val){
		return val instanceof Jua_Num && this.value == val.value;
	}
	toString(){
		return String(this.value);
	}
	toBoolean(){
		return Boolean(this.value);
	}
	toInt(){
		return Math.round(this.value);
	}
	toJSON(){
		return this.value;
	}
}
class Jua_Str extends Jua_Val{
	//严格按照规范的话，应当使用 Uint8Array
	//进行二进制操作时，每个utf16码元看作一个字节
	static proto = new Jua_Obj;
	constructor(value=''){
		super('string', Jua_Str.proto);
		this.value=value;
	}
	correctIndex(i){ //i为jua值
		i = i.toInt() % this.value.length;
		if(i < 0)
			i += this.value.length;
		return i;
	}
	hasItem(substr){
		if(substr.type!='string')
			return Jua_Bool.false;
		return Jua_Bool[this.value.includes(substr.value)];
	}
	getItem(index){
		let c = this.value.at(index.toInt()); //todo: 限制范围
		if(c)return new Jua_Str(c);
		return Jua_Null.inst;
	}
	add(val){
		if(val.type!='string')
			throw 'expect string';
		return new Jua_Str(this.toString()+val.toString());
	}
	slice(start, end){
		start = start ? this.correctIndex(start) : 0;
		end = end && this.correctIndex(end) || this.value.length;
		return new Jua_Str(this.value.slice(start, end));
	}
	equalTo(val){
		return val instanceof Jua_Str && this.value == val.value;
	}
	toString(){
		return this.value;
	}
	toBoolean(){
		return this.value=='' ? false : true;
	}
	toJSON(){
		return this.value;
	}
}
class Jua_Func extends Jua_Val{
	static proto = new Jua_Obj;
	constructor(){
		super('function', Jua_Func.proto);
	}
	call(args=[]){ //总是返回jua值
		throw 'pure virtual function';
	}
	toString(){
		return '<function>';
	}
	toJSON(){
		return undefined;
	}
}

class Scope extends Jua_Obj{
	constructor(parent, trace_stack){
		super();
		this.parent = parent || null; //若为真，则必为 Scope
		this.trace_stack = trace_stack || parent.trace_stack; //必为真？
	}
	inheritProp(key){
		return this.parent?.getProp(key);
	}
	assign(name, val){ //val必须为jua值
		for(let env=this; env; env=env.parent)
			if(env.hasOwn(name)){
				env.setProp(name, val);
				return;
			}
		throw 'Undeclared variable: '+name;
	}
	trackFunc(name){
		//暂时无用
	}
	trackExpr(expr, unsafe_fn){
		this.trace_stack.push(expr.src||new TrackInfo(expr));
		let res;
		try{
			res = unsafe_fn();
		}catch(err){
			if(err instanceof JuaError)
				err.track(this.trace_stack);
			this.trace_stack.pop();
			throw err;
		}
		this.trace_stack.pop();
		return res;
	}
}
class Jua_NativeFunc extends Jua_Func{ //应处于全局环境
	constructor(native){
		//native: (...Jua_Val) -> Jua_Val?
		super();
		this.native = native;
	}
	call(args=[]){
		return this.native(...args) || Jua_Null.inst;
	}
}
class Jua_Array extends Jua_Obj{
	static proto = new Jua_Obj;
	constructor(items=[]){
		super(Jua_Array.proto);
		this.items = items;
	}
	[Symbol.iterator](){
		return new ListIterator(this.items);
	}
	correctIndex(i){ //i为jua值
		i = i.toInt() % this.items.length;
		if(i < 0)
			i += this.items.length;
		return i;
	}
	//todo: hasItem
	getItem(key){
		let i = this.correctIndex(key);
		return this.items[i];
	}
	setItem(key, val){ //不允许空槽，不改变长度
		let i = this.correctIndex(key);
		this.items[i] = val;
	}
	toJSON(){
		return this.items;
	}
}
class Jua_Buffer extends Jua_Obj{ //不推荐在js实现中使用，性能很低
	static proto = new Jua_Obj;
	constructor(length){
		super(Jua_Buffer.proto);
		this.bytes = new Uint8Array(length);
		this.setProp('length', new Jua_Num(length));
	}
	getItem(key){
		let i = key.toInt();
		return new Jua_Num(this.bytes.at(i));
	}
	setItem(key, val){
		if(!(val instanceof Jua_Num))
			throw 'value must be a number';
		let i = this.correctIndex(key);
		this.bytes[i] = val.value;
	}
	read(start, end){
		start = this.correctIndex(start);
		end = this.correctIndex(end);
		let bytes = this.bytes.slice(start, end || this.bytes.length);
		let str = String.fromCharCode(...bytes);
		return new Jua_Str(str);
	}
	write(str, pos){
		if(!(str instanceof Jua_Str))
			throw 'expect string';
		pos = this.correctIndex(pos);
		let len = str.value.length;
		if(len+pos > this.bytes.length)
			throw 'out of range';
		let bytes = new Uint8Array(len);
		for(let i=0; i<len; i++){
			bytes[i] = str.value.charCodeAt(i);
		}
		this.bytes.set(bytes, pos);
	}
	correctIndex(i){ //i为jua值或空
		if(!i)return 0;
		i = i.toInt() % this.bytes.length;
		if(i < 0)
			i += this.bytes.length;
		return i;
	}
}

//以下非Jua_Val
class TrackInfo{
	constructor(expr, file='', n=''){
		this.expr = expr;
		this.name = expr.constructor.name;
		this.fileName = String(file);
		this.lineNumber = String(n);
	}
	toJuaObj(){
		let obj = new Jua_Obj;
		obj.setProp('fileName', this.fileName);
		obj.setProp('lineNumber', this.lineNumber);
		return obj;
	}
	toString(){
		return `${this.name||'<unknown expression>'} (${this.fileName||'<unknown file>'}:${this.lineNumber||'<unknown line>'})`;
	}
}
const errorProtos = {
	Error: new Jua_Obj,
};
//jua产生的错误均应使用 JuaError 包装
//特点：可以被try函数捕获
class JuaError extends Error{
	track(stack){
		if(this.jua_stack)
			return;
		this.jua_stack = [...stack];
		//console.log(this.jua_stack)
	}
	toDebugString(){ //todo: 保证不报错（即使在toString元方法中）
		let str = this.toString();
		if(this.jua_stack)
			for(let i=this.jua_stack.length-1; i>=0; i--)
				str += '\n\tat '+this.jua_stack[i];
		return str;
	}
	toJuaObj(){
		let jerr = new Jua_Obj(errorProtos.Error);
		jerr.setProp('message', new Jua_Str(this.message));
		if(this.jua_stack){
			let arr = this.createJuaStack(this.jua_stack.length - this.stack_layer);
			jerr.setProp('stack', arr);
		}
		return jerr;
	}
	createJuaStack(len){
		len ??= this.jua_stack.length;
		let items = [];
		for(let i=0; i<len; i++){
			let {fileName, lineNumber} = this.jua_stack[i], info = new Jua_Obj;
			info.setProp('fileName', new Jua_Str(fileName));
			info.setProp('lineNumber', new Jua_Str(lineNumber));
			items.push(info);
		}
		return new Jua_Array(items);
	}
}
JuaError.prototype.name = 'JuaError';
class JuaSyntaxError extends JuaError{
	
}
class JuaErrorWrapper extends JuaError{ //通过 throw 函数抛出
	constructor(jerr, option){
		super('Client error', {cause:jerr});
		this.stack_layer = option.stack||0; //开始追踪的层数，负数表示不追踪
	}
	toString(){
		return this.toJuaObj().toString();
	}
	toJuaObj(){
		let jerr = this.cause;
		if(this.jua_stack && this.stack_layer>=0){
			let arr = this.createJuaStack(this.jua_stack.length - this.stack_layer);
			jerr.setProp('stack', arr);
		}
		return jerr;
	}
}
class JuaRefError extends JuaError{
	constructor(msg, option={}){
		super(msg, option);
		this.obj = option.obj;
		this.prop = option.prop;
	}
}
class JuaTypeError extends JuaError{}

class JuaIterator{
	//抽象类
	next(){
		//返回 { value: Jua_Val|null, done: Boolean }
		//done为假时，value非空
		throw 'pure virtual function';
	}
}
class CustomIterator extends JuaIterator{
	constructor(val, iterFn){
		super();
		this.target = val;
		this.iter = iterFn||val.getMetaMethod('iter');
		if(!this.iter)
			throw new JuaTypeError('Uniterable: '+val);
		this.key = Jua_Null.inst;
	}
	next(){ //不捕获错误，由调用者负责
		let res = this.iter.call([this.target, this.key]);
		let done = res.getOwn('done')||Jua_Null.inst;
		if(done.toBoolean())
			return {
				value: res.getOwn('value'),
				done: true
			}
		this.key = res.getOwn('key')||Jua_Null.inst;
		return {
			value: res.getOwn('value')||Jua_Null.inst,
			done: false
		}
	}
}
class ListIterator extends JuaIterator{
	constructor(list){
		super();
		this.iterator = list[Symbol.iterator]();
	}
	next(){
		let {value, done} = this.iterator.next();
		return {value:value||null, done};
	}
}

export {
	Jua_Val, Jua_Null, Jua_Num, Jua_Str, Jua_Bool, Jua_Obj, Jua_Func, Jua_NativeFunc,
	Scope, Jua_Array, Jua_Buffer,
	TrackInfo, errorProtos, JuaError, JuaSyntaxError, JuaErrorWrapper, JuaRefError, JuaTypeError
};