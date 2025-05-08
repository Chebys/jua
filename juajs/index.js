import {Scope, Jua_Null, Jua_Num, Jua_Str, Jua_Bool, Jua_Obj, Jua_Array, Jua_Func, Jua_NativeFunc, Jua_Buffer} from 'jua/value';
import {buildClass} from 'jua/builtin';
import parse from 'jua/parser';
import JuaProcess from 'jua/process';

const convertCache = new Map;
function JSToJua(val){
	//包装字符串时不会进行编码转换，直接将每个utf16码元视作一个字节
	//包装函数时，会包装函数返回值，但不包装参数
	if(convertCache.has(val))return convertCache.get(val);
	let res = _JSToJua(val);
	convertCache.set(val, res);
	return res;
}
function _JSToJua(val){
	if(val===null)return Jua_Null.inst;
	switch(typeof val){
		case 'undefined': return Jua_Null.inst;
		case 'boolean': return val ? Jua_Bool.true : Jua_Bool.false;
		case 'number': return new Jua_Num(val);
		case 'string': return new Jua_Str(val);
		case 'function': return new Jua_NativeFunc((...args)=>JSToJua(val(...args)));
		case 'object': {
			let obj = new Jua_Obj;
			assign(obj, val)
			return obj;
		}
	}
	throw new TypeError('Unconvertable type: ' + typeof val);
}
function assign(juaObj, jsObj){
	for(let k in jsObj)
		juaObj.setProp(k, JSToJua(jsObj[k]))
}

export {
	JSToJua, buildClass, assign,
	Jua_Null, Jua_Num, Jua_Str, Jua_Bool, Jua_Obj, Jua_NativeFunc,
	parse,
	JuaProcess
};
