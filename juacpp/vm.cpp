#include "jua-vm.h"
#include "jua-syntax.h"
#include "coding.h"

using namespace Jua;
using std::string;

VM::VM(){
    initBuiltins();
    makeGlobal();
    modules["math"] = makeMath();
    modules["json"] = makeJSON();
    modules["date"] = makeDate();
}

void VM::run(const string& script){
    try{
        eval(script);
    }catch(JuaError* e){
        j_stderr(e);
        delete e; //错误对象没人持有，报告完即释放
    }
}

Value* VM::eval(const string& script){
    //每次求值新建一个作用域：模块之间不共享局部变量
    return evalIn(new Scope(_G), script);
}
Value* VM::evalIn(Scope* env, const string& script){
    //返回非空指针，可能需要垃圾回收
    auto body = parse(script);
    return body->exec(env);
}

void VM::own(Value* value){
    ownedValues.insert(value);
}

void VM::initBuiltins(){
    obj_new = makeFunc([this](jualist& args){
        Value* proto = nullptr;
        if(args.size() > 0){
            proto = args[0];
            if(proto->type == Value::Null){
                proto = nullptr;
            }else if(proto->type != Value::Obj){
                throw new JuaTypeError("Object.new() requires an object prototype");
            }
            args.pop_front();
        }
        auto obj = new Object(this, static_cast<Object*>(proto));
        if(proto){
            auto init = obj->getMetaMethod("init");
            if(init){
                args.push_front(obj);
                init->call(args);
            }
        }
        return obj;
    });
    obj_next = makeFunc([](jualist& args){
        if(args.size() < 2) throw new JuaTypeError("Object.next() requires 2 arguments");
        auto self = args[0];
        if(self->type != Value::Obj){
            throw new JuaError("Range.next() called on a non-object");
        }
        auto obj = static_cast<Object*>(self);
        auto key = args[1];
        std::unordered_map<string, Value*>::iterator it;
        if(key->type == Value::Str){
            string str_key = key->toString();
            it = obj->dict.find(str_key);
            it++;
        }else{
            it = obj->dict.begin();
        }
        auto res = new Object(self->vm);
        if(it == obj->dict.end()){
            res->setProp("done", Boolean::getInst(true));
        }else{
            auto value = new String(self->vm, it->first);
            res->setProp("done", Boolean::getInst(false));
            res->setProp("key", value);
            res->setProp("value", value);
        }
        return res;
    });
    classProto = new Object(this); //标准可构造类的原型，首先初始化
    classProto->setProp("__class", Boolean::getInst(true));
    //FunctionProto 必须尽早建立：此后创建的每个函数才会带上原型
    FunctionProto = makeFunctionProto();
    classProto->setProp("__call", obj_new);

    RangeProto = makeRangeProto(); //必须在 NumberProto 之前
    NumberProto = makeNumberProto();
    StringProto = makeStringProto();
    BooleanProto = new Object(this);
    ObjectProto = makeObjectProto();

    ArrayProto = makeArrayProto();
    BufferProto = makeBufferProto();
    ErrorProto = makeErrorProto();
    TryResProto = makeTryResProto();
}
void VM::makeGlobal(){
    _G = new Scope(this);
    _G->setProp("String", StringProto);
    _G->setProp("Number", NumberProto);
    _G->setProp("Boolean", BooleanProto);
    _G->setProp("Function", FunctionProto);
    _G->setProp("Object", ObjectProto);
    _G->setProp("Array", ArrayProto);
    _G->setProp("Buffer", BufferProto);
    _G->setProp("Range", RangeProto);
    _G->setProp("Error", ErrorProto);
    _G->setProp("_G", _G);
    _G->setProp("print", makeFunc([this](jualist& args){
        j_stdout(args);
        return Null::getInst();
    }));
    _G->setProp("class", makeFunc([this](jualist& args){
        if(args.size() < 1) throw new JuaTypeError("class() requires at least one argument");
        Value* proto = args[0];
        if(proto->type != Value::Obj){
            throw new JuaError("class() requires an object prototype");
        }
        proto->proto = classProto; //todo: 如果采用引用计数，需要设置一个统一的入口来修改 proto
        return proto;
    }));
    _G->setProp("require", makeFunc([this](jualist& args){
        if(args.size() < 1) throw new JuaTypeError("require() requires at least one argument");
        if(args[0]->type != Value::Str){
            throw new JuaError("require() requires a string argument");
        }
        return require(args[0]->toString());
    }));
    _G->setProp("throw", makeFunc([this](jualist& args){
        if(args.size() < 1) throw new JuaTypeError("throw() requires at least one argument");
        Value* val = args[0];
        //用户主动抛出的错误归为 Client 类
        if(val->type == Value::Str){
            throw new JuaError(val->toString(), JuaError::Kind::Client);
        }
        //Error 对象：连同对象本身一起抛出，try 与 catch 都能拿到它
        if(val->type == Value::Obj && val->isInst(ErrorProto)){
            throw new JuaErrorWithVal(val->toString(), val, JuaError::Kind::Client);
        }
        throw new JuaTypeError("throw() requires a string or an Error object");
        return Null::getInst(); // unreachable, but required for function signature
    }));
    _G->setProp("try", makeFunc([this](jualist& args){
        auto res = new Object(this, TryResProto);
        try{
            if(args.size() < 1) throw new JuaTypeError("try() requires at least one argument");
            Value* fn = args[0];
            if(fn->type != Value::Func){
                throw new JuaTypeError("try() requires a function argument");
            }
            args.pop_front();
            res->setProp("value", fn->call(args));
            res->setProp("status", Boolean::getInst(true));
            return res;
        } catch (JuaError* e) {
            //抛出的若是 Error 对象，就把对象本身交给调用者；否则给错误文本
            if(auto withVal = dynamic_cast<JuaErrorWithVal*>(e))
                res->setProp("error", withVal->val);
            else
                res->setProp("error", new String(this, e->toDebugString()));
            res->setProp("errorKind", new String(this, e->kindName()));
            res->setProp("status", Boolean::getInst(false));
            delete e; //错误已经转成 jua 值，原对象可以释放
            return res;
        }
    }));
    _G->setProp("type", makeFunc([this](jualist& args){
        if(args.size() < 1) throw new JuaTypeError("type() requires at least one argument");
        Value* val = args[0];
        auto typeName = val->getTypeName();
        return new String(this, typeName);
    }));
    // useNS(scope, ns)：把命名空间成员导入 scope（见 draft/builtins/fns.md）
    _G->setProp("useNS", makeFunc([this](jualist& args){
        if(args.size() < 2) throw new JuaTypeError("useNS() requires 2 arguments");
        Value* scope = args[0];
        if(scope->type != Value::Obj)
            throw new JuaTypeError("useNS() requires an object scope");
        Value* ns = args[1];
        if(ns->type == Value::Str) ns = require(ns->toString());
        auto assign = ObjectProto->getOwn("assign");
        if(!assign || assign->type != Value::Func)
            throw new JuaError("internal: Object.assign is missing");
        static_cast<Function*>(assign)->call({scope, ns});
        return Null::getInst();
    }));
}
Value* VM::require(const string& name){
    if(modules.contains(name))return modules[name];
    //缓存发生在执行之后（见 draft/builtins/fns.md），因此执行期间再次导入
    //同一个模块会导致重复执行，属于循环导入，直接报错
    if(loading.contains(name))
        throw new JuaError("循环导入：" + name);
    const string& script = findModule(name);
    loading.insert(name);
    Value* value = nullptr;
    try{
        value = eval(script);
    }catch(...){
        loading.erase(name);
        throw;
    }
    loading.erase(name);
    modules[name] = value;
    return value;
}

