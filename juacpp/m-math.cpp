#include "jua-value.h"
#include "jua-vm.h"
#include <random>
#include <limits>

Object* VM::makeMath(){
    auto math = new Object(this);
    math->setProp("PI", new Number(this, 3.141592653589793));
    math->setProp("E", new Number(this, 2.718281828459045));

    // 三角函数（弧度）
    math->setProp("sin", makeFunc([](jualist& args){
        if(args.size() < 1)throw new JuaTypeError("Math.sin() requires 1 argument");
        auto val = args[0];
        if(val->type != Value::Num)
            throw new JuaTypeError("Math.sin() called on non-number value");
        auto num = static_cast<Number*>(val);
        return new Number(val->vm, sin(num->value));
    }));
    math->setProp("cos", makeFunc([](jualist& args){
        if(args.size() < 1)throw new JuaTypeError("Math.cos() requires 1 argument");
        auto val = args[0];
        if(val->type != Value::Num)
            throw new JuaTypeError("Math.cos() called on non-number value");
        auto num = static_cast<Number*>(val);
        return new Number(val->vm, cos(num->value));
    }));
    math->setProp("tan", makeFunc([](jualist& args){
        if(args.size() < 1)throw new JuaTypeError("Math.tan() requires 1 argument");
        auto val = args[0];
        if(val->type != Value::Num)
            throw new JuaTypeError("Math.tan() called on non-number value");
        auto num = static_cast<Number*>(val);
        return new Number(val->vm, tan(num->value));
    }));
    // 反三角函数（弧度）
    math->setProp("asin", makeFunc([](jualist& args){
        if(args.size() < 1)throw new JuaTypeError("Math.asin() requires 1 argument");
        auto val = args[0];
        if(val->type != Value::Num)
            throw new JuaTypeError("Math.asin() called on non-number value");
        auto num = static_cast<Number*>(val);
        if(num->value < -1 || num->value > 1)
            throw new JuaRangeError("Math.asin() called on value outside [-1, 1]");
        return new Number(val->vm, asin(num->value));
    }));
    math->setProp("acos", makeFunc([](jualist& args){
        if(args.size() < 1)throw new JuaTypeError("Math.acos() requires 1 argument");
        auto val = args[0];
        if(val->type != Value::Num)
            throw new JuaTypeError("Math.acos() called on non-number value");
        auto num = static_cast<Number*>(val);
        if(num->value < -1 || num->value > 1)
            throw new JuaRangeError("Math.acos() called on value outside [-1, 1]");
        return new Number(val->vm, acos(num->value));
    }));
    math->setProp("atan", makeFunc([](jualist& args){
        if(args.size() < 1)throw new JuaTypeError("Math.atan() requires 1 argument");
        auto val = args[0];
        if(val->type != Value::Num)
            throw new JuaTypeError("Math.atan() called on non-number value");
        auto num = static_cast<Number*>(val);
        return new Number(val->vm, atan(num->value));
    }));

    // 平方根：参数必须非负
    math->setProp("sqrt", makeFunc([](jualist& args){
        if(args.size() < 1)throw new JuaTypeError("Math.sqrt() requires 1 argument");
        auto val = args[0];
        if(val->type != Value::Num)
            throw new JuaTypeError("Math.sqrt() called on non-number value");
        auto num = static_cast<Number*>(val);
        if(num->value < 0)
            throw new JuaRangeError("Math.sqrt() called on negative number");
        return new Number(val->vm, sqrt(num->value));
    }));

    // 对数：参数必须为正
    math->setProp("log", makeFunc([](jualist& args){
        if(args.size() < 1)throw new JuaTypeError("Math.log() requires 1 argument");
        auto val = args[0];
        if(val->type != Value::Num)
            throw new JuaTypeError("Math.log() called on non-number value");
        auto num = static_cast<Number*>(val);
        if(num->value <= 0)
            throw new JuaRangeError("Math.log() called on non-positive number");
        return new Number(val->vm, log(num->value));
    }));
    math->setProp("log2", makeFunc([](jualist& args){
        if(args.size() < 1)throw new JuaTypeError("Math.log2() requires 1 argument");
        auto val = args[0];
        if(val->type != Value::Num)
            throw new JuaTypeError("Math.log2() called on non-number value");
        auto num = static_cast<Number*>(val);
        if(num->value <= 0)
            throw new JuaRangeError("Math.log2() called on non-positive number");
        return new Number(val->vm, log2(num->value));
    }));
    math->setProp("log10", makeFunc([](jualist& args){
        if(args.size() < 1)throw new JuaTypeError("Math.log10() requires 1 argument");
        auto val = args[0];
        if(val->type != Value::Num)
            throw new JuaTypeError("Math.log10() called on non-number value");
        auto num = static_cast<Number*>(val);
        if(num->value <= 0)
            throw new JuaRangeError("Math.log10() called on non-positive number");
        return new Number(val->vm, log10(num->value));
    }));

    // e 的幂
    math->setProp("exp", makeFunc([](jualist& args){
        if(args.size() < 1)throw new JuaTypeError("Math.exp() requires 1 argument");
        auto val = args[0];
        if(val->type != Value::Num)
            throw new JuaTypeError("Math.exp() called on non-number value");
        auto num = static_cast<Number*>(val);
        return new Number(val->vm, exp(num->value));
    }));

    // 绝对值
    math->setProp("abs", makeFunc([](jualist& args){
        if(args.size() < 1)throw new JuaTypeError("Math.abs() requires 1 argument");
        auto val = args[0];
        if(val->type != Value::Num)
            throw new JuaTypeError("Math.abs() called on non-number value");
        auto num = static_cast<Number*>(val);
        return new Number(val->vm, fabs(num->value));
    }));

    // 取整
    math->setProp("ceil", makeFunc([](jualist& args){
        if(args.size() < 1)throw new JuaTypeError("Math.ceil() requires 1 argument");
        auto val = args[0];
        if(val->type != Value::Num)
            throw new JuaTypeError("Math.ceil() called on non-number value");
        auto num = static_cast<Number*>(val);
        return new Number(val->vm, ceil(num->value));
    }));
    math->setProp("floor", makeFunc([](jualist& args){
        if(args.size() < 1)throw new JuaTypeError("Math.floor() requires 1 argument");
        auto val = args[0];
        if(val->type != Value::Num)
            throw new JuaTypeError("Math.floor() called on non-number value");
        auto num = static_cast<Number*>(val);
        return new Number(val->vm, floor(num->value));
    }));
    // 四舍五入，恰好一半时远离零
    math->setProp("round", makeFunc([](jualist& args){
        if(args.size() < 1)throw new JuaTypeError("Math.round() requires 1 argument");
        auto val = args[0];
        if(val->type != Value::Num)
            throw new JuaTypeError("Math.round() called on non-number value");
        auto num = static_cast<Number*>(val);
        return new Number(val->vm, round(num->value));
    }));
    // 截断（向零取整）
    math->setProp("trunc", makeFunc([](jualist& args){
        if(args.size() < 1)throw new JuaTypeError("Math.trunc() requires 1 argument");
        auto val = args[0];
        if(val->type != Value::Num)
            throw new JuaTypeError("Math.trunc() called on non-number value");
        auto num = static_cast<Number*>(val);
        return new Number(val->vm, trunc(num->value));
    }));

    // 最大值：接受任意数量数字参数
    math->setProp("max", makeFunc([](jualist& args){
        if(args.size() < 1)throw new JuaTypeError("Math.max() requires at least 1 argument");
        double result = -std::numeric_limits<double>::infinity();
        for(auto val : args){
            if(val->type != Value::Num)
                throw new JuaTypeError("Math.max() called on non-number value");
            double v = static_cast<Number*>(val)->value;
            if(v > result) result = v;
        }
        return new Number(args[0]->vm, result);
    }));
    // 最小值：接受任意数量数字参数
    math->setProp("min", makeFunc([](jualist& args){
        if(args.size() < 1)throw new JuaTypeError("Math.min() requires at least 1 argument");
        double result = std::numeric_limits<double>::infinity();
        for(auto val : args){
            if(val->type != Value::Num)
                throw new JuaTypeError("Math.min() called on non-number value");
            double v = static_cast<Number*>(val)->value;
            if(v < result) result = v;
        }
        return new Number(args[0]->vm, result);
    }));

    // 伪随机数：无参返回 [0, 1) 的 double
    static std::mt19937_64 rng{std::random_device{}()};
    math->setProp("random", makeFunc([this](jualist&){
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        return new Number(this, dist(rng));
    }));

    return math;
}
