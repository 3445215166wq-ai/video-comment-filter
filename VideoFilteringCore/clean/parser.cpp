#include "parser.h"
#include <vector>
#include <string>
#include <windows.h>
#include <algorithm>

// 内部工具：宽字符串转UTF8（适配JSON输出）
static std::string wstring_to_utf8(const std::wstring& ws) {
    if (ws.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string res(len, 0);
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, &res[0], len, nullptr, nullptr);
    res.pop_back(); // 去掉末尾的\0
    return res;
}

// 内部工具：去首尾空格
static std::wstring trim(const std::wstring& s) {
    size_t l = s.find_first_not_of(L" \t\n\r");
    size_t r = s.find_last_not_of(L" \t\n\r");
    return (l > r) ? L"" : s.substr(l, r - l + 1);
}

// 内部工具：判断是否为纯数字（识别点赞数）
static bool is_pure_number(const std::wstring& s) {
    if (s.empty()) return false;
    for (wchar_t c : s) {
        if (!iswdigit(c)) return false;
    }
    return true;
}

// 单个评论转JSON
nlohmann::json DouyinComment::to_json() const {
    nlohmann::json j;
    j["username"] = username;
    j["content"] = content;
    j["time"] = time;
    j["location"] = location;
    j["likes"] = likes;
    j["reply_to"] = reply_to;
    return j;
}

// 核心解析逻辑（严格匹配你的2.txt→3.json规则）
std::string parse_cleaned_text_to_json(const std::wstring& cleaned_text) {
    if (cleaned_text.empty()) {
        fprintf(stderr, "[DEBUG parser] Input cleaned_text is empty!\n");
        return "[]";
    }

    std::vector<std::wstring> lines;
    std::wstring buf;

    // 第一步：分行
    for (wchar_t c : cleaned_text) {
        if (c == L'\n' || c == L'\r') {
            if (!buf.empty()) lines.push_back(trim(buf));
            buf.clear();
        }
        else {
            buf += c;
        }
    }
    if (!buf.empty()) lines.push_back(trim(buf));

    // 第二步：按规则解析每条评论
    std::vector<DouyinComment> comments;
    DouyinComment current_comment;
    bool in_comment_content = false;

    for (size_t i = 0; i < lines.size(); ++i) {
        const std::wstring& line = lines[i];
        if (line.empty()) continue;

        // 规则1：识别用户名（XXX头像 或 纯用户名行）
        size_t avatar_pos = line.find(L"头像");
        if (avatar_pos != std::wstring::npos) {
            // 保存上一条评论
            if (!current_comment.username.empty()) {
                comments.push_back(current_comment);
                current_comment = DouyinComment(); // 重置
            }
            // 提取用户名（去掉"头像"）
            current_comment.username = wstring_to_utf8(trim(line.substr(0, avatar_pos)));
            in_comment_content = true;
            continue;
        }

        // 规则2：纯用户名行（不包含"前"、"·"、纯数字）
        if (!in_comment_content && line.find(L"前") == std::wstring::npos
            && line.find(L"·") == std::wstring::npos && !is_pure_number(line)) {
            // 保存上一条评论
            if (!current_comment.username.empty()) {
                comments.push_back(current_comment);
                current_comment = DouyinComment();
            }
            current_comment.username = wstring_to_utf8(line);
            in_comment_content = true;
            continue;
        }

        // 规则3：识别时间+地点（如"1月前·广西"）
        if (line.find(L"前") != std::wstring::npos) {
            size_t dot_pos = line.find(L"·");
            if (dot_pos != std::wstring::npos) {
                current_comment.time = wstring_to_utf8(trim(line.substr(0, dot_pos)));
                current_comment.location = wstring_to_utf8(trim(line.substr(dot_pos + 1)));
            }
            else {
                current_comment.time = wstring_to_utf8(line);
                current_comment.location = "";
            }
            in_comment_content = false;
            continue;
        }

        // 规则4：识别点赞数（纯数字）
        if (is_pure_number(line)) {
            current_comment.likes = std::stoi(wstring_to_utf8(line));
            continue;
        }

        // 规则5：其余为评论内容
        if (in_comment_content) {
            if (!current_comment.content.empty()) {
                current_comment.content += "\n";
            }
            current_comment.content += wstring_to_utf8(line);
        }
    }

    // 保存最后一条评论
    if (!current_comment.username.empty()) {
        comments.push_back(current_comment);
    }

    // 第三步：生成JSON数组（和你的3.json格式完全一致）
    nlohmann::json json_arr = nlohmann::json::array();
    for (const auto& c : comments) {
        json_arr.push_back(c.to_json());
    }

    fprintf(stderr, "[DEBUG parser] Parsed %zu comments into JSON\n", comments.size());

    return json_arr.dump(4); // 4空格缩进，和你的3.json对齐
}