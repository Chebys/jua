import {
	Scope, Jua_Null, Jua_Num, Jua_Str, Jua_Bool, Jua_Obj, Jua_Array, Jua_Func, Jua_NativeFunc, Jua_Buffer,
	JuaTypeError
} from 'jua/value';

function initBuiltins(process){
	todo
}

function _buildClass(proto, ctor){ //ctor为Jua_Func
	let classProto = new Jua_Obj;
	classProto.setProp('__class', Jua_Bool.true);
	classProto.setProp('__call', ctor);
	proto.proto = classProto;
}
function buildClass(proto, ctor){ //ctor为js函数，且不会接收第一个参数
	_buildClass(proto, new Jua_NativeFunc((_, ...args)=>ctor(...args)));
}

let obj_new = new Jua_NativeFunc((proto, ...args)=>{
	let obj = new Jua_Obj(proto);
	let initfn = proto.getProp('init');
	if(initfn instanceof Jua_Func)
		initfn.call([obj, ...args]);
	return obj;
});
_buildClass(Jua_Obj.proto, obj_new);
const classProto = Jua_Obj.proto.proto;
Jua_Obj.proto.setProp('new', obj_new);
Jua_Obj.proto.setProp('get', new Jua_NativeFunc((obj, key, mode)=>{
	if(!(key instanceof Jua_Str))
		throw new JuaTypeError('non-string key: '+key.type);
	key = key.value;
	if(mode instanceof Jua_Str){
		if(mode.value=='own')
			return obj.getOwn(key);
		if(mode.value=='inherit')
			return obj.inheritProp(key);
	}
	return obj.getProp(key);
}));
Jua_Obj.proto.setProp('set', new Jua_NativeFunc((obj, key, value)=>{
	if(!(key instanceof Jua_Str))
		throw new JuaTypeError('non-string key: '+key.type);
	obj.setProp(key.value, value);
}));
Jua_Obj.proto.setProp('del', new Jua_NativeFunc((obj, key)=>{
	if(!(obj instanceof Jua_Obj))
		throw new JuaTypeError('Expect object');
	if(!(key instanceof Jua_Str))
		throw new JuaTypeError('non-string key: '+key.type);
	obj.delProp(key.value);
}));
Jua_Obj.proto.setProp('hasOwn', new Jua_NativeFunc((obj, key)=>{
	if(!(obj instanceof Jua_Obj))
		return Jua_Bool.false;
	if(!(key instanceof Jua_Str))
		throw new JuaTypeError('non-string key: '+key.type);
	return Jua_Bool[obj.hasOwn(key)];
}));
Jua_Obj.proto.setProp('setProto', new Jua_NativeFunc((obj, proto)=>{
	if(!(obj instanceof Jua_Obj))
		throw new JuaTypeError('Expect object');
	if(proto instanceof Jua_Obj)
		obj.proto = proto;
	else
		obj.proto = null;
}));
Jua_Obj.proto.setProp('getProto', new Jua_NativeFunc(obj => obj?.proto || Jua_Null.inst));
Jua_Obj.proto.setProp('iter', new Jua_NativeFunc((obj, key)=>{ //不建议使用，性能较低
	if(!(obj instanceof Jua_Obj))
		throw new JuaTypeError('Expect object');
	if(key instanceof Jua_Str)
		key = key.value;
	else
		key = true;
	let res = new Jua_Obj;
	for(let k in obj.dict){
		if(key===true){
			key = new Jua_Str(k);
			res.setProp('key', key);
			res.setProp('value', key);
			res.setProp('done', Jua_Bool.false);
			return res;
		}else if(key==k){
			key = true;
		}
	}
	res.setProp('done', Jua_Bool.true);
	return res;
}));
let idcounter = -1;
Jua_Obj.proto.setProp('id', new Jua_NativeFunc(obj=>{
	if(!(obj instanceof Jua_Obj || obj instanceof Jua_Func))
		throw new JuaTypeError('Object.id() expects object or function');
	if(obj.id<0)
		obj.id = ++idcounter;
	return new Jua_Num(obj.id);
}));

