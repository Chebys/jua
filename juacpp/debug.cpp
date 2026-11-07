#include "jua-value.h"
#include <iostream>
#include <format>

namespace Jua {

void d_log(const string& msg){
    #ifdef JUA_DEBUG
    std::cout << msg << '\n';
    #else
    (void)msg; //未定义 JUA_DEBUG 时参数不被使用
    #endif
}
void d_log(char c){
    d_log(string(1, c));
}
void d_log(Value* val){
    auto ad = std::format("address: {:p}", (void*)val);
    d_log(ad);
    d_log(val->safeToString());
}
void d_log(string msg, Value* val){
    msg.append(val->safeToString());
    d_log(msg);
}
void d_log(string label, int num){
    label.append(": ");
    label.append(std::to_string(num));
    d_log(label);
}

}
