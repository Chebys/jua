#include "jua-value.h"
#include "jua-vm.h"
bool is_func(Value* val){
    return val->type == Value::Func;
}
struct InvalidJSONException: JuaError{
    InvalidJSONException(const string& msg): JuaError("Invalid JSON: " + msg) {}
};
string encode(Value* val){
    switch(val->type){
        case Value::Null:
            return "null";
        case Value::Bool:
            return val->toBoolean() ? "true" : "false";
        case Value::Num:
            return val->toString();
        case Value::Str: {
            string str = val->toString();
            string res = "\"";
            for(char c: str){
                switch(c){
                    case '\"': res += "\\\""; break;
                    case '\\': res += "\\\\"; break;
                    case '\b': res += "\\b"; break;
                    case '\f': res += "\\f"; break;
                    case '\n': res += "\\n"; break;
                    case '\r': res += "\\r"; break;
                    case '\t': res += "\\t"; break;
                    default: //todo: utf8
                        if(static_cast<uint8_t>(c) < 0x20){
                            char buf[7];
                            snprintf(buf, sizeof(buf), "\\u%04x", static_cast<uint8_t>(c));
                            res += buf;
                        }else{
                            res += c;
                        }
                }
            }
            res += "\"";
            return res;
        }
        case Value::Obj: {
            if(val->isType(Array::type_id)){
                auto arr = static_cast<Array*>(val);
                string res = "[";
                for(size_t i=0; i<arr->items.size(); i++){
                    if(is_func(arr->items[i]))continue;
                    if(i > 0) res += ",";
                    res += encode(arr->items[i]);
                }
                res += "]";
                return res;
            }else{
                auto obj = static_cast<Object*>(val);
                string res = "{";
                bool first = true;
                for(auto& [key, value]: obj->dict){
                    if(is_func(value))continue;
                    if(!first) res += ",";
                    first = false;
                    res += encode(new String(val->vm, key));
                    res += ":";
                    res += encode(value);
                }
                res += "}";
                return res;
            }
        }
        default:
            throw new JuaTypeError("cannot encode type: " + val->getTypeName());
    }
}

