// onnx_infer.cpp
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include "onnx_infer.h"
#include <iostream>
#include <algorithm>    // std::min, std::copy_n

static OnnxInferencer g_inferencer;

// ------------------- 单条推理（保持不变） -------------------
bool OnnxInferencer::run_infer(const std::vector<int64_t>& ids, const std::vector<int64_t>& mask, std::vector<float>& emb) {
    emb.clear();
    OrtMemoryInfo* mi = nullptr;
    OrtValue* in_ids = nullptr, * in_tt = nullptr, * in_mask = nullptr, * out = nullptr;
    const char* inputs[] = { "input_ids","token_type_ids","attention_mask" };
    const char* outputs[] = { "last_hidden_state" };

    ort_->CreateCpuMemoryInfo(OrtDeviceAllocator, OrtMemTypeDefault, &mi);
    std::vector<int64_t> token_type(max_len_, 0);
    int64_t shape[] = { 1, max_len_ };

    ort_->CreateTensorWithDataAsOrtValue(mi, (void*)const_cast<int64_t*>(ids.data()), max_len_ * sizeof(int64_t), shape, 2, ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64, &in_ids);
    ort_->CreateTensorWithDataAsOrtValue(mi, (void*)const_cast<int64_t*>(token_type.data()), max_len_ * sizeof(int64_t), shape, 2, ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64, &in_tt);
    ort_->CreateTensorWithDataAsOrtValue(mi, (void*)const_cast<int64_t*>(mask.data()), max_len_ * sizeof(int64_t), shape, 2, ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64, &in_mask);

    OrtValue* tensors[] = { in_ids,in_tt,in_mask };
    ort_->Run(session_, nullptr, inputs, tensors, 3, outputs, 1, &out);

    OrtTensorTypeAndShapeInfo* info = nullptr;
    ort_->GetTensorTypeAndShape(out, &info);
    size_t cnt = 0;
    ort_->GetDimensionsCount(info, &cnt);
    std::vector<int64_t> dims(cnt);
    ort_->GetDimensions(info, dims.data(), (int)cnt);
    int64_t hidden = dims.back();
    ort_->ReleaseTensorTypeAndShapeInfo(info);

    float* data = nullptr;
    ort_->GetTensorMutableData(out, (void**)&data);
    emb.assign(data, data + hidden);

    ort_->ReleaseValue(in_ids);
    ort_->ReleaseValue(in_tt);
    ort_->ReleaseValue(in_mask);
    ort_->ReleaseValue(out);
    ort_->ReleaseMemoryInfo(mi);
    return !emb.empty();
}

// ------------------- 初始化（增加 CPU 线程数 + 图优化） -------------------
bool OnnxInferencer::init(const wchar_t* onnx_path) {
    std::lock_guard<std::mutex> lk(mutex_);
    if (!preproc_.init()) return false;

    ort_ = OrtGetApiBase()->GetApi(ORT_API_VERSION);
    ort_->CreateEnv(ORT_LOGGING_LEVEL_WARNING, "AI", &env_);

    OrtSessionOptions* opt = nullptr;
    ort_->CreateSessionOptions(&opt);

    // ===== CPU 优化（适用于 ONNX Runtime ≥ 1.13）=====
    ort_->SetSessionGraphOptimizationLevel(opt, ORT_ENABLE_ALL);
    // 尝试使用 OrtApi 的成员函数设置线程数（在某些 onnxruntime 发行版中可用）
    // 如果编译器报错该成员不存在，请告诉我完整错误信息，我会给出备选方案。
    ort_->SetIntraOpNumThreads(opt, 4);
    ort_->SetInterOpNumThreads(opt, 2);
    // ==================================================

    auto status = ort_->CreateSession(env_, onnx_path, opt, &session_);
    ort_->ReleaseSessionOptions(opt);

    if (status != nullptr) {
        const char* err = ort_->GetErrorMessage(status);
        std::cerr << "[ERROR] 模型加载失败！原因：" << err << std::endl;
        ort_->ReleaseStatus(status);
        return false;
    }
    std::cout << "[INFO] ONNX模型加载成功" << std::endl;
    return true;
}

// ------------------- 单文本编码（保持不变） -------------------
bool OnnxInferencer::encode(const std::wstring& text, std::vector<float>& emb) {
    std::lock_guard<std::mutex> lk(mutex_);
    if (!session_) return false;
    std::vector<int64_t> ids, mask;
    preproc_.tokenize(text, ids, mask);
    return run_infer(ids, mask, emb);
}

