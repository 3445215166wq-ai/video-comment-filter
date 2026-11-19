// text_preproc.h
#pragma once
#include <vector>
#include <string>
#include "../include/sentencepiece_processor.h"

class TextPreprocessor {
private:
    sentencepiece::SentencePieceProcessor processor_;
    bool sp_loaded_ = false;
    const int max_len_ = 128;
    const int skip_tokens_ = 0;
    // 分词模型路径：运行时解析为 <exe 所在目录>\models\sentencepiece.bpe.model
    std::wstring sp_path_;

    // wstring转UTF8
    std::string wstring_to_utf8(const std::wstring& wstr);
    // 解析分词模型路径（相对 exe 目录，避免硬编码绝对路径）
    std::wstring resolve_sp_model_path();
    // 加载分词模型
    bool load_sp_model();

public:
    // 初始化（加载分词模型）
    bool init();
    // 分词核心接口：输入文本，输出Token ID和Attention Mask
    void tokenize(const std::wstring& text, std::vector<int64_t>& ids, std::vector<int64_t>& mask);
    // 释放资源
    void uninit() { sp_loaded_ = false; }
};