Value* decode(VM* vm, const string& str, size_t& pos);
String* decode_string(VM* vm, const string& str, size_t& pos){
    //从'"'开始解析，返回时 pos 指向闭合引号之后
    pos++;
    size_t start = pos;
    string result;
    while(pos < str.size()){
        char c = str[pos];
        if(c == '\"'){
            pos++;
            return new String(vm, result);
        }
        if(c != '\\'){
            //普通字符：这是原先缺失的分支，没有它就会原地死循环
            result += c;
            pos++;
            continue;
        }
        pos++; //跳过反斜杠
        if(pos >= str.size()){
            throw new InvalidJSONException("Unterminated string starting at position " + std::to_string(start-1));
        }
        char esc = str[pos++];
        switch(esc){
            case '\"': result += '\"'; break;
            case '\\': result += '\\'; break;
            case '/': result += '/'; break;
            case 'b': result += '\b'; break;
            case 'f': result += '\f'; break;
            case 'n': result += '\n'; break;
            case 'r': result += '\r'; break;
            case 't': result += '\t'; break;
            case 'u': {
                if(pos + 4 > str.size()){
                    throw new InvalidJSONException("Invalid unicode escape at position " + std::to_string(pos));
                }
                string hex = str.substr(pos, 4);
                char* endptr;
                long codepoint = strtol(hex.c_str(), &endptr, 16);
                if(*endptr != '\0' || codepoint < 0 || codepoint > 0xFFFF){
                    throw new InvalidJSONException("Invalid unicode escape at position " + std::to_string(pos));
                }
                pos += 4;
                //简单处理BMP字符
                if(codepoint <= 0x7F){
                    result += static_cast<char>(codepoint);
                }else if(codepoint <= 0x7FF){
                    result += static_cast<char>(0xC0 | ((codepoint >> 6) & 0x1F));
                    result += static_cast<char>(0x80 | (codepoint & 0x3F));
                }else{
                    result += static_cast<char>(0xE0 | ((codepoint >> 12) & 0x0F));
                    result += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                    result += static_cast<char>(0x80 | (codepoint & 0x3F));
                }
                break;
            }
            default:
                throw new InvalidJSONException("Unknown escape at position " + std::to_string(pos-1));
        }
    }
    throw new InvalidJSONException("Unterminated string starting at position " + std::to_string(start-1));
}
Number* decode_number(VM* vm, const string& str, size_t& pos){
    size_t start = pos;
    bool has_decimal = false;
    bool has_exponent = false;
    if(str[pos] == '-') pos++;
    while(pos < str.size()){
        char c = str[pos];
        if(c >= '0' && c <= '9'){
            pos++;
        }else if(c == '.' && !has_decimal){
            has_decimal = true;
            pos++;
        }else if((c == 'e' || c == 'E') && !has_exponent){
            has_exponent = true;
            pos++;
            if(pos < str.size() && (str[pos] == '+' || str[pos] == '-')) pos++;
        }else{
            break;
        }
    }
    if(start == pos){
        throw new InvalidJSONException("Invalid number at position " + std::to_string(start));
    }
    string num_str = str.substr(start, pos - start);
    try {
        double value = std::stod(num_str);
        return new Number(vm, value);
    } catch (...) {
        throw new InvalidJSONException("Invalid number at position " + std::to_string(start));
    }
}
Array* decode_array(VM* vm, const string& str, size_t& pos){
    //从'['开始解析
    //未完全遵循 JSON 标准（允许尾逗号，允许省略逗号）
    pos++;
    auto arr = new Array(vm, {});
    while(pos < str.size()){
        if(isspace(str[pos])){
            pos++;
            continue;
        }
        if(str[pos] == ']'){
            pos++;
            return arr;
        }
        auto item = decode(vm, str, pos);
        arr->items.push_back(item);
        while(pos < str.size() && isspace(str[pos])) pos++;
        if(pos < str.size() && str[pos] == ','){
            pos++;
            continue;
        }
    }
    throw new InvalidJSONException("Missing closing ']'");
}
Object* decode_object(VM* vm, const string& str, size_t& pos){
    //从'{'开始解析
    //未完全遵循 JSON 标准（允许尾逗号，允许省略逗号）
    pos++;
    auto obj = new Object(vm);
    while(pos < str.size()){
        if(isspace(str[pos])){
            pos++;
            continue;
        }
        if(str[pos] == '}'){
            pos++;
            return obj;
        }
        if(str[pos] != '\"'){
            throw new InvalidJSONException("Expected '\"' at position " + std::to_string(pos));
        }
        //解析键
        auto key = decode_string(vm, str, pos);
        while(pos < str.size() && isspace(str[pos])) pos++;
        if(pos >= str.size() || str[pos] != ':'){
            throw new InvalidJSONException("Expected ':' after key at position " + std::to_string(pos));
        }
        pos++;
        while(pos < str.size() && isspace(str[pos])) pos++;
        if(pos >= str.size()){
            throw new InvalidJSONException("Unexpected end of input after ':' at position " + std::to_string(pos));
        }
        auto value = decode(vm, str, pos);
        obj->setProp(key->toString(), value);
        while(pos < str.size() && isspace(str[pos])) pos++;
        if(pos < str.size() && str[pos] == ','){
            pos++;
            continue;
        }
    }
    throw new InvalidJSONException("Missing closing '}'");
}
Value* decode(VM* vm, const string& str){
    size_t pos = 0;
    auto val = decode(vm, str, pos);
    //这里可以检查 pos 是否到达末尾
    return val;
}
Value* decode(VM* vm, const string& str, size_t& pos){
    switch(str[pos]){
        case 'n':
            if(str.compare(pos, 4, "null") == 0){
                pos += 4;
                return Null::getInst();
            }
            break;
        case 't':
            if(str.compare(pos, 4, "true") == 0){
                pos += 4;
                return Boolean::getInst(true);
            }
            break;
        case 'f':
            if(str.compare(pos, 5, "false") == 0){
                pos += 5;
                return Boolean::getInst(false);
            }
            break;
        case '\"':
            return decode_string(vm, str, pos);
        case '[':
            return decode_array(vm, str, pos);
        case '{':
            return decode_object(vm, str, pos);
        default:
            if((str[pos] >= '0' && str[pos] <= '9') || str[pos] == '-'){
                return decode_number(vm, str, pos);
            }
    }
    throw new InvalidJSONException("Unexpected character at position " + std::to_string(pos));
}

Object* VM::makeJSON(){
    auto proto = new Object(this, ObjectProto);
    proto->setProp("encode", makeFunc([this](jualist& args){
        if(args.size() < 1)
            throw new JuaError("JSON.stringify() requires at least 1 argument");
        return new String(this, encode(args[0]));
    }));
    proto->setProp("decode", makeFunc([this](jualist& args){
        if(args.size() < 1)
            throw new JuaError("JSON.parse() requires at least 1 argument");
        if(args[0]->type != Value::Str)
            throw new JuaTypeError("JSON.parse() requires a string argument");
        return decode(this, args[0]->toString());
    }));
    return proto;
}