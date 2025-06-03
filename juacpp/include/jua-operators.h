#pragma once
#include "jua-value.h"
#include <unordered_map>
enum class UniOper{
    unm,
    not_
};
enum class BinOper{
    add,
    sub,
    mul,
    div
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
};

inline Jua_Val* operate(UniOper type, Jua_Val* val){
    switch (type){
        case UniOper::unm: return val->unm();
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
        default: throw "todo: BinOper";
    }
}