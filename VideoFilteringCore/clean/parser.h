#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

// 评论结构体（和你的3.json字段完全一致）
struct DouyinComment {
    std::string username;
    std::string content;
    std::string time;
    std::string location;
    int likes = 0;
    std::nullptr_t reply_to = nullptr;

    // 转单个JSON对象
    nlohmann::json to_json() const;
};

// 核心功能：输入清洗后的文本，输出JSON数组字符串（和你的3.json完全一致）
std::string parse_cleaned_text_to_json(const std::wstring& cleaned_text);