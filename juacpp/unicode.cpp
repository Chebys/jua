#include <iostream>
#include <string>
#include <vector>
#include <cstdint>

std::u16string utf8_to_utf16(const std::string& utf8) {
    std::u16string utf16;
    size_t i = 0;
    
    while (i < utf8.size()) {
        uint32_t codepoint = 0;
        uint8_t first = utf8[i++];
        
        if ((first & 0x80) == 0) { // 1字节序列 (0xxxxxxx)
            codepoint = first;
        }else if((first & 0xE0) == 0xC0 && i < utf8.size()){ // 2字节序列 (110xxxxx 10xxxxxx)
            uint8_t second = utf8[i++];
            codepoint = ((first & 0x1F) << 6) | (second & 0x3F);
        }else if((first & 0xF0) == 0xE0 && i+1 < utf8.size()){ // 3字节序列 (1110xxxx 10xxxxxx 10xxxxxx)
            uint8_t second = utf8[i++];
            uint8_t third = utf8[i++];
            codepoint = ((first & 0x0F) << 12) | ((second & 0x3F) << 6) | (third & 0x3F);
        }else if((first & 0xF8) == 0xF0 && i+2 < utf8.size()){ // 4字节序列 (11110xxx 10xxxxxx 10xxxxxx 10xxxxxx)
            uint8_t second = utf8[i++];
            uint8_t third = utf8[i++];
            uint8_t fourth = utf8[i++];
            codepoint = ((first & 0x07) << 18) | ((second & 0x3F) << 12) | 
                       ((third & 0x3F) << 6) | (fourth & 0x3F);
        }else{
            // 无效的UTF-8序列
            throw std::runtime_error("Invalid UTF-8 sequence");
        }
        
        // 将Unicode码点转换为UTF-16
        if (codepoint < 0x10000) {
            // 基本多文种平面 (BMP)
            utf16.push_back(static_cast<char16_t>(codepoint));
        } else {
            // 辅助平面 - 转换为代理对
            codepoint -= 0x10000;
            utf16.push_back(static_cast<char16_t>(0xD800 + (codepoint >> 10)));
            utf16.push_back(static_cast<char16_t>(0xDC00 + (codepoint & 0x3FF)));
        }
    }
    
    return utf16;
}

int main() {
    try {
        std::string utf8_str = u8"你好，世界！🌍";
        std::u16string utf16_str = utf8_to_utf16(utf8_str);
        
        std::cout << "UTF-8 length: " << utf8_str.length() << std::endl;
        std::cout << "UTF-16 length: " << utf16_str.length() << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    
    return 0;
}