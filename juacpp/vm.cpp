#include "jua-vm.h"
#include "jua-syntax.h"
#include "coding.h"

JuaVM::JuaVM(){
    initBuiltins();
    makeGlobal();
}

void JuaVM::run(const string& script){
    try{
        auto val = eval(script);
        val->gc();
    }catch(JuaError* e){
        j_stderr(e);
    }
}

Jua_Val* JuaVM::eval(const string& script){
    //返回非空指针，可能需要垃圾回收
    //d_log("eval");
    //d_log(script);
    auto body = parse(script);
    return body->exec(new Scope(_G));
}
void JuaVM::initBuiltins(){
    obj_new = makeFunc([this](jualist& args){
        Jua_Val* proto = nullptr;
        if(args.size() > 0){
            proto = args[0];
            if(proto->type == Jua_Val::Null){
                proto = nullptr;
            }else if(proto->type != Jua_Val::Obj){
                throw new JuaError("Object.new() requires an object prototype");
            }
            args.pop_front();
        }
        auto obj = new Jua_Obj(this, static_cast<Jua_Obj*>(proto));
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
        if(args.size() < 2) throw "Range.next() requires 2 arguments";
        auto self = args[0];
        if(self->type != Jua_Val::Obj){
            throw new JuaError("Range.next() called on a non-object");
        }
        auto obj = static_cast<Jua_Obj*>(self);
        auto key = args[1];
        std::unordered_map<string, Jua_Val*>::iterator it;
        if(key->type == Jua_Val::Str){
            string str_key = key->toString();
            it = obj->dict.find(str_key);
            it++;
        }else{
            it = obj->dict.begin();
        }
        auto res = new Jua_Obj(self->vm);
        if(it == obj->dict.end()){
            res->setProp("done", Jua_Bool::getInst(true));
        }else{
            auto value = new Jua_Str(self->vm, it->first);
            res->setProp("done", Jua_Bool::getInst(false));
            res->setProp("key", value);
            res->setProp("value", value);
        }
        return res;
    });
    classProto = new Jua_Obj(this); //标准可构造类的原型，首先初始化
    classProto->setProp("__class", Jua_Bool::getInst(true));
    classProto->setProp("__call", obj_new);

    RangeProto = makeRangeProto(); //必须在 NumberProto 之前
    NumberProto = makeNumberProto();
    StringProto = makeStringProto();
    BooleanProto = new Jua_Obj(this);
    FunctionProto = new Jua_Obj(this);
    ObjectProto = makeObjectProto();

    ArrayProto = new Jua_Obj(this);
    BufferProto = new Jua_Obj(this);

}
void JuaVM::makeGlobal(){
    _G = new Scope(this);
    _G->setProp("String", StringProto);
    _G->setProp("Number", NumberProto);
    _G->setProp("Boolean", BooleanProto);
    _G->setProp("Function", FunctionProto);
    _G->setProp("Object", ObjectProto);
    _G->setProp("Array", ArrayProto);
    _G->setProp("Buffer", BufferProto);
    _G->setProp("Range", RangeProto);
    _G->addRef();
    _G->setProp("_G", _G);
    _G->setProp("print", makeFunc([this](jualist& args){
        j_stdout(args);
        return Jua_Null::getInst();
    }));
    _G->setProp("class", makeFunc([this](jualist& args){
        if(args.size() < 1) throw "class() requires at least one argument";
        Jua_Val* proto = args[0];
        if(proto->type != Jua_Val::Obj){
            throw new JuaError("class() requires an object prototype");
        }
        proto->proto = classProto; //todo: 如果采用引用计数，需要设置一个统一的入口来修改 proto
        return proto;
    }));
    _G->setProp("require", makeFunc([this](jualist& args){
        if(args.size() < 1) throw "require() requires at least one argument";
        if(args[0]->type != Jua_Val::Str){
            throw new JuaError("require() requires a string argument");
        }
        return require(args[0]->toString());
    }));
    _G->setProp("throw", makeFunc([this](jualist& args){
        if(args.size() < 1) throw "throw() requires at least one argument";
        Jua_Val* val = args[0];
        //todo: 允许抛出对象
        if(val->type != Jua_Val::Str){
            throw new JuaError("throw() requires a string argument");
        }
        throw new JuaError(val->toString());
        return Jua_Null::getInst(); // unreachable, but required for function signature
    }));
    _G->setProp("try", makeFunc([this](jualist& args){
        auto res = new Jua_Obj(this); //todo: 设置原型
        try{
            if(args.size() < 1) throw "try() requires at least one argument";
            Jua_Val* fn = args[0];
            if(fn->type != Jua_Val::Func){
                throw new JuaError("try() requires a function argument");
            }
            args.pop_front();
            res->setProp("value", fn->call(args));
            res->setProp("status", Jua_Bool::getInst(true));
            return res;
        } catch (JuaError* e) {
            res->setProp("error", new Jua_Str(this, e->toDebugString()));
            res->setProp("status", Jua_Bool::getInst(false));
            return res;
        }
    }));
    _G->setProp("type", makeFunc([this](jualist& args){
        if(args.size() < 1) throw "type() requires at least one argument";
        Jua_Val* val = args[0];
        auto typeName = val->getTypeName();
        return new Jua_Str(this, typeName);
    }));
}
Jua_Val* JuaVM::require(const string& name){
    if(modules.contains(name))return modules[name];
    //todo: 检查循环导入
    const string& script = findModule(name);
    modules[name] = eval(script);
    return modules[name];
}

