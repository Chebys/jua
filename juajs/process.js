import {
	Jua_Val, Scope, Jua_Obj, Jua_Null, Jua_Bool, Jua_Num, Jua_Str, Jua_Func, Jua_NativeFunc, Jua_Array, Jua_Buffer,
	JuaError, JuaErrorWrapper, JuaTypeError
} from 'jua/value';
import {buildClass, classProto, errorProto, init_error, JuaJSON} from 'jua/builtin';
import {DeclarationList, Jua_PFunc} from 'jua/program';
import parse from 'jua/parser';

class JuaProcess{ //目前，一次只能创建一个实例，否则内置值冲突
	modules = Object.create(null); //模块名到模块导出值的对应，可提前加载
	constructor(main_name){
		this.main = main_name; //模块名
		this.initBuiltins();
		this._G = new Scope(null, []);
		this.makeGlobal(this._G);
	}
	run(){
		//只能运行一次
		//同步运行，可能导致阻塞
		//遇到未捕获的jua错误时，写入标准错误
		//不会捕获非jua错误
		if(globalThis.JUA_DEBUG)
			return this.require(this.main);
		try{
			this.require(this.main);
		}catch(err){
			if(err instanceof JuaError)
				this.stderr(err);
			else
				throw err;
		}
	}
	eval(script, {global=false, fileName=''}){
		let body = parse(script, {fileName});
		let env = global ? this._G : new Scope(this._G)
		return body.exec(env);
	}
	//protected:
	initBuiltins(){
		buildClass(Jua_Func.proto, (...args)=>{
			let bodystr = args.pop();
			let argnames = args.map(val=>{
				if(!(val instanceof Jua_Str))
					throw new JuaTypeError('Expect string');
				return val.value;
			});
			if(!(bodystr instanceof Jua_Str))
				throw new JuaTypeError('Expect string');
			let decList = DeclarationList.fromNames(argnames);
			let body = parse(bodystr.value);
			return new Jua_PFunc(this._G, decList, body);
		});
		this.modules['json'] = JuaJSON;
		let mathm = new Jua_Obj;
		let props = ['E', 'PI'];
		for(let key of props){
			mathm.setProp(key, new Jua_Num(Math[key]));
		}
		let fns = ['abs', 'acos', 'asin', 'atan', 'atan2', 'ceil', 'cos', 'exp', 'floor', 'log', 'max', 'min', 'pow', 'random', 'round', 'sin', 'sqrt', 'tan'];
		function toNumber(val){
			if(val.type!='number')
				throw new JuaTypeError(val+' is not a number');
			return val.value;
		}
		for(let key of fns){
			mathm.setProp(key, new Jua_NativeFunc((...vals)=>{
				let args = vals.map(toNumber);
				return new Jua_Num(Math[key](...args));
			}));
		}
		this.modules['math'] = mathm;
	}
	makeGlobal(env){ //虚函数
		//env.is_G = true;
		const tryResProto = new Jua_Obj; //非类
		tryResProto.setProp('catch', new Jua_NativeFunc((self, cb)=>{
			if(!(self instanceof Jua_Obj))
				throw new JuaTypeError('Expect object');
			if(self.getProp('status')==Jua_Bool.true)
				return null;
			if(cb instanceof Jua_Func)
				cb.call([self.getProp('error')||Jua_Null.inst]);
			else
				throw new JuaTypeError('Expect function');
		}));
		const builtinObj = { //todo
			_G: env,
			Object: Jua_Obj.proto,
			Number: Jua_Num.proto,
			String: Jua_Str.proto,
			Function: Jua_Func.proto,
			Array: Jua_Array.proto,
			Buffer: Jua_Buffer.proto,
			Error: errorProto,
		};
		const builtinFunc = {
			print: (...args)=>this.stdout(args),
			require: name=>{
				if(name instanceof Jua_Str)
					return this.require(name.value);
				throw new JuaTypeError('Expect string');
			},
			throw: (err, option)=>{
				if(err instanceof Jua_Str){
					let msg = err;
					err = new Jua_Obj(errorProto);
					init_error(err, msg);
				}else if(!(err && err.isInst(errorProto))){
					throw new JuaTypeError('throw() expects error or string', {cause:err});
				}
				let opt = {};
				if(option instanceof Jua_Obj){
					let track = option.getProp('track');
					if(track){
						opt.track = track.toInt();
					}
				}
				throw new JuaErrorWrapper(err, opt);
			},
			try: (fn, ...args)=>{
				let res = new Jua_Obj(tryResProto);
				try{
					let value = fn.call(args);
					res.setProp('status', Jua_Bool.true);
					res.setProp('value', value);
				}catch(err){
					if(!(err instanceof JuaError))
						throw err;
					let juaerr = err.toJuaObj();
					//juaerr = new Jua_Obj(errorProto);
					//init_error(juaerr, new Jua_Str('(from host)'+String(err)));
					res.setProp('status', Jua_Bool.false);
					res.setProp('error', juaerr);
				}
				return res;
			},
			type: val=>{
				if(!val)throw new JuaTypeError('Missing argument');
				return new Jua_Str(val.type);
			},
			class: proto=>{
				if(!(proto instanceof Jua_Obj))
					throw new JuaTypeError('prototype must be an object');
				proto.proto = classProto;
				return proto;
			},
			useNS: (scope, ns)=>{
				if(!(scope instanceof Scope))
					throw new JuaTypeError('useNS() expects a scope');
				if(ns instanceof Jua_Str)
					ns = this.require(ns.value);
				if(!(ns instanceof Jua_Obj))
					throw new JuaTypeError('useNS() expects an object or a name of module exporting an object');
				scope.assignProps(ns);
			}
		};
		for(let name in builtinObj)env.setProp(name, builtinObj[name]);
		for(let name in builtinFunc)env.setProp(name, new Jua_NativeFunc(builtinFunc[name]));
	}
	findModule(name){ //纯虚函数；返回模块代码
		throw 'pure virtual function';
	}
	stdout(vals){} //虚函数，接收Jua值
	stderr(err){} //虚函数，接收Jua错误（具体来说，是 JuaError.toJuaObj()）
	require(name){
		if(name in this.modules)return this.modules[name];
		//todo: 检查循环导入
		let script = this.findModule(name);
		this.modules[name] = this.eval(script, {fileName:name});
		return this.modules[name];
	}
}

export default JuaProcess;