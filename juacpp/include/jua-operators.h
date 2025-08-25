#pragma once
#include "jua-value.h"
#include <unordered_map>
enum class UniOper{
    unm,
    not_
};
enum class BinOper{
    pow,
    add,
    sub,
    mul,
    div,
    mod,
    range,
    lt,
    le,
    gt,
    ge,
    eq,
    ne,
    in,
    is,
    and_,
    or_,
};
inline std::unordered_map<string, UniOper> uniOpers{
    {"-", UniOper::unm},
    {"!", UniOper::not_}
};
inline std::unordered_map<string, BinOper> binOpers{
    {"^", BinOper::pow},
    {"*", BinOper::mul},
    {"/", BinOper::div},
    {"%", BinOper::mod},
    {"+", BinOper::add},
    {"-", BinOper::sub},
    {"..", BinOper::range},
    {"<", BinOper::lt},
    {"<=", BinOper::le},
    {">", BinOper::gt},
    {">=", BinOper::ge},
    {"==", BinOper::eq},
    {"!=", BinOper::ne},
    {"in", BinOper::in},
    {"is", BinOper::is},
    {"&&", BinOper::and_},
    {"||", BinOper::or_}
};
inline std::unordered_map<BinOper, int> binOperPriority{
    {BinOper::pow, 12},
    {BinOper::mul, 10},
    {BinOper::div, 10},
    {BinOper::mod, 9},
    {BinOper::add, 8},
    {BinOper::sub, 8},
    {BinOper::range, 7},
    {BinOper::lt, 6},
    {BinOper::le, 6},
    {BinOper::gt, 6},
    {BinOper::ge, 6},
    {BinOper::in, 6},
    {BinOper::is, 6},
    {BinOper::eq, 5},
    {BinOper::ne, 5},
    {BinOper::and_, 4},
    {BinOper::or_, 3}
};

inline Jua_Val* operate(UniOper type, Jua_Val* val){
    switch (type){
        case UniOper::unm: return val->unm();
        case UniOper::not_: return Jua_Bool::getInst(!val->toBoolean());
        default: throw "todo: UniOper";
    }
}
inline Jua_Val* operate(BinOper type, Jua_Val* left, Jua_Val* right){
    //短路不在此处理
    switch(type){
        case BinOper::add: return left->add(right);
        case BinOper::sub: return left->sub(right);
        case BinOper::mul: return left->mul(right);
        case BinOper::div: return left->div(right);
        case BinOper::range: return left->range(right);
        case BinOper::lt: return left->lt(right);
        case BinOper::le: return left->le(right);
        case BinOper::gt: return Jua_Bool::getInst(!left->le(right)->toBoolean());
        case BinOper::ge: return Jua_Bool::getInst(!left->lt(right)->toBoolean());
        case BinOper::eq: return left->eq(right);
        case BinOper::ne: return Jua_Bool::getInst(!(*left == right));
    }
    d_log(int(type));
    throw "todo: BinOper";
}
