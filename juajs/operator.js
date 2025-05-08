import  {Jua_Val, Jua_Num, Jua_Bool} from 'jua/value';
//todo
export const uniOperator = {
	'-': val => {
		if(val instanceof Jua_Num)
			return new Jua_Num(-val.value);
		return Jua_Num.NaN; //todo: 重载
	},
	'!': val => val.toBoolean() ? Jua_Bool.false : Jua_Bool.true
}
export const binOperator = {
	'^': {
		priority: 12
	},
	'*': {
		priority: 11,
		fn(v1, v2){
			return v1.mul(v2);
		}
	},
	'/': {
		priority: 11,
		fn(v1, v2){
			return v1.div(v2);
		}
	},
	'+': {
		priority: 10,
		fn(v1, v2){
			return v1.add(v2);
		}
	},
	'-': {
		priority: 10,
		fn(v1, v2){
			return v1.sub(v2);
		}
	},
	'%': {
		priority: 9
	},
	'..': {
		priority: 7,
		fn(v1, v2){
			return v1.range(v2);
		}
	},
	'<<': {
		priority: 6
	},
	'>>': {
		priority: 6
	},
	'<': {
		priority: 5,
		fn(v1, v2){
			return v1.lt(v2);
		}
	},
	'>': {
		priority: 5,
		fn(v1, v2){
			return v1.le(v2).toBoolean() ? Jua_Bool.false : Jua_Bool.true;
		}
	},
	'<=': {
		priority: 5,
		fn(v1, v2){
			return v1.le(v2);
		}
	},
	'>=': {
		priority: 5,
		fn(v1, v2){
			return v1.lt(v2).toBoolean() ? Jua_Bool.false : Jua_Bool.true;
		}
	},
	'==': {
		priority: 5,
		fn(v1, v2){
			return v1.eq(v2);
		}
	},
	'!=': {
		priority: 5,
		fn(v1, v2){
			return v1.equalTo(v2) ? Jua_Bool.false : Jua_Bool.true;
		}
	},
	'in': {
		priority: 5,
		fn(v1, v2){
			return v2.hasItem(v1);
		}
	},
	'is': {
		priority: 5,
		fn(v1, v2){
			return v1.isInst(v2) ? Jua_Bool.true : Jua_Bool.false;
		}
	},
	'&&': {
		priority: 3,
		circuited: true,
		fn(env, e1, e2){
			let v1 = e1.calc(env);
			if(v1.toBoolean())
				return e2.calc(env);
			return v1;
		}
	},
	'||': {
		priority: 3,
		circuited: true,
		fn(env, e1, e2){
			let v1 = e1.calc(env);
			if(v1.toBoolean())return v1;
			return e2.calc(env);
		}
	},
};
for(let str in binOperator){
	binOperator[str].str = str;
}