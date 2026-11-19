// comment_loader.cpp
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include "comment_loader.h"
#include "../nlohmann/json.hpp"
#include <fstream>
#include <iostream>

using json = nlohmann::json;

std::vector<Comment> load_comments_from_json(const std::string& file_path) {
    std::vector<Comment> comments;
    std::ifstream file(file_path);
    std::string line;

    if (!file.is_open()) {
        std::cerr << "[ERROR] 打开JSON文件失败" << std::endl;
        return comments;
    }

    while (std::getline(file, line)) {
        if (line.empty()) continue;
        try {
            json j = json::parse(line);
            Comment c;

            // 解析基础字段（string→wstring）
            std::string uname = j["username"];
            c.username = std::wstring(uname.begin(), uname.end());
            std::string content = j["content"];
            c.content = std::wstring(content.begin(), content.end());

            // 可选字段判空赋值
            c.time = j.count("time") && !j["time"].is_null() ? j["time"] : "";
            c.location = j.count("location") && !j["location"].is_null() ? j["location"] : "无";
            c.likes = j.value("likes", 0);
            c.reply_to = j.count("reply_to") && !j["reply_to"].is_null() ? j["reply_to"] : "无";

            comments.push_back(c);
        }
        catch (...) {
            std::cerr << "[WARN] 解析JSON行失败" << std::endl;
        }
    }
    file.close();
    std::cout << "[INFO] 加载评论数量：" << comments.size() << std::endl;
    return comments;
}