Jua_Obj* JuaVM::buildClass(Jua_NativeFunc::Native constructor){
    //constructor 接收初始化参数（不包括类自身），返回类实例
    auto clsProto = new Jua_Obj(this);
    clsProto->setProp("__class", Jua_Bool::getInst(true));
    clsProto->setProp("__call", makeFunc([this, constructor](jualist& args){
        args.pop_front(); // 移除类本身
        return constructor(args);
    }));
    auto cls = new Jua_Obj(this, clsProto);
    return cls;
}

Jua_Obj* JuaVM::makeRangeProto(){
    auto proto = new Jua_Obj(this, classProto);
    proto->setProp("init", makeFunc([](jualist& args){
        if(args.size() < 3) throw "range() requires 3 arguments";
        auto self = args[0];
        if(self->type != Jua_Val::Obj)throw new JuaError("range() called on non-object value");
        auto obj = static_cast<Jua_Obj*>(self);
        double start = args[1]->toNumber();
        double end = args[2]->toNumber();
        double step = 1;
        if(args.size() >= 4){
            step = args[4]->toNumber();
        }
        obj->setProp("start", new Jua_Num(self->vm, start));
        obj->setProp("end", new Jua_Num(self->vm, end));
        obj->setProp("step", new Jua_Num(self->vm, step));
        return Jua_Null::getInst();
    }));
    proto->setProp("next", makeFunc([](jualist& args){
        //todo: 支持浮点数
        if(args.size() < 2) throw "Range.next() requires 2 arguments";
        auto self = args[0];
        if(self->type != Jua_Val::Obj){
            throw new JuaError("Range.next() called on a non-object");
        }
        auto key = args[1];
        size_t index;
        if(key->type == Jua_Val::Num){
            auto step = self->getProp("step");
            if(!step || step->type != Jua_Val::Num)
                throw new JuaError("Range.next() called on a non-range object");
            index = key->toInt() + step->toInt();
        }else{
            auto start = self->getProp("start");
            if(!start || start->type != Jua_Val::Num)
                throw new JuaError("Range.next() called on a non-range object");
            index = start->toInt();
        }
        
        auto end = self->getProp("end");
        if(!end || end->type != Jua_Val::Num)
            throw new JuaError("Range.next() called on a non-range object");
        auto res = new Jua_Obj(self->vm);
        bool done = index >= end->toInt();
        res->setProp("done", Jua_Bool::getInst(done));
        if(!done){
            auto value = new Jua_Num(self->vm, index);
            res->setProp("value", value);
            res->setProp("key", value);
        }
        return res;
    }));
    return proto;
}
Jua_Obj* JuaVM::makeNumberProto(){
    auto proto = buildClass([this](jualist& args){
        double value = 0.0;
        if(args.size() > 0){
            Jua_Val* arg = args[0];
            if(arg->type == Jua_Val::Num){
                value = static_cast<Jua_Num*>(arg)->value;
            }else if(arg->type == Jua_Val::Str){
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
        return new Jua_Num(this, value);
    });
    proto->setProp("LITTLE_ENDIAN", Jua_Bool::getInst(isLittleEndian()));
    proto->setProp("range", RangeProto);
    proto->setProp("__range", makeFunc([this](jualist& args){
        if(args.size() < 2)throw new JuaError("requires at least 2 arguments");
        return RangeProto->call(args);
    }));
    proto->setProp("toString", makeFunc([](jualist& args){
        if(args.size() < 1) throw new JuaError("Number.toString() requires 1 argument");
        auto self = args[0];
        if(self->type != Jua_Val::Num){
            throw new JuaError("Number.toString() called on a non-number");
        }
        return new Jua_Str(self->vm, self->toString());
    }));
    proto->setProp("isInt", makeFunc([](jualist& args){
        if(args.size() < 1) throw new JuaError("Number.isInt() requires 1 argument");
        auto val = args[0];
        if(val->type != Jua_Val::Num)return Jua_Bool::getInst(false);
        throw "todo";
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
Jua_Obj* JuaVM::makeStringProto(){
    auto proto = buildClass([this](jualist& args){
        string value;
        if(args.size() > 0){
            value = args[0]->toString();
        }
        return new Jua_Str(this, value);
    });
    proto->setProp("fromByte", makeFunc([this](jualist& args){
        string str;
        str.reserve(args.size());
        for(auto v: args){
            str.push_back(v->toInt());
        }
        return new Jua_Str(this, str);
    }));
    proto->setProp("len", makeFunc([](jualist& args){
        if(args.size() < 1)throw new JuaError("missing argument");
        auto val = args[0];
        if(val->type != Jua_Val::Str)
            throw new JuaError("String.len() called on non-string value");
        auto str = static_cast<Jua_Str*>(val);
        return new Jua_Num(val->vm, str->value.size());
    }));
    proto->setProp("toHex", makeFunc([](jualist& args){
        static const char hexDigits[] = "0123456789abcdef";

        if(!args.size())throw new JuaError("String.toHex() requires at least 1 argument");
        auto self = args[0];
        if(self->type != Jua_Val::Str)throw new JuaError("String.toHex() called on non-string value");
        auto str = self->toString();
        string hexStr;
        hexStr.reserve(str.size() * 3);
        for (uint8_t byte: str) {
            hexStr.push_back(hexDigits[byte >> 4]);  // 高4位
            hexStr.push_back(hexDigits[byte & 0x0F]); // 低4位
            hexStr.push_back(' ');
        }
        hexStr.pop_back();
        return new Jua_Str(self->vm, hexStr);
    }));
    return proto;
}
Jua_Obj* JuaVM::makeObjectProto(){
    auto proto = new Jua_Obj(this, classProto);
    proto->setProp("new", obj_new);
    proto->setProp("get", makeFunc([](jualist& args){
        if(args.size() < 2) throw "Object.get() requires 2 arguments";
        auto self = args[0];
        if(self->type != Jua_Val::Obj){
            throw new JuaError("Object.get() called on a non-object");
        }
        auto obj = static_cast<Jua_Obj*>(self);
        auto key = args[1];
        if(key->type != Jua_Val::Str){
            throw new JuaError("Object.get() requires a string key");
        }
        return obj->getOwn(key->toString());
    }));
    proto->setProp("hasOwn", makeFunc([this](jualist& args){
        if(args.size() < 2) throw "Object.hasOwn() requires 2 arguments";
        auto self = args[0];
        if(self->type != Jua_Val::Obj){
            throw new JuaError("Object.hasOwn() called on a non-object");
        }
        auto obj = static_cast<Jua_Obj*>(self);
        auto key = args[1];
        if(key->type != Jua_Val::Str){
            throw new JuaError("Object.hasOwn() requires a string key");
        }
        return Jua_Bool::getInst(obj->hasOwn(key));
    }));
    proto->setProp("set", makeFunc([](jualist& args){
        if(args.size() < 3) throw "Object.set() requires 3 arguments";
        auto self = args[0];
        if(self->type != Jua_Val::Obj){
            throw new JuaError("Object.set() called on a non-object");
        }
        auto obj = static_cast<Jua_Obj*>(self);
        auto key = args[1];
        if(key->type != Jua_Val::Str){
            throw new JuaError("Object.set() requires a string key");
        }
        auto value = args[2];
        obj->setProp(key->toString(), value);
        return Jua_Null::getInst();
    }));
    proto->setProp("del", makeFunc([](jualist& args){
        if(args.size() < 2) throw "Object.del() requires 2 arguments";
        auto self = args[0];
        if(self->type != Jua_Val::Obj){
            throw new JuaError("Object.del() called on a non-object");
        }
        auto obj = static_cast<Jua_Obj*>(self);
        auto key = args[1];
        if(key->type != Jua_Val::Str){
            throw new JuaError("Object.del() requires a string key");
        }
        obj->delProp(key->toString());
        return Jua_Null::getInst();
    }));
    proto->setProp("getProto", makeFunc([](jualist& args){
        if(args.size() < 1) throw new JuaError("Object.getProto() requires 1 argument");
        return args[0]->proto; // nullptr 会自动转换为 Jua_Null
    }));
    proto->setProp("setProto", makeFunc([](jualist& args){
        if(args.size() < 2) throw new JuaError("Object.setProto() requires 2 arguments");
        auto self = args[0];
        if(self->type != Jua_Val::Obj){
            throw new JuaError("Object.setProto() called on a non-object");
        }
        auto obj = static_cast<Jua_Obj*>(self);
        auto proto = args[1];
        if(proto->type != Jua_Val::Obj && proto->type != Jua_Val::Null){
            throw new JuaError("Object.setProto() requires an object or null prototype");
        }
        obj->proto = static_cast<Jua_Obj*>(proto);
        return Jua_Null::getInst();
    }));
    proto->setProp("next", obj_next);
    proto->setProp("toString", makeFunc([](jualist& args){
        if(args.size() < 1) throw "Object.toString() requires 1 argument";
        auto self = args[0];
        if(self->type != Jua_Val::Obj){
            throw new JuaError("Object.toString() called on a non-object");
        }
        return new Jua_Str(self->vm, self->safeToString());
    }));
    proto->setProp("id", makeFunc([this](jualist& args){
        if(args.size() < 1) throw "Object.id() requires 1 argument";
        auto self = args[0];
        if(self->type != Jua_Val::Obj && self->type != Jua_Val::Func){
            throw new JuaError("Object.id() expected an object or function");
        }
        if(self->id == -1){
            self->id = ++idcounter; // 分配一个唯一的 ID
        }
        return new Jua_Num(self->vm, self->id);
    }));
    return proto;
}
Jua_NativeFunc* JuaVM::makeEncodeFunc(Encoder encode){
    return makeFunc([this, encode](jualist& args){
        if(args.size() < 1) throw "encode() requires at least one argument";
        Jua_Val* val = args[0];
        if(val->type != Jua_Val::Num){
            throw new JuaError("encode() requires a number argument");
        }
        string result;
        encode(val->toNumber(), result);
        return new Jua_Str(this, result);
    });
}
Jua_NativeFunc* JuaVM::makeDecodeFunc(Decoder decode){
    return makeFunc([this, decode](jualist& args){
        if(args.size() < 1) throw "decode() requires at least one argument";
        Jua_Val* val = args[0];
        if(val->type != Jua_Val::Str){
            throw new JuaError("decode() requires a string argument");
        }
        double result;
        try{
            result = decode(val->toString());
        } catch (size_t size) {
            throw new JuaError(std::format("decode() requires a string of at least %zu bytes", size));
        }
        return new Jua_Num(this, result);
    });
}