// ------------------- 批量推理实现（真正的 batch） -------------------
bool OnnxInferencer::run_infer_batch(const std::vector<std::vector<int64_t>>& batch_ids,
    const std::vector<std::vector<int64_t>>& batch_mask,
    std::vector<std::vector<float>>& batch_emb) {
    batch_emb.clear();
    size_t batch_size = batch_ids.size();
    if (batch_size == 0) return true;

    // 将所有样本填充到 max_len_ 并拼成连续内存
    std::vector<int64_t> flat_ids(batch_size * max_len_, 0);
    std::vector<int64_t> flat_tt(batch_size * max_len_, 0);     // token_type 全0
    std::vector<int64_t> flat_mask(batch_size * max_len_, 0);

    for (size_t i = 0; i < batch_size; ++i) {
        size_t copy_len = std::min(batch_ids[i].size(), (size_t)max_len_);
        std::copy_n(batch_ids[i].begin(), copy_len, flat_ids.begin() + i * max_len_);
        copy_len = std::min(batch_mask[i].size(), (size_t)max_len_);
        std::copy_n(batch_mask[i].begin(), copy_len, flat_mask.begin() + i * max_len_);
    }

    // 创建内存信息和形状
    OrtMemoryInfo* mi = nullptr;
    ort_->CreateCpuMemoryInfo(OrtDeviceAllocator, OrtMemTypeDefault, &mi);

    int64_t input_shape[] = { (int64_t)batch_size, (int64_t)max_len_ };
    OrtValue* in_ids = nullptr, * in_tt = nullptr, * in_mask = nullptr, * out = nullptr;

    ort_->CreateTensorWithDataAsOrtValue(mi, flat_ids.data(), flat_ids.size() * sizeof(int64_t),
        input_shape, 2, ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64, &in_ids);
    ort_->CreateTensorWithDataAsOrtValue(mi, flat_tt.data(), flat_tt.size() * sizeof(int64_t),
        input_shape, 2, ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64, &in_tt);
    ort_->CreateTensorWithDataAsOrtValue(mi, flat_mask.data(), flat_mask.size() * sizeof(int64_t),
        input_shape, 2, ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64, &in_mask);

    // 执行推理
    const char* inputs[] = { "input_ids","token_type_ids","attention_mask" };
    const char* outputs[] = { "last_hidden_state" };
    OrtValue* tensors[] = { in_ids, in_tt, in_mask };
    ort_->Run(session_, nullptr, inputs, tensors, 3, outputs, 1, &out);

    // 解析输出 [batch_size, max_len_, hidden]
    OrtTensorTypeAndShapeInfo* info = nullptr;
    ort_->GetTensorTypeAndShape(out, &info);
    size_t dim_cnt = 0;
    ort_->GetDimensionsCount(info, &dim_cnt);
    std::vector<int64_t> dims(dim_cnt);
    ort_->GetDimensions(info, dims.data(), dim_cnt);
    int64_t hidden = dims.back();
    ort_->ReleaseTensorTypeAndShapeInfo(info);

    float* data = nullptr;
    ort_->GetTensorMutableData(out, (void**)&data);

    // 提取每个样本的 CLS 向量（第0个位置）
    batch_emb.resize(batch_size);
    for (size_t i = 0; i < batch_size; ++i) {
        size_t offset = i * max_len_ * hidden;
        batch_emb[i].assign(data + offset, data + offset + hidden);
    }

    // 释放资源
    ort_->ReleaseValue(in_ids);
    ort_->ReleaseValue(in_tt);
    ort_->ReleaseValue(in_mask);
    ort_->ReleaseValue(out);
    ort_->ReleaseMemoryInfo(mi);

    return true;
}

// ------------------- 批量编码（改用真正的 batch） -------------------
bool OnnxInferencer::encode_batch(const std::vector<std::wstring>& texts, std::vector<std::vector<float>>& embs) {
    std::lock_guard<std::mutex> lk(mutex_);
    if (!session_) return false;

    embs.clear();
    embs.resize(texts.size());

    // 收集所有非空文本的 ids / mask
    std::vector<std::vector<int64_t>> all_ids;
    std::vector<std::vector<int64_t>> all_mask;
    std::vector<size_t> valid_indices;   // 记录在原始 texts 中的位置

    for (size_t i = 0; i < texts.size(); ++i) {
        if (texts[i].empty()) continue;   // 空文本结果保持空向量
        std::vector<int64_t> ids, mask;
        preproc_.tokenize(texts[i], ids, mask);
        all_ids.push_back(std::move(ids));
        all_mask.push_back(std::move(mask));
        valid_indices.push_back(i);
    }

    if (all_ids.empty()) return true;   // 全是空文本

    // 一次性批量推理
    std::vector<std::vector<float>> batch_embs;
    if (!run_infer_batch(all_ids, all_mask, batch_embs))
        return false;

    // 将结果按原顺序放回 embs
    for (size_t j = 0; j < valid_indices.size(); ++j) {
        embs[valid_indices[j]] = std::move(batch_embs[j]);
    }

    return true;
}

// ------------------- 资源释放（保持不变） -------------------
void OnnxInferencer::uninit() {
    std::lock_guard<std::mutex> lk(mutex_);
    preproc_.uninit();
    if (ort_) {
        if (session_) ort_->ReleaseSession(session_);
        if (env_) ort_->ReleaseEnv(env_);
        ort_ = nullptr;
    }
}

// ------------------- 全局接口实现（完全不变） -------------------
bool load_onnx_model(const wchar_t* onnx_path) {
    return g_inferencer.init(onnx_path);
}

bool encode_text(const std::wstring& text, std::vector<float>& emb) {
    return g_inferencer.encode(text, emb);
}

void unload_onnx_model() {
    g_inferencer.uninit();
}

bool encode_text_batch(const std::vector<std::wstring>& texts, std::vector<std::vector<float>>& embs) {
    return g_inferencer.encode_batch(texts, embs);
}