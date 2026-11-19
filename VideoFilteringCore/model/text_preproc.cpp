// text_preproc.cpp
#define _CRT_SECURE_NO_WARNINGS
#include "text_preproc.h"
#include <Windows.h>
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>

// ============================================================================
// XLM-R 词表 ID 补偿（fairseq_offset）
//
// 本项目的分词表是 XLM-R 体系的 sentencepiece.bpe.model，而它的**原始 id 与模型
// 实际使用的 token id 相差 1 位**——这是 XLM-R 的既有设计，不是文件损坏：
//
//   id   fairseq 词表（= 模型 embedding 行号）   spm 原始 id（= SentencePiece 返回）
//    0   <s>                                   <unk>
//    1   <pad>                                 <s>
//    2   </s>                                  </s>
//    3   <unk>                                 ','
//    4   ','                                   '.'
//    ... 之后每个都差 1
//
// HuggingFace 在 transformers/models/xlm_roberta/tokenization_xlm_roberta.py 里
// 用 self.fairseq_offset = 1 显式补偿；<mask> 的位置为 len(sp_model) + 1 = 250001。
//
// 本项目直接使用 SentencePiece C++ 库，拿不到那层 Python 包装，因此**必须在这里
// 自己做同样的补偿**——偏移只能加一次，所以分词表保持上游原版、不改动文件。
// ============================================================================
namespace {

constexpr int64_t kModelIdBos = 0;   // <s>    （模型 id）
constexpr int64_t kModelIdPad = 1;   // <pad>  （模型 id；spm 词表中没有 <pad>）
constexpr int64_t kModelIdEos = 2;   // </s>   （模型 id）
constexpr int64_t kModelIdUnk = 3;   // <unk>  （模型 id）
constexpr int64_t kFairseqOffset = 1;

// spm 的原始 id → 模型期望的 id
int64_t SpmIdToModelId(int64_t spm_id) {
    switch (spm_id) {
    case 0:  return kModelIdUnk;                    // spm: <unk>
    case 1:  return kModelIdBos;                    // spm: <s>
    case 2:  return kModelIdEos;                    // spm: </s>
    default: return spm_id + kFairseqOffset;        // 普通词元统一 +1
    }
}

} // namespace

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

    // 拼装序列： [<s>] + 正文（已做 fairseq_offset 补偿） + [</s>] + [<pad>]...
    // 特殊词元的 id 直接使用模型 id 常量，不从 spm 查询：
    //   - spm 里 <s> 是 1、<unk> 是 0，与模型 id 不同
    //   - spm 里根本没有 <pad>（查询会静默回退成 <unk>）
    ids.push_back(kModelIdBos);
    for (int id : filtered) {
        if (ids.size() >= max_len_ - 1) break;
        ids.push_back(SpmIdToModelId(id));
    }
    if (ids.size() < max_len_) ids.push_back(kModelIdEos);
    while (ids.size() < max_len_) ids.push_back(kModelIdPad);

    // 生成 Attention Mask：按"是否为填充位"判定
    mask.assign(max_len_, 0);
    for (size_t i = 0; i < max_len_; ++i) mask[i] = (ids[i] != kModelIdPad);
}