#define _CRT_SECURE_NO_WARNINGS
#include "string_util.h"
#include <Windows.h>

std::string wstring_to_utf8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
    if (len <= 0) return "";
    std::string utf8_str(len - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &utf8_str[0], len, NULL, NULL);
    return utf8_str;
}