Object* VM::buildClass(NativeFunction::Native constructor){
    //constructor 接收初始化参数（不包括类自身），返回类实例
    auto clsProto = new Object(this);
    clsProto->setProp("__class", Boolean::getInst(true));
    clsProto->setProp("__call", makeFunc([this, constructor](jualist& args){
        args.pop_front(); // 移除类本身
        return constructor(args);
    }));
    auto cls = new Object(this, clsProto);
    return cls;
}

Object* VM::makeRangeProto(){
    auto proto = new Object(this, classProto);
    proto->setProp("init", makeFunc([](jualist& args){
        if(args.size() < 3) throw new JuaTypeError("range() requires 3 arguments");
        auto self = args[0];
        if(self->type != Value::Obj)
            throw new JuaError("range() called on non-object value");
        auto obj = static_cast<Object*>(self);
        double start = args[1]->toNumber();
        double end = args[2]->toNumber();
        double step = 1;
        if(args.size() >= 4){
            step = args[3]->toNumber();
        }
        obj->setProp("start", new Number(self->vm, start));
        obj->setProp("end", new Number(self->vm, end));
        obj->setProp("step", new Number(self->vm, step));
        return Null::getInst();
    }));
    proto->setProp("next", makeFunc([](jualist& args){
        //todo: 支持浮点数
        if(args.size() < 2) throw new JuaTypeError("Range.next() requires 2 arguments");
        auto self = args[0];
        if(self->type != Value::Obj){
            throw new JuaError("Range.next() called on a non-object");
        }
        auto key = args[1];
        int64_t index;
        if(key->type == Value::Num){
            auto step = self->getProp("step");
            if(!step || step->type != Value::Num)
                throw new JuaError("Range.next() called on a non-range object");
            index = key->toInt() + step->toInt();
        }else{
            auto start = self->getProp("start");
            if(!start || start->type != Value::Num)
                throw new JuaError("Range.next() called on a non-range object");
            index = start->toInt();
        }
        
        auto end = self->getProp("end");
        if(!end || end->type != Value::Num)
            throw new JuaError("Range.next() called on a non-range object");
        auto res = new Object(self->vm);
        bool done = index >= end->toInt();
        res->setProp("done", Boolean::getInst(done));
        if(!done){
            auto value = new Number(self->vm, index);
            res->setProp("value", value);
            res->setProp("key", value);
        }
        return res;
    }));
    return proto;
}
Object* VM::makeNumberProto(){
    auto proto = buildClass([this](jualist& args){
        double value = 0.0;
        if(args.size() > 0){
            Value* arg = args[0];
            if(arg->type == Value::Num){
                value = static_cast<Number*>(arg)->value;
            }else if(arg->type == Value::Str){
                try {
                    value = std::stod(arg->toString());
                } catch (const std::invalid_argument&) {
                    throw new JuaError("Invalid number string: " + arg->toString());
                } catch (const std::out_of_range&) {
                    throw new JuaError("Number out of range: " + arg->toString());
                }
            } else {
                throw new JuaError("Number constructor requires a number or string argument");
            }
        }
        return new Number(this, value);
    });
    proto->setProp("LITTLE_ENDIAN", Boolean::getInst(isLittleEndian()));
    proto->setProp("range", RangeProto);
    proto->setProp("__range", makeFunc([this](jualist& args){
        if(args.size() < 2)throw new JuaError("requires at least 2 arguments");
        return RangeProto->call(args);
    }));
    proto->setProp("toString", makeFunc([](jualist& args){
        if(args.size() < 1) throw new JuaError("Number.toString() requires 1 argument");
        auto self = args[0];
        if(self->type != Value::Num){
            throw new JuaError("Number.toString() called on a non-number");
        }
        return new String(self->vm, self->toString());
    }));
    proto->setProp("isInt", makeFunc([](jualist& args){
        if(args.size() < 1) throw new JuaError("Number.isInt() requires 1 argument");
        auto val = args[0];
        if(val->type != Value::Num)return Boolean::getInst(false);
        double d = val->toNumber();
        //Infinity 没有小数部分，但不是整数
        return Boolean::getInst(std::isfinite(d) && std::floor(d) == d);
    }));
    proto->setProp("encodeUint8", makeEncodeFunc(encode<uint8_t>));
    proto->setProp("decodeUint8", makeDecodeFunc(decode<uint8_t>));
    proto->setProp("encodeUint16", makeEncodeFunc(encode<uint16_t>));
    proto->setProp("decodeUint16", makeDecodeFunc(decode<uint16_t>));
    proto->setProp("encodeUint32", makeEncodeFunc(encode<uint32_t>));
    proto->setProp("decodeUint32", makeDecodeFunc(decode<uint32_t>));
    proto->setProp("encodeInt8", makeEncodeFunc(encode<int8_t>));
    proto->setProp("decodeInt8", makeDecodeFunc(decode<int8_t>));
    proto->setProp("encodeInt16", makeEncodeFunc(encode<int16_t>));
    proto->setProp("decodeInt16", makeDecodeFunc(decode<int16_t>));
    proto->setProp("encodeInt32", makeEncodeFunc(encode<int32_t>));
    proto->setProp("decodeInt32", makeDecodeFunc(decode<int32_t>));
    proto->setProp("encodeFloat32", makeEncodeFunc(encode<float>));
    proto->setProp("decodeFloat32", makeDecodeFunc(decode<float>));
    proto->setProp("encodeFloat64", makeEncodeFunc(encode<double>));
    proto->setProp("decodeFloat64", makeDecodeFunc(decode<double>));
    return proto;
}
Object* VM::makeStringProto(){
    auto proto = buildClass([this](jualist& args){
        string value;
        if(args.size() > 0){
            value = args[0]->toString();
        }
        return new String(this, value);
    });
    proto->setProp("byte", makeFunc([](jualist& args){
        if(args.size() < 1)throw new JuaError("missing argument");
        auto val = args[0];
        if(val->type != Value::Str)
            throw new JuaError("String.byte() called on non-string value");
        auto self = static_cast<String*>(val);
        if(self->value.empty())
            throw new JuaRangeError("String.byte() called on an empty string");
        size_t index = 0;
        if(args.size() >= 2){
            int64_t len = (int64_t)self->value.size();
            int64_t i = args[1]->toInt();
            if(i < 0)i += len; //负数从末尾算起
            index = (size_t)(i % len);
        }
        uint8_t byte = self->value[index];
        return new Number(self->vm, byte);
    }));
    proto->setProp("fromByte", makeFunc([this](jualist& args){
        string str;
        str.reserve(args.size());
        for(auto v: args){
            str.push_back(v->toInt());
        }
        return new String(this, str);
    }));
    proto->setProp("len", makeFunc([](jualist& args){
        if(args.size() < 1)throw new JuaError("missing argument");
        auto val = args[0];
        if(val->type != Value::Str)
            throw new JuaError("String.len() called on non-string value");
        auto str = static_cast<String*>(val);
        return new Number(val->vm, str->value.size());
    }));
    proto->setProp("toHex", makeFunc([](jualist& args){
        static const char hexDigits[] = "0123456789abcdef";

        if(!args.size())throw new JuaError("String.toHex() requires at least 1 argument");
        auto self = args[0];
        if(self->type != Value::Str)throw new JuaError("String.toHex() called on non-string value");
        auto str = self->toString();
        string hexStr;
        hexStr.reserve(str.size() * 3);
        for (uint8_t byte: str) {
            hexStr.push_back(hexDigits[byte >> 4]);  // 高4位
            hexStr.push_back(hexDigits[byte & 0x0F]); // 低4位
            hexStr.push_back(' ');
        }
        //每字节后都追加了空格，去掉末尾那个；空串没有可去的
        if(!hexStr.empty())hexStr.pop_back();
        return new String(self->vm, hexStr);
    }));
    proto->setProp("lower", makeFunc([](jualist& args){        if(!args.size())throw new JuaError("String.lower() requires at least 1 argument");
        auto self = args[0];
        if(self->type != Value::Str)throw new JuaError("String.lower() called on non-string value");
        auto str = self->toString();
        string lowerStr;
        lowerStr.reserve(str.size());
        for(char c: str){
            lowerStr.push_back(std::tolower(c));
        }
        return new String(self->vm, lowerStr);
    }));
    proto->setProp("upper", makeFunc([](jualist& args){
        if(!args.size())throw new JuaError("String.upper() requires at least 1 argument");
        auto self = args[0];
        if(self->type != Value::Str)throw new JuaError("String.upper() called on non-string value");
        auto str = self->toString();
        string upperStr;
        upperStr.reserve(str.size());
        for(char c: str){
            upperStr.push_back(std::toupper(c));
        }
        return new String(self->vm, upperStr);
    }));
    proto->setProp("hasItem", makeFunc([](jualist& args){
        if(args.size() < 2)throw new JuaError("String.hasItem() requires 2 arguments");
        return args[0]->hasItem(args[1]);
    }));
    proto->setProp("getItem", makeFunc([](jualist& args){
        if(args.size() < 2)throw new JuaError("String.getItem() requires 2 arguments");
        if(args[0]->type != Value::Str)throw new JuaError("String.getItem() called on non-string value");
        return args[0]->getItem(args[1]);
    }));
    proto->setProp("slice", makeFunc([](jualist& args){
        if(!args.size())throw new JuaError("String.slice() requires at least 1 argument");
        auto self = args[0];
        if(self->type != Value::Str)throw new JuaError("String.slice() called on non-string value");
        const string& str = static_cast<String*>(self)->value;
        int64_t len = (int64_t)str.size();
        int64_t start = args.size() >= 2 ? args[1]->toInt() : 0;
        int64_t end = args.size() >= 3 ? args[2]->toInt() : len;
        //负数表示从末尾算起
        if(start < 0)start += len;
        if(end < 0)end += len;
        if(start < 0)start = 0;
        if(end > len)end = len;
        if(start >= end)return new String(self->vm, "");
        return new String(self->vm, str.substr((size_t)start, (size_t)(end-start)));
    }));
    return proto;
}
Object* VM::makeFunctionProto(){
    auto proto = buildClass([this](jualist& args){
        if(args.size() < 1) throw new JuaError("Function constructor requires at least one argument");
        std::deque<DeclarationItem*> params;
        for(size_t i=0; i<args.size()-1; i++){
            if(args[i]->type != Value::Str){
                throw new JuaError("Function constructor requires string arguments");
            }
            auto varname = new Varname(args[i]->toString()); //todo: 检查合法性
            auto decItem = new DeclarationItem(varname, nullptr);
            params.push_back(decItem);
        }
        auto script = args.back();
        auto body = parse(script->toString());
        auto declist = new DeclarationList(params);
        return new Jua_PFunc(_G, declist, body);
    });
    return proto;
}
Object* VM::makeObjectProto(){
    auto proto = new Object(this, classProto);
    proto->setProp("new", obj_new);
    proto->setProp("get", makeFunc([](jualist& args){
        if(args.size() < 2) throw new JuaTypeError("Object.get() requires 2 arguments");
        //obj 未必是对象：任何值都可以进行属性访问（见 draft/builtins/classes.md）
        auto self = args[0];
        auto key = args[1];
        if(key->type != Value::Str){
            throw new JuaError("Object.get() requires a string key");
        }
        auto value = self->getProp(key->toString());
        if(value)return value;
        //第三个参数是属性不存在时的默认值
        return args.size() >= 3 ? args[2] : Null::getInst();
    }));
    proto->setProp("assign", makeFunc([this](jualist& args){
        if(args.size() < 1) throw new JuaTypeError("Object.assign() requires at least 1 argument");
        auto target = args[0];
        if(target->type != Value::Obj)
            throw new JuaTypeError("Object.assign() requires an object target");
        auto dst = static_cast<Object*>(target);
        for(size_t i=1; i<args.size(); i++){
            auto src = args[i];
            if(src->type == Value::Null) continue; //同 JS：null 源被忽略
            if(src->type != Value::Obj)
                throw new JuaTypeError("Object.assign() requires object sources");
            auto obj = static_cast<Object*>(src);
            //先收集再写入：源与目标可能是同一个对象，边遍历边插入会破坏迭代器
            std::vector<std::pair<std::string, Value*>> pairs;
            for(auto& kv: obj->dict)
                pairs.emplace_back(kv.first, kv.second);
            for(auto& [key, value]: pairs)
                dst->setProp(key, value);
        }
        return target;
    }));
    proto->setProp("getOwn", makeFunc([](jualist& args){
        if(args.size() < 2) throw new JuaTypeError("Object.getOwn() requires 2 arguments");
        auto self = args[0];
        if(self->type != Value::Obj)
            throw new JuaTypeError("Object.getOwn() called on a non-object");
        auto key = args[1];
        if(key->type != Value::Str)
            throw new JuaTypeError("Object.getOwn() requires a string key");
        auto value = static_cast<Object*>(self)->getOwn(key->toString());
        if(value)return value;
        return args.size() >= 3 ? args[2] : Null::getInst();
    }));
    proto->setProp("keys", makeFunc([this](jualist& args){
        if(args.size() < 1) throw new JuaTypeError("Object.keys() requires 1 argument");
        auto self = args[0];
        if(self->type != Value::Obj)
            throw new JuaTypeError("Object.keys() called on a non-object");
        auto arr = new Array(this, {});
        for(auto& kv: static_cast<Object*>(self)->dict)
            arr->items.push_back(new String(this, kv.first));
        return arr;
    }));
    proto->setProp("entries", makeFunc([this](jualist& args){
        if(args.size() < 1) throw new JuaTypeError("Object.entries() requires 1 argument");
        auto self = args[0];
        if(self->type != Value::Obj)
            throw new JuaTypeError("Object.entries() called on a non-object");
        auto arr = new Array(this, {});
        for(auto& kv: static_cast<Object*>(self)->dict){
            auto pair = new Array(this, {});
            pair->items.push_back(new String(this, kv.first));
            pair->items.push_back(kv.second);
            arr->items.push_back(pair);
        }
        return arr;
    }));
    // 每次调用建一个迭代器原型：元方法只查原型，next 必须挂在原型上
    proto->setProp("pairs", makeFunc([this](jualist& args){
        if(args.size() < 1) throw new JuaTypeError("Object.pairs() requires 1 argument");
        auto self = args[0];
        if(self->type != Value::Obj)
            throw new JuaTypeError("Object.pairs() called on a non-object");
        auto itProto = new Object(this);
        itProto->setProp("next", makeFunc([this](jualist& args) -> Value* {
            if(args.size() < 2) throw new JuaTypeError("pairs.next() requires 2 arguments");
            auto iter = args[0];
            if(iter->type != Value::Obj)
                throw new JuaTypeError("pairs.next() called on a non-object");
            auto target = static_cast<Object*>(iter)->getOwn("target");
            if(!target || target->type != Value::Obj)
                throw new JuaError("pairs iterator is invalid");
            jualist nextArgs{target, args[1]};
            auto res = obj_next->call(nextArgs);
            auto done = res->getProp("done");
            if(done && done->toBoolean())return res;
            auto key = res->getProp("key");
            auto pair = new Array(this, {});
            pair->items.push_back(key);
            pair->items.push_back(static_cast<Object*>(target)->getOwn(key->toString()));
            auto out = new Object(this);
            out->setProp("done", Boolean::getInst(false));
            out->setProp("key", key);
            out->setProp("value", pair);
            return out;
        }));
        auto iter = new Object(this, itProto);
        iter->setProp("target", self);
        return iter;
    }));
    proto->setProp("hasOwn", makeFunc([this](jualist& args){
        if(args.size() < 2) throw new JuaTypeError("Object.hasOwn() requires 2 arguments");
        auto self = args[0];
        if(self->type != Value::Obj){
            throw new JuaError("Object.hasOwn() called on a non-object");
        }
        auto obj = static_cast<Object*>(self);
        auto key = args[1];
        if(key->type != Value::Str){
            throw new JuaError("Object.hasOwn() requires a string key");
        }
        return Boolean::getInst(obj->hasOwn(key));
    }));
    proto->setProp("set", makeFunc([](jualist& args){
        if(args.size() < 3) throw new JuaTypeError("Object.set() requires 3 arguments");
        auto self = args[0];
        if(self->type != Value::Obj){
            throw new JuaError("Object.set() called on a non-object");
        }
        auto obj = static_cast<Object*>(self);
        auto key = args[1];
        if(key->type != Value::Str){
            throw new JuaError("Object.set() requires a string key");
        }
        auto value = args[2];
        obj->setProp(key->toString(), value);
        return Null::getInst();
    }));
    proto->setProp("del", makeFunc([](jualist& args){
        if(args.size() < 2) throw new JuaTypeError("Object.del() requires 2 arguments");
        auto self = args[0];
        if(self->type != Value::Obj){
            throw new JuaError("Object.del() called on a non-object");
        }
        auto obj = static_cast<Object*>(self);
        auto key = args[1];
        if(key->type != Value::Str){
            throw new JuaError("Object.del() requires a string key");
        }
        obj->delProp(key->toString());
        return Null::getInst();
    }));
    proto->setProp("getProto", makeFunc([](jualist& args){
        if(args.size() < 1) throw new JuaError("Object.getProto() requires 1 argument");
        return args[0]->proto; // nullptr 会自动转换为 Null
    }));
    proto->setProp("setProto", makeFunc([](jualist& args){
        if(args.size() < 2) throw new JuaError("Object.setProto() requires 2 arguments");
        auto self = args[0];
        if(self->type != Value::Obj){
            throw new JuaError("Object.setProto() called on a non-object");
        }
        auto obj = static_cast<Object*>(self);
        auto proto = args[1];
        if(proto->type != Value::Obj && proto->type != Value::Null){
            throw new JuaError("Object.setProto() requires an object or null prototype");
        }
        obj->proto = static_cast<Object*>(proto);
        return Null::getInst();
    }));
    proto->setProp("next", obj_next);
    proto->setProp("toString", makeFunc([](jualist& args){
        if(args.size() < 1) throw new JuaTypeError("Object.toString() requires 1 argument");
        auto self = args[0];
        if(self->type != Value::Obj){
            throw new JuaError("Object.toString() called on a non-object");
        }
        return new String(self->vm, self->safeToString());
    }));
    proto->setProp("id", makeFunc([this](jualist& args){
        if(args.size() < 1) throw new JuaTypeError("Object.id() requires 1 argument");
        auto self = args[0];
        if(self->type != Value::Obj && self->type != Value::Func){
            throw new JuaTypeError("Object.id() expected an object or function");
        }
        if(self->id == (size_t)-1){
            self->id = ++idcounter; // 分配一个唯一的 ID
        }
        return new Number(self->vm, self->id);
    }));
    return proto;
}
NativeFunction* VM::makeEncodeFunc(Encoder encode){
    return makeFunc([this, encode](jualist& args){
        auto str = new String(this, "");
        for(auto v: args) {
            if(v->type != Value::Num){
                throw new JuaError("encode() requires number arguments");
            }
            str->value += encode(v->toNumber());
        }
        return str;
    });
}
NativeFunction* VM::makeDecodeFunc(Decoder decode){
    return makeFunc([this, decode](jualist& args){
        if(args.size() < 1) throw new JuaTypeError("decode() requires at least one argument");
        Value* val = args[0];
        if(val->type != Value::Str){
            throw new JuaError("decode() requires a string argument");
        }
        double result;
        try{
            result = decode(val->toString());
        } catch (size_t size) {
            throw new JuaError(std::format("decode() requires a string of at least %zu bytes", size));
        }
        return new Number(this, result);
    });
}

