// onnx_infer.h
#pragma once
#include <vector>
#include <string>
#include <mutex>
#include <onnxruntime_c_api.h>
#include "text_preproc.h"

class OnnxInferencer {
private:
    TextPreprocessor preproc_;
    const OrtApi* ort_ = nullptr;
    OrtEnv* env_ = nullptr;
    OrtSession* session_ = nullptr;
    std::mutex mutex_;
    const int max_len_ = 128;

    // 单条推理
    bool run_infer(const std::vector<int64_t>& ids, const std::vector<int64_t>& mask, std::vector<float>& emb);
    // 批量推理（CPU优化：一次处理多条）
    bool run_infer_batch(const std::vector<std::vector<int64_t>>& batch_ids,
        const std::vector<std::vector<int64_t>>& batch_mask,
        std::vector<std::vector<float>>& batch_emb);

public:
    bool init(const wchar_t* onnx_path);
    bool encode(const std::wstring& text, std::vector<float>& emb);
    bool encode_batch(const std::vector<std::wstring>& texts, std::vector<std::vector<float>>& embs);
    void uninit();
};

// 全局接口不变
bool load_onnx_model(const wchar_t* onnx_path);
bool encode_text(const std::wstring& text, std::vector<float>& emb);
void unload_onnx_model();
bool encode_text_batch(const std::vector<std::wstring>& texts, std::vector<std::vector<float>>& embs);