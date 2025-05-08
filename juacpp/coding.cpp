#include<string>

template<typename T>
void encode(double num, std::string& str){
    T value = num;
    str.resize(sizeof(T));
    memcpy(&str[0], &value, sizeof(T));
}

template<typename T>
double decode(void* data){ //不检查范围
    T value;
    memcpy(&value, data, sizeof(T));
    return value;
}