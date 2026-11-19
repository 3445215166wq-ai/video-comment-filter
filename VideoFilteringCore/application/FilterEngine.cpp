#include "FilterEngine.h"

#include <fstream>
#include <sstream>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <algorithm>

// 项目头文件
#include "business_operations.h"
#include "clean/cleaners.h"
#include "clean/parser.h"
#include "model/onnx_infer.h"
#include "core/similarity_policy.h"
#include "nlohmann/json.hpp"
#include "model/string_util.h"      // wstring_to_utf8
#include "utils/utils.h"            // Utils::DebugLogW（可选）
 // AI辅助生成：DeepSeek-R1, 2026-4-2

/// 匿名命名空间：封装FilterEngine内部工具函数，避免外部链接冲突
namespace {
    /**
     * @brief 生成毫秒级时间戳字符串（用于文件名唯一标识）
     * @return 时间戳宽字符串
     */
    std::wstring GetTimestampString() {
        return std::to_wstring(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count());
    }

    /**
     * @brief 计算一组向量的均值嵌入（向量逐元素平均）
     * @param embeddings 输入的嵌入向量集合
     * @param mean_emb 输出：计算后的均值嵌入向量
     * @return 计算成功返回true，输入为空返回false
     */
    bool calculate_mean_embedding(const std::vector<std::vector<float>>& embeddings, std::vector<float>& mean_emb) {
        if (embeddings.empty() || embeddings[0].empty()) return false;
        mean_emb.resize(embeddings[0].size(), 0.0f);

        // 逐元素累加所有向量
        for (const auto& emb : embeddings) {
            for (size_t i = 0; i < emb.size(); ++i) {
                mean_emb[i] += emb[i];
            }
        }

        // 除以数量得到均值
        for (auto& val : mean_emb) {
            val /= static_cast<float>(embeddings.size());
        }
        return true;
    }

    /**
     * @brief 从JSON数组中提取评论文本字段（content）
     * @param j 输入的JSON数组（每条包含content字段）
     * @return 评论宽字符串集合
     */
    std::vector<std::wstring> ExtractCommentTextsFromJson(const nlohmann::json& j) {
        std::vector<std::wstring> texts;
        if (!j.is_array()) return texts;

        for (const auto& item : j) {
            if (!item.is_object()) continue;
            if (item.contains("content") && item["content"].is_string()) {
                const std::string content = item["content"].get<std::string>();

                // Win32 API：UTF-8 转宽字符串，先获取长度再分配内存
                int len = MultiByteToWideChar(CP_UTF8, 0, content.c_str(), -1, nullptr, 0);
                std::wstring ws(static_cast<size_t>(len), L'\0');
                MultiByteToWideChar(CP_UTF8, 0, content.c_str(), -1, &ws[0], len);

                // 移除API自动生成的末尾终止空字符，保证字符串纯净
                if (!ws.empty() && ws.back() == L'\0') ws.pop_back();
                texts.emplace_back(ws);
            }
        }
        return texts;
    }

    /**
     * @brief 对一批文本编码并计算均值嵌入
     * @param texts 输入文本集合
     * @param mean_emb 输出均值嵌入向量
     * @return 成功返回true
     */
    bool BuildMeanEmbeddingFromTexts(const std::vector<std::wstring>& texts, std::vector<float>& mean_emb) {
        if (texts.empty()) return false;
        std::vector<std::vector<float>> embs;

        if (!encode_text_batch(texts, embs) || embs.empty()) return false;
        return calculate_mean_embedding(embs, mean_emb);
    }

    /**
     * @brief 从JSON文件读取评论并构建均值嵌入（供相似度策略回调使用）
     * @param file_path JSON模板文件路径
     * @param mean_emb 输出均值嵌入
     * @return 成功返回true
     */
    bool BuildMeanEmbeddingFromJsonFile(const std::wstring& file_path, std::vector<float>& mean_emb) {
        std::ifstream ifs(file_path);
        if (!ifs.is_open()) return false;

        std::stringstream ss;
        ss << ifs.rdbuf();
        const std::string content = ss.str();
        if (content.empty()) return false;

        try {
            const nlohmann::json j = nlohmann::json::parse(content);
            const auto texts = ExtractCommentTextsFromJson(j);
            return BuildMeanEmbeddingFromTexts(texts, mean_emb);
        }
        catch (...) {
            // JSON解析异常直接返回失败，屏蔽所有解析错误
            return false;
        }
    }

