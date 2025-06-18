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
    {"+", BinOper::add},
    {"-", BinOper::sub},
    {"*", BinOper::mul},
    {"/", BinOper::div},
    {"%", BinOper::mod},
    {"^", BinOper::pow},
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

inline Jua_Val* operate(UniOper type, Jua_Val* val){
    switch (type){
        case UniOper::unm: return val->unm();
        case UniOper::not_: return Jua_Bool::getInst(!val->toBoolean());
        default: throw "todo: UniOper";
    }
}
//todo: 短路
inline Jua_Val* operate(BinOper type, Jua_Val* left, Jua_Val* right){
    switch(type){
        case BinOper::add: return left->add(right);
        case BinOper::sub: return left->sub(right);
        case BinOper::mul: return left->mul(right);
        case BinOper::div: return left->div(right);
        case BinOper::lt: return left->lt(right);
        case BinOper::le: return left->le(right);
        case BinOper::gt: return Jua_Bool::getInst(!left->le(right));
        case BinOper::ge: return Jua_Bool::getInst(!left->lt(right));
        case BinOper::eq: return left->eq(right);
        case BinOper::ne: return Jua_Bool::getInst(!(*left == right));
        case BinOper::and_: return left->toBoolean() ? right : left;
        case BinOper::or_: return left->toBoolean() ? left : right;
        default: throw "todo: BinOper";
    }
}