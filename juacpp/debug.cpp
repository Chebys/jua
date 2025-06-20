#include "jua-value.h"
#include <iostream>
#include <format>

void d_log(const string& msg){
    #ifdef JUA_DEBUG
    std::cout << msg << '\n';
    #endif
}
void d_log(char c){
    d_log(string(1, c));
}
void d_log(Jua_Val* val){
    auto ad = std::format("address: {:p}", (void*)val);
    d_log(ad);
    d_log(val->safeToString());
}
void d_log(string msg, Jua_Val* val){
    msg.append(val->safeToString());
    d_log(msg);
}
void d_log(string label, int num){
    label.append(": ");
    label.append(std::to_string(num));
    d_log(label);
}