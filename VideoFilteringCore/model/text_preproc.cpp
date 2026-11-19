// text_preproc.cpp
#define _CRT_SECURE_NO_WARNINGS
#include "text_preproc.h"
#include <Windows.h>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>

std::string TextPreprocessor::wstring_to_utf8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
    std::string utf8_str(len - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &utf8_str[0], len, NULL, NULL);
    return utf8_str;
}

// 解析分词模型路径：<exe 所在目录>\models\sentencepiece.bpe.model
// 与 model.onnx 的定位方式保持一致，避免硬编码绝对路径
std::wstring TextPreprocessor::resolve_sp_model_path() {
    wchar_t exe_path[MAX_PATH] = {};
    DWORD n = ::GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
    std::wstring dir;
    if (n > 0 && n < MAX_PATH) {
        dir.assign(exe_path, n);
        const size_t slash = dir.find_last_of(L"\\/");
        if (slash != std::wstring::npos) {
            dir.erase(slash);
        }
    }
    return dir + L"\\models\\sentencepiece.bpe.model";
}

bool TextPreprocessor::load_sp_model() {
    sp_path_ = resolve_sp_model_path();
    const std::string path_utf8 = wstring_to_utf8(sp_path_);

    // 说明：SentencePieceProcessor::Load() 接收的是窄字符路径，
    // 在 Windows 下由 CRT 按 ANSI 代码页解释，路径含中文等非 ASCII 字符时会报
    // "No such file or directory"。因此这里改用宽字符路径自行读取原始字节，
    // 再通过 LoadFromSerializedProto() 加载，从而支持任意 Unicode 路径。
    std::ifstream file(sp_path_.c_str(), std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[ERROR] 无法打开分词模型文件：" << path_utf8
                  << "\n[ERROR] 请将 sentencepiece.bpe.model 放到 exe 同级的 models 目录下"
                  << std::endl;
        return false;
    }

    std::string serialized((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
    file.close();

    if (serialized.empty()) {
        std::cerr << "[ERROR] 分词模型文件为空：" << path_utf8 << std::endl;
        return false;
    }

    auto status = processor_.LoadFromSerializedProto(serialized);
    if (!status.ok()) {
        std::cerr << "[ERROR] 加载SentencePiece失败：" << status.ToString()
                  << "\n[ERROR] 模型路径：" << path_utf8
                  << std::endl;
        return false;
    }
    sp_loaded_ = true;
    std::cout << "[INFO] SentencePiece加载成功" << std::endl;
    return true;
}

bool TextPreprocessor::init() {
    return load_sp_model();
}

void TextPreprocessor::tokenize(const std::wstring& text, std::vector<int64_t>& ids, std::vector<int64_t>& mask) {
    ids.clear(); mask.clear();
    std::string text_utf8 = wstring_to_utf8(text);

    // 分词生成原始Token ID
    std::vector<int> temp_ids;
    processor_.Encode(text_utf8, &temp_ids);

    // 过滤跳过的Token
    std::vector<int> filtered = (skip_tokens_ > 0 && temp_ids.size() > (size_t)skip_tokens_)
        ? std::vector<int>(temp_ids.begin() + skip_tokens_, temp_ids.end())
        : temp_ids;

    // 拼接特殊Token（CLS/SEP/PAD）
    int64_t cls = processor_.PieceToId("<s>");
    int64_t sep = processor_.PieceToId("</s>");
    int64_t pad = processor_.PieceToId("<pad>");

    ids.push_back(cls);
    for (int id : filtered) {
        if (ids.size() >= max_len_ - 1) break;
        ids.push_back((int64_t)id);
    }
    if (ids.size() < max_len_) ids.push_back(sep);
    while (ids.size() < max_len_) ids.push_back(pad);

    // 生成Attention Mask
    mask.assign(max_len_, 0);
    for (size_t i = 0; i < max_len_; ++i) mask[i] = (ids[i] != pad);
}