Object* VM::makeArrayProto(){
    auto proto = buildClass([this](jualist& args){
        if(!args.size())throw new JuaError("Missing argument");
        auto val = args[0];
        auto arr = new Array(this, {});
        val->collectItems(arr->items);
        return arr;
    });
    proto->setProp("hasItem", makeFunc([](jualist& args){
        if(args.size() < 2) throw new JuaError("Array.hasItem() requires 2 arguments");
        auto self = args[0];
        if(!self->isType(Array::type_id)){
            throw new JuaError("Array.hasItem() called on a non-array object");
        }
        auto arr = static_cast<Array*>(self);
        auto item = args[1];
        for(auto v: arr->items){
            if(*v == item){
                return Boolean::getInst(true);
            }
        }
        return Boolean::getInst(false);
    }));
    proto->setProp("join", makeFunc([](jualist& args){
        if(args.size() < 1) throw new JuaError("Array.join() requires at least 1 argument");
        auto self = args[0];
        string sep = "";
        if(args.size() >= 2){
            sep = args[1]->toString();
        }
        string result;
        auto iter = self->getIterator();
        bool first = true;
        while(auto value = iter->next()){
            if(first) first = false;
            else result += sep;
            result += value->toString();
        }
        delete iter;
        return new String(self->vm, result);
    }));
    proto->setProp("len", makeFunc([](jualist& args){
        if(args.size() < 1) throw new JuaError("Array.len() requires 1 argument");
        auto self = args[0];
        if(!self->isType(Array::type_id)){
            throw new JuaError("Array.len() called on a non-array object");
        }
        auto arr = static_cast<Array*>(self);
        return new Number(self->vm, arr->items.size());
    }));
    proto->setProp("of", makeFunc([this](jualist& args){
        return new Array(this, args);
    }));
    proto->setProp("pop", makeFunc([](jualist& args) -> Value* {
        if(args.size() < 1) throw new JuaError("Array.pop() requires 1 argument");
        auto self = args[0];
        if(!self->isType(Array::type_id)){
            throw new JuaError("Array.pop() called on a non-array object");
        }
        auto arr = static_cast<Array*>(self);
        if(arr->items.empty()){
            return Null::getInst();
        }
        auto val = arr->items.back();
        arr->items.pop_back();
        return val;
    }));
    proto->setProp("push", makeFunc([](jualist& args){
        if(args.size() < 2) throw new JuaError("Array.push() requires at least 1 argument");
        auto self = args[0];
        if(!self->isType(Array::type_id)){
            throw new JuaError("Array.push() called on a non-array object");
        }
        auto arr = static_cast<Array*>(self);
        for(size_t i=1; i<args.size(); i++){
            arr->items.push_back(args[i]);
        }
        return Null::getInst();
    }));
    proto->setProp("shift", makeFunc([](jualist& args) -> Value* {
        if(args.size() < 1) throw new JuaError("Array.shift() requires 1 argument");
        auto self = args[0];
        if(!self->isType(Array::type_id)){
            throw new JuaError("Array.shift() called on a non-array object");
        }
        auto arr = static_cast<Array*>(self);
        if(arr->items.empty()){
            return Null::getInst();
        }
        auto val = arr->items.front();
        arr->items.pop_front();
        return val;
    }));
    proto->setProp("splice", makeFunc([](jualist& args){
        if(args.size() < 3) throw new JuaError("Array.splice() requires at least 3 arguments");
        auto self = args[0];
        if(!self->isType(Array::type_id)){
            throw new JuaError("Array.splice() called on a non-array object");
        }
        auto arr = static_cast<Array*>(self);
        int start = args[1]->toInt();
        int deleteCount = args[2]->toInt();
        if(start < 0) start = std::max(0, (int)arr->items.size() + start);
        if(deleteCount < 0) deleteCount = 0;
        if(start > (int)arr->items.size()) start = arr->items.size();
        if(start + deleteCount > (int)arr->items.size()){
            deleteCount = arr->items.size() - start;
        }
        //todo: 优化
        auto it = arr->items.begin();
        std::advance(it, start);
        for(int i=0; i<deleteCount; i++){
            it = arr->items.erase(it);
        }
        for(size_t i=3; i<args.size(); i++){
            it = arr->items.insert(it, args[i]);
            it++;
        }
        return Null::getInst();
    }));
    proto->setProp("toString", makeFunc([](jualist& args){
        if(args.size() < 1) throw new JuaError("Array.toString() requires 1 argument");
        auto self = args[0];
        //仅要求可迭代
        auto iter = self->getIterator();
        string result = "[";
        while(auto value = iter->next()){
            if(result.size() > 1) result += ", ";
            result += value->toString();
        }
        result += "]";
        return new String(self->vm, result);
    }));
    proto->setProp("unshift", makeFunc([](jualist& args){
        if(args.size() < 2) throw new JuaError("Array.unshift() requires at least 1 argument");
        auto self = args[0];
        if(!self->isType(Array::type_id)){
            throw new JuaError("Array.unshift() called on a non-array object");
        }
        auto arr = static_cast<Array*>(self);
        for(size_t i=args.size()-1; i>=1; i--){
            arr->items.push_front(args[i]);
        }
        return Null::getInst();
    }));
    return proto;
}
Object* VM::makeBufferProto(){
    auto proto = buildClass([this](jualist& args){
        if(!args.size())
            throw new JuaError("Missing argument");
        auto val = args[0];
        auto buf = new Buffer(this, val->toInt());
        return buf;
    });
    proto->setProp("read", makeFunc([](jualist& args){
        if(!args.size()) throw new JuaError("Buffer.read() requires at least 1 argument");
        auto self = args[0],
            start = args.size()>1 ? args[1] : nullptr,
            end = args.size()>2 ? args[2] : nullptr;
        if(!self->isType(Buffer::type_id)){
            throw new JuaError("Buffer.read() called on a improper value");
        }
        auto buf = static_cast<Buffer*>(self);
        return buf->read(start, end);
    }));
    proto->setProp("write", makeFunc([](jualist& args){
        if(args.size() < 2) throw new JuaError("Buffer.write() requires at least 2 arguments");
        auto self = args[0], data = args[1], pos = args.size() > 2 ? args[2] : nullptr;
        if(!self->isType(Buffer::type_id)){
            throw new JuaError("Buffer.write() called on a improper value");
        }
        auto buf = static_cast<Buffer*>(self);
        buf->write(data, pos);
        return Null::getInst();
    }));
    return proto;
}
Object* VM::makeErrorProto(){
    auto proto = new Object(this, classProto);
    proto->setProp("init", makeFunc([this](jualist& args){
        auto self = args[0];
        if(self->type != Value::Obj)
            throw new JuaError("Error.init() called on a non-object");
        string message;
        if(args.size() > 1){
            message = args[1]->toString();
        }
        auto err = static_cast<Object*>(self);
        err->setProp("message", new String(this, message));
        return Null::getInst();
    }));
    proto->setProp("name", new String(this, "Error"));
    proto->setProp("toString", makeFunc([](jualist& args){
        if(args.size() < 1) throw new JuaError("Error.toString() requires 1 argument");
        auto self = args[0];
        if(self->type != Value::Obj)
            throw new JuaError("Error.toString() called on a non-object");
        auto err = static_cast<Object*>(self);
        auto name = err->getProp("name");
        string prefix = name ? name->toString() : "Error";
        auto msg = err->getProp("message");
        string message = msg ? msg->toString() : "unknown";
        return new String(self->vm, prefix + ": " + message);
    }));
    return proto;
}
Object* VM::makeTryResProto(){
    auto proto = new Object(this);
    proto->setProp("catch", makeFunc([this](jualist& args) -> Value* {
        if(args.size() < 2) throw new JuaError("TryRes.catch() requires 2 arguments");
        auto self = args[0], fn = args[1]; //todo: catch(self, ErrorType, fn)
        if(self->type != Value::Obj)
            throw new JuaError("TryRes.catch() called on a non-object");
        auto res = static_cast<Object*>(self);
        auto status = res->getProp("status");
        if(!status || status->type != Value::Bool)
            throw new JuaError("TryRes.catch() called on an invalid object");
        if(status->toBoolean()){
            return res;
        }
        if(fn->type != Value::Func)
            throw new JuaError("TryRes.catch() requires a function argument");
        auto error = res->getProp("error");
        if(!error)
            throw new JuaError("TryRes.catch() called on an invalid object");
        return fn->call({error});
    }));
    return proto;
}