buildClass(Jua_Num.proto, val=>{
	if(val instanceof Jua_Num)return val;
	if(val instanceof Jua_Str)return new Jua_Num(Number(val.str));
	if(val==Jua_Null.inst || val==Jua_Bool.false)return new Jua_Num(0);
	if(val==Jua_Bool.true)return new Jua_Num(1);
	return Jua_Num.NaN;
});
function testEndian(){
	let arr = new Uint16Array(1);
	arr[0] = 1;
	let bytes = new Uint8Array(arr.buffer);
	return bytes[0];
}
Jua_Num.proto.setProp('LITTLE_ENDIAN', testEndian() ? Jua_Bool.true : Jua_Bool.false);
Jua_Num.proto.setProp('toString', new Jua_NativeFunc((num, radix)=>{
	if(!(num instanceof Jua_Num))
		throw new JuaTypeError('Expect number');
	return new Jua_Str(num.value.toString(radix ? radix.toInt() : 10));
}));
Jua_Num.proto.setProp('range',  new Jua_NativeFunc((v1, v2)=>{
	if(!(v1 instanceof Jua_Num))
		throw new JuaTypeError('Expect number');
	return v1.range(v2);
}));
Jua_Num.proto.setProp('isInt', new Jua_NativeFunc(num=>{
	if(!(num instanceof Jua_Num))
		return Jua_Bool.false;
	return Number.isInt(num.value) ? Jua_Bool.true : Jua_Bool.false;
}));
function registerCoding(label){ // 8/16/32位整数，32/64位浮点数
	let TArray = globalThis[label+'Array'];
	if(!TArray)throw 'Invalid label: '+label;
	let size = TArray.BYTES_PER_ELEMENT;
	Jua_Num.proto.setProp('encode'+label, new Jua_NativeFunc((...nums)=>{
		let len = nums.length;
		let arr = new TArray(len);
		for(let i=0; i<len; i++){
			if(nums[i].type!='number')
				throw new JuaTypeError('Expect number');
			arr[i] = nums[i].value;
		}
		let bytes = new Uint8Array(arr.buffer);
		return new Jua_Str(String.fromCharCode(...bytes));
	}));
	Jua_Num.proto.setProp('decode'+label, new Jua_NativeFunc((str, offset)=>{
		if(!str || str.type!='string')
			throw new JuaTypeError('Expect string');
		offset = offset ? offset.toInt() : 0;
		str = str.value;
		let bytes = new Uint8Array(size);
		for(let i=0; i<size; i++){
			bytes[i] = str.charCodeAt(offset+i);
		}
		let arr = new TArray(bytes.buffer);
		return new Jua_Num(arr[0]);
	}));
}
['Uint8', 'Uint16', 'Uint32', 'Int8', 'Int16', 'Int32', 'Float32', 'Float64'].forEach(registerCoding);
Jua_Num.proto.setProp('parse', ()=>todo);

buildClass(Jua_Str.proto, val=>{
	if(!val)
		return new Jua_Str;
	if(val instanceof Jua_Str)
		return val;
	return new Jua_Str(val.toString());
	
});
Jua_Str.proto.setProp('byte', new Jua_NativeFunc((str, i)=>{
	if(!(str instanceof Jua_Str))
		throw new JuaTypeError('Expect string');
	let c = str._getItem(i || Jua_Null.inst);
	if(c)return new Jua_Num(c.charCodeAt());
	return Jua_Null.inst;
}));
Jua_Str.proto.setProp('fromByte', new Jua_NativeFunc((...args)=>{
	let str = String.fromCharCode(...args.map(val=>val.toInt()));
	return new Jua_Str(str);
}));
Jua_Str.proto.setProp('hasItem', new Jua_NativeFunc((str, key)=>{
	if(!(str instanceof Jua_Str))
		throw new JuaTypeError('Expect string');
	return str.hasItem(key || Jua_Null.inst);
}));
Jua_Str.proto.setProp('getItem', new Jua_NativeFunc((str, i)=>{
	if(!(str instanceof Jua_Str))
		throw new JuaTypeError('Expect string');
	return str.getItem(i || Jua_Null.inst);
}));
Jua_Str.proto.setProp('len', new Jua_NativeFunc(str=>{
	if(!(str instanceof Jua_Str))
		throw new JuaTypeError('Expect string');
	return new Jua_Num(str.value.length);
}));
Jua_Str.proto.setProp('lower', new Jua_NativeFunc(str=>{
	if(!(str instanceof Jua_Str))
		throw new JuaTypeError('Expect string');
	return new Jua_Str(str.value.toLowerCase());
}));
Jua_Str.proto.setProp('upper', new Jua_NativeFunc(str=>{
	if(!(str instanceof Jua_Str))
		throw new JuaTypeError('Expect string');
	return new Jua_Str(str.value.toUpperCase());
}));
Jua_Str.proto.setProp('slice', new Jua_NativeFunc((str, start, end)=>{
	if(!(str instanceof Jua_Str))
		throw new JuaTypeError('Expect string');
	return str.slice(start, end);
}));
Jua_Str.proto.setProp('toHex', new Jua_NativeFunc((str, start, end)=>{
	if(!(str instanceof Jua_Str))
		throw new JuaTypeError('Expect string');
	let hex = [];
	for(let c of str.value)
		hex.push(c.charCodeAt().toString(16).padStart(2, '0'));
	return new Jua_Str(hex.join(' '));
}));

