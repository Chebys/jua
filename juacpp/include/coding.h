#pragma once
#include<string>
#include <cstdint>

template<typename T>
void encode(double num, std::string& str){
    T value = num;
    str.resize(sizeof(T));
    memcpy(&str[0], &value, sizeof(T));
}

template<typename T>
double decode(const std::string& str){
    if(str.size() < sizeof(T)) throw sizeof(T);
    T value;
    memcpy(&value, str.data(), sizeof(T));
    return value;
}

bool isLittleEndian(){
    uint16_t num = 0x0102;
    return *(uint8_t*)&num == 0x02; // 如果低位在前，则为小端
}