    /**
     * @brief 保存原始文本、清洗文本、解析JSON到文件目录
     * @param data_dir 根目录
     * @param raw_text 原始爬取文本
     * @param cleaned_text 清洗后文本
     * @param json_str 解析输出的JSON字符串
     * @param record_raw_and_cleaned 是否保存原始/清洗文本
     * @return 保存的JSON文件路径
     */
    std::wstring SaveData(const std::wstring& data_dir,
        const std::wstring& raw_text,
        const std::wstring& cleaned_text,
        const std::string& json_str,
        bool record_raw_and_cleaned) {
        std::filesystem::create_directories(data_dir + L"\\raw");
        std::filesystem::create_directories(data_dir + L"\\cleaned");
        std::filesystem::create_directories(data_dir + L"\\parser");

        const auto timestamp = GetTimestampString();

        // 始终保存解析结果JSON，为后续审计提供核心数据
        std::wstring parser_file_path = data_dir + L"\\parser\\parser_" + timestamp + L".json";
        std::ofstream parser_file(wstring_to_utf8(parser_file_path));
        if (parser_file.is_open()) {
            parser_file << json_str;
            parser_file.close();
        }

        if (record_raw_and_cleaned) {
            // 二进制模式保存原始文本，避免中文/特殊字符编码丢失
            std::ofstream raw_file(wstring_to_utf8(data_dir + L"\\raw\\raw_" + timestamp + L".txt"), std::ios::binary);
            if (raw_file.is_open()) {
                raw_file << wstring_to_utf8(raw_text);
                raw_file.close();
            }

            // 二进制模式保存清洗后文本
            std::ofstream cleaned_file(wstring_to_utf8(data_dir + L"\\cleaned\\cleaned_" + timestamp + L".txt"), std::ios::binary);
            if (cleaned_file.is_open()) {
                cleaned_file << wstring_to_utf8(cleaned_text);
                cleaned_file.close();
            }
        }

        return parser_file_path;
    }

    /**
     * @brief 写入相似度检测报告（标记超过阈值的模板）
     * @param data_dir 输出目录
     * @param parser_file_path 本次解析文件路径
     * @param comment_mean_emb 评论均值嵌入
     * @param decision 相似度决策结果
     * @param positive_threshold 正向阈值
     * @param negative_threshold 负向阈值
     */
    void WriteSimilarityReport(const std::wstring& data_dir,
        const std::wstring& parser_file_path,
        const std::vector<float>& comment_mean_emb,
        const SimilarityPolicy::Decision& decision,
        float positive_threshold,
        float negative_threshold) {
        std::filesystem::create_directories(data_dir + L"\\SimilarityCheck");
        const std::wstring report_path = data_dir + L"\\SimilarityCheck\\SimilarityCheck_" + GetTimestampString() + L".txt";
        std::ofstream out(wstring_to_utf8(report_path));
        if (!out.is_open()) return;

        out << "parser_file: " << wstring_to_utf8(parser_file_path) << "\n";
        out << "comment_mean_dim: " << comment_mean_emb.size() << "\n\n";

        for (const auto& group : decision.groups) {
            out << "[" << wstring_to_utf8(group.group_name) << "]\n";
            if (group.scores.empty()) {
                out << "no valid template json found\n\n";
                continue;
            }

            // 根据组名自动匹配正负向阈值，大小写不敏感，提升策略兼容性
            float threshold = 0.0f;
            std::wstring group_name = group.group_name;
            if (group_name.find(L"positive") != std::wstring::npos ||
                group_name.find(L"Positive") != std::wstring::npos) {
                threshold = positive_threshold;
            }
            else if (group_name.find(L"negative") != std::wstring::npos ||
                group_name.find(L"Negative") != std::wstring::npos) {
                threshold = negative_threshold;
            }

            // 输出分数并标记超过阈值项，★为命中标记
            for (const auto& score : group.scores) {
                out << wstring_to_utf8(score.template_name) << " => "
                    << std::fixed << std::setprecision(6) << score.similarity;
                if (score.similarity > threshold) {
                    out << " ★";
                }
                out << "\n";
            }
            out << "\n";
        }
        out.close();
    }

} // anonymous namespace

// ==================== FilterEngine 实现 ====================

/**
 * @brief 初始化过滤器引擎（加载ONNX模型）
 * @param model_path 模型文件路径
 * @return 初始化成功返回true
 */
bool FilterEngine::Initialize(const std::wstring& model_path) {
    if (!load_onnx_model(model_path.c_str())) {
        return false;
    }
    is_initialized_ = true;
    return true;
}

/**
 * @brief 关闭引擎，释放模型资源
 */
void FilterEngine::Shutdown() {
    if (is_initialized_) {
        unload_onnx_model();
        is_initialized_ = false;
    }
}