const errorProto = new Jua_Obj(classProto);
function init_error(self, msg){
	if(!(self instanceof Jua_Obj))
		throw new JuaTypeError('Expect object');
	if(!msg)
		msg = new Jua_Str;
	else if(msg.type != 'string')
		msg = new Jua_Str(msg.toString());
	self.setProp('message', msg);
	self.setProp('stack', Jua_Null.inst);
}
errorProto.setProp('init', new Jua_NativeFunc(init_error));
errorProto.setProp('name', new Jua_Str('Error'));
errorProto.setProp('toString', new Jua_NativeFunc(err=>{
	if(!(err instanceof Jua_Obj))
		throw new JuaTypeError('Expect object');
	let name = err.getProp('name'), msg = err.getProp('message');
	name = name ? name.toString() : 'Unknown Error';
	msg = msg ? msg.toString() : '';
	return new Jua_Str(msg ?  name+': '+msg : name);
}));

buildClass(Jua_Array.proto, iterable=>{
	let items = [];
	for(let val of iterable)
		items.push(val);
	return new Jua_Array(items);
});
Jua_Array.proto.setProp('of', new Jua_NativeFunc((...items)=>new Jua_Array(items)));
function jua_join(jarr, sep=new Jua_Str(', ')){ //只要求jarr可迭代
	let start = true, str = '';
	for(let item of jarr){
		if(start)
			start = false;
		else
			str += sep.value;
		str += item.toString();
	}
	return new Jua_Str(str);
}
Jua_Array.proto.setProp('join', new Jua_NativeFunc(jua_join));
Jua_Array.proto.setProp('toString', new Jua_NativeFunc(jarr=>{
	let jstr = jua_join(jarr);
	return new Jua_Str(`[${jstr.value}]`);
}));
Jua_Array.proto.setProp('getItem', new Jua_NativeFunc((arr, i)=>{
	if(!(arr instanceof Jua_Array))
		throw new JuaTypeError('Expect array');
	return arr.getItem(i || Jua_Null.inst);
}));

Jua_Num.rangeProto.proto = classProto;
Jua_Num.rangeProto.setProp('iter', new Jua_NativeFunc((self, key)=>{
	if(key instanceof Jua_Num)
		key = key.add(new Jua_Num(1));
	else
		key = self.getProp('start');
	let res = new Jua_Obj;
	res.setProp('key', key);
	res.setProp('value', key);
	res.setProp('done', key.lt(self.getProp('end')).toBoolean() ? Jua_Bool.false : Jua_Bool.true);
	return res;
}));

buildClass(Jua_Buffer.proto, length=>{
	return new Jua_Buffer(length.toInt());
});
Jua_Buffer.proto.setProp('read', new Jua_NativeFunc((self, start, end)=>{
	if(self instanceof Jua_Buffer)
		return self.read(start, end);
	throw new JuaTypeError('expect buffer');
}));
Jua_Buffer.proto.setProp('write', new Jua_NativeFunc((self, str, pos)=>{
	if(self instanceof Jua_Buffer)
		return self.write(str, pos);
	throw new JuaTypeError('expect buffer');
}));
Jua_Buffer.proto.setProp('clear', new Jua_NativeFunc(self=>{
	if(!(self instanceof Jua_Buffer))
		throw new JuaTypeError('expect buffer');
	self.bytes.fill(0);
}));
Jua_Buffer.proto.setProp('len', new Jua_NativeFunc(obj=>obj?.getProp('length')));


let JuaJSON = new Jua_Obj;
JuaJSON.setProp('encode', new Jua_NativeFunc(val=>{
	if(!val)
		throw new JuaTypeError('Missing argument');
	let str = JSON.stringify(val); //递归调用toJSON
	return new Jua_Str(str);
}));
JuaJSON.setProp('decode', ()=>todo);
export {buildClass, classProto, errorProto, init_error, JuaJSON};