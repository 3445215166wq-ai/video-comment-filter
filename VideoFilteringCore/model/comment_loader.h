// comment_loader.h
#pragma once
#include <vector>
#include <string>

// 评论结构体（仅存储原始数据，向量后续赋值）
struct Comment {
    std::wstring username;
    std::wstring content;
    std::string time;
    std::string location;
    int likes = 0;
    std::string reply_to;

    std::vector<float> embedding;  // 后续推理模块填充
    bool encode_success = false;
};

// 从JSON文件加载评论（逐行解析）
std::vector<Comment> load_comments_from_json(const std::string& file_path);