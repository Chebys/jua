#include "jua-value.h"
#include "jua-vm.h"

Object* VM::makeMath(){
    auto math = new Object(this);
    math->setProp("PI", new Number(this, 3.141592653589793));
    math->setProp("E", new Number(this, 2.718281828459045));
    math->setProp("sin", makeFunc([](jualist& args){
        if(args.size() < 1)throw new JuaError("Math.sin() requires 1 argument");
        auto val = args[0];
        if(val->type != Value::Num)
            throw new JuaError("Math.sin() called on non-number value");
        auto num = static_cast<Number*>(val);
        return new Number(val->vm, sin(num->value));
    }));
    math->setProp("cos", makeFunc([](jualist& args){
        if(args.size() < 1)throw new JuaError("Math.cos() requires 1 argument");
        auto val = args[0];
        if(val->type != Value::Num)
            throw new JuaError("Math.cos() called on non-number value");
        auto num = static_cast<Number*>(val);
        return new Number(val->vm, cos(num->value));
    }));
    math->setProp("tan", makeFunc([](jualist& args){
        if(args.size() < 1)throw new JuaError("Math.tan() requires 1 argument");
        auto val = args[0];
        if(val->type != Value::Num)
            throw new JuaError("Math.tan() called on non-number value");
        auto num = static_cast<Number*>(val);
        return new Number(val->vm, tan(num->value));
    }));
    math->setProp("sqrt", makeFunc([](jualist& args){
        if(args.size() < 1)throw new JuaError("Math.sqrt() requires 1 argument");
        auto val = args[0];
        if(val->type != Value::Num)
            throw new JuaError("Math.sqrt() called on non-number value");
        auto num = static_cast<Number*>(val);
        if(num->value < 0)
            throw new JuaError("Math.sqrt() called on negative number");
        return new Number(val->vm, sqrt(num->value));
    }));
    math->setProp("log", makeFunc([](jualist& args){
        if(args.size() < 1)throw new JuaError("Math.log() requires 1 argument");
        auto val = args[0];
        if(val->type != Value::Num)
            throw new JuaError("Math.log() called on non-number value");
        auto num = static_cast<Number*>(val);
        if(num->value <= 0)
            throw new JuaError("Math.log() called on non-positive number");
        return new Number(val->vm, log(num->value));
    }));
    math->setProp("exp", makeFunc([](jualist& args){
        if(args.size() < 1)throw new JuaError("Math.exp() requires 1 argument");
        auto val = args[0];
        if(val->type != Value::Num)
            throw new JuaError("Math.exp() called on non-number value");
        auto num = static_cast<Number*>(val);
        return new Number(val->vm, exp(num->value));
    }));
    math->setProp("ceil", makeFunc([](jualist& args){
        if(args.size() < 1)throw new JuaError("Math.ceil() requires 1 argument");
        auto val = args[0];
        if(val->type != Value::Num)
            throw new JuaError("Math.ceil() called on non-number value");
        auto num = static_cast<Number*>(val);
        return new Number(val->vm, ceil(num->value));
    }));
    math->setProp("floor", makeFunc([](jualist& args){
        if(args.size() < 1)throw new JuaError("Math.floor() requires 1 argument");
        auto val = args[0];
        if(val->type != Value::Num)
            throw new JuaError("Math.floor() called on non-number value");
        auto num = static_cast<Number*>(val);
        return new Number(val->vm, floor(num->value));
    }));
    math->setProp("round", makeFunc([](jualist& args){
        if(args.size() < 1)throw new JuaError("Math.round() requires 1 argument");
        auto val = args[0];
        if(val->type != Value::Num)
            throw new JuaError("Math.round() called on non-number value");
        auto num = static_cast<Number*>(val);
        return new Number(val->vm, round(num->value));
    }));
    return math;
}