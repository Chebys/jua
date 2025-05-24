#include "jua-value.h"
#include <iostream>

void d_log(const string& msg){
    std::cout << msg << '\n';
}
void d_log(char c){
    std::cout << c << '\n';
}
void d_log(Jua_Val* val){
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