/**
 * @brief 设置数据输出根目录
 * @param data_dir 目录路径
 */
void FilterEngine::SetDataDirectory(const std::wstring& data_dir) {
    data_dir_ = data_dir;
}

/**
 * @brief 设置正/负向模板目录
 * @param positive_dir 正向模板路径
 * @param negative_dir 负向模板路径
 */
void FilterEngine::SetTemplateDirectories(const std::wstring& positive_dir,
    const std::wstring& negative_dir) {
    positive_template_dir_ = positive_dir;
    negative_template_dir_ = negative_dir;
}

/**
 * @brief 执行一次完整过滤周期：抓取→清洗→解析→编码→相似度匹配→执行动作
 * @param target_window 目标窗口句柄
 * @param mode 运行模式
 * @param positive_threshold 正向匹配阈值
 * @param negative_threshold 负向匹配阈值
 * @param record_data 是否记录中间数据
 * @return 过滤结果（成功/决策/消息）
 */
 /**
  * @brief 执行单次完整过滤周期
  * @details 流程：窗口抓取 → 文本清洗 → JSON解析 → 均值嵌入计算 → 相似度策略评估 → 执行决策
  * @see SimilarityPolicy::Evaluate
  */
FilterEngine::Result FilterEngine::ExecuteFilterCycle(
    HWND target_window,
    int mode,
    float positive_threshold,
    float negative_threshold,
    bool record_data) {

    Result result;
    result.success = false;
    result.decision = 1; // 默认滚动

    // 引擎未初始化，直接返回错误
    if (!is_initialized_) {
        result.message = "FilterEngine not initialized";
        return result;
    }

    // 无效窗口句柄校验，避免操作崩溃
    if (target_window == nullptr) {
        result.message = "Target window not set";
        return result;
    }

    // 1. 抓取原始页面内容
    std::wstring raw_text;
    if (!BusinessOps::CopyPageContent(target_window, raw_text)) {
        BusinessOps::ScrollAndRestorePosition(target_window);
        result.message = "Failed to copy page content";
        return result;
    }

    // 2. 文本清洗
    std::wstring cleaned_text = clean_douyin_text(raw_text);
    if (cleaned_text.empty()) {
        BusinessOps::ScrollAndRestorePosition(target_window);
        result.message = "Cleaned text is empty";
        return result;
    }

    // 3. 解析为JSON结构
    std::string json_str = parse_cleaned_text_to_json(cleaned_text);
    if (json_str.empty() || json_str == "[]") {
        BusinessOps::ScrollAndRestorePosition(target_window);
        result.message = "Parsed JSON is empty or empty array";
        return result;
    }

    // 4. 保存所有中间数据
    std::wstring data_dir = data_dir_.empty() ? L"." : data_dir_;
    std::wstring parser_file_path = SaveData(data_dir, raw_text, cleaned_text, json_str, record_data);

    // 5. 从JSON提取评论并计算均值嵌入
    std::vector<std::wstring> comment_texts;
    try {
        nlohmann::json parsed = nlohmann::json::parse(json_str);
        comment_texts = ExtractCommentTextsFromJson(parsed);
    }
    catch (...) {
        BusinessOps::ScrollAndRestorePosition(target_window);
        result.message = "Failed to parse JSON for comment extraction";
        return result;
    }

    if (comment_texts.empty()) {
        BusinessOps::ScrollAndRestorePosition(target_window);
        result.message = "No comment texts extracted";
        return result;
    }

    std::vector<float> comment_mean_emb;
    if (!BuildMeanEmbeddingFromTexts(comment_texts, comment_mean_emb)) {
        BusinessOps::ScrollAndRestorePosition(target_window);
        result.message = "Failed to build mean embedding from comments";
        return result;
    }

    // 6. 执行相似度匹配决策，传入模板加载回调函数
    const auto decision = SimilarityPolicy::Evaluate(
        comment_mean_emb,
        mode,
        positive_threshold,
        negative_threshold,
        positive_template_dir_,
        negative_template_dir_,
        BuildMeanEmbeddingFromJsonFile
    );

    WriteSimilarityReport(data_dir, parser_file_path, comment_mean_emb, decision,
        positive_threshold, negative_threshold);

    // 7. 根据决策执行滚动或停止：1=继续滚动，0=停止匹配
    result.success = true;
    if (decision.filter_result == 1) {
        BusinessOps::ScrollAndRestorePosition(target_window);
        result.decision = 1;
        result.message = "No match, scrolling to next";
    }
    else {
        result.decision = 0;
        result.message = "Match found, stopping loop";
    }

    return result;
}