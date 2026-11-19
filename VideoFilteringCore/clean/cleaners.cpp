#include "clean/cleaners.h"
#include <regex>
#include <vector>
#include <string>

// 内部工具：去首尾空格
static std::wstring trim(const std::wstring& s) {
    size_t l = s.find_first_not_of(L" \t\n\r");
    size_t r = s.find_last_not_of(L" \t\n\r");
    return (l > r) ? L"" : s.substr(l, r - l + 1);
}

// 核心清洗逻辑（匹配手动清洗文本1.txt→2.txt的规则）
std::wstring clean_douyin_text(const std::wstring& raw_text) {
    std::wstring text = raw_text;

    // 步骤1：只保留「全部评论」到「留下你的精彩评论吧」之间的内容
    size_t start_pos = text.find(L"全部评论");
    size_t end_pos = text.find(L"留下你的精彩评论吧");
    if (start_pos != std::wstring::npos && end_pos != std::wstring::npos && start_pos < end_pos) {
        text = text.substr(start_pos, end_pos - start_pos);
    } else {
        // DEBUG: 标记找不到关键标记
        fprintf(stderr, "[DEBUG cleaners] Failed to find markers: start_pos=%zu, end_pos=%zu\n", start_pos, end_pos);
    }

    // 步骤2：过滤垃圾关键词行（和你的2.txt完全对齐）
    std::vector<std::wstring> garbage_keywords = {
        L"开启读屏", L"下载抖音精选", L"2025 © 抖音", L"京ICP备", L"京公网安备",
        L"许可证", L"举报", L"用户服务协议", L"隐私政策", L"分享", L"回复",
        L"展开", L"加载中", L"站点地图", L"下载抖音"
    };

    // 分行处理
    std::vector<std::wstring> lines;
    std::wstring buf;
    for (wchar_t c : text) {
        if (c == L'\n' || c == L'\r') {
            if (!buf.empty()) lines.push_back(trim(buf));
            buf.clear();
        }
        else {
            buf += c;
        }
    }
    if (!buf.empty()) lines.push_back(trim(buf));

    // 过滤垃圾行，保留有效内容
    std::wstring cleaned;
    for (const auto& line : lines) {
        if (line.empty()) continue;

        // 跳过包含垃圾关键词的行
        bool is_garbage = false;
        for (const auto& kw : garbage_keywords) {
            if (line.find(kw) != std::wstring::npos) {
                is_garbage = true;
                break;
            }
        }
        if (!is_garbage) {
            cleaned += line + L"\n";
        }
    }

    // DEBUG: Log final result
    fprintf(stderr, "[DEBUG cleaners] Input lines=%zu, Output size=%zu\n", lines.size(), cleaned.size());

    return cleaned;
}