#pragma once
#include <string>
#include <vector>
#include <functional>

/// 相似度策略命名空间：提供文本嵌入与模板的相似度匹配、过滤决策核心逻辑
namespace SimilarityPolicy {

    /**
     * @brief 检测模式枚举（支持位运算组合，可同时启用正向/负向检测）
     */
    enum class Mode : int {
        None = 0,        ///< 无检测模式
        Positive = 1 << 0,   ///< 正向模板检测（匹配目标内容）
        Negative = 1 << 1    ///< 负向模板检测（排除干扰内容）
    };

    /**
     * @brief 单个模板文件的相似度得分结果
     */
    struct TemplateScore {
        std::wstring template_name;  ///< 模板文件名
        float similarity = 0.0f;     ///< 与目标文本的相似度分数
    };

    /**
     * @brief 一组模板（正向/负向）的相似度得分集合
     */
    struct GroupScores {
        std::wstring group_name;             ///< 分组名称（如 positive_templates / negative_templates）
        std::vector<TemplateScore> scores;    ///< 分组内所有模板的相似度得分列表
    };

    /**
     * @brief 相似度策略最终决策结果
     */
    struct Decision {
        int filter_result = 0;               ///< 过滤执行结果：1=滚动下一条，0=停止匹配
        std::vector<GroupScores> groups;      ///< 所有模板分组的详细得分数据（用于报告生成）
    };

    /**
     * @brief 均值嵌入构建器回调函数类型
     * @details 外部传入的函数：从JSON文件路径加载并生成均值嵌入向量
     */
    using MeanEmbeddingBuilder = std::function<bool(const std::wstring&, std::vector<float>&)>;

    /**
     * @brief 执行相似度匹配与过滤决策（核心对外接口）
     * @param comment_mean_emb 待检测评论的均值嵌入向量
     * @param mode_value 运行模式值（Mode枚举位运算组合）
     * @param positive_threshold 正向模板匹配阈值
     * @param negative_threshold 负向模板匹配阈值
     * @param positive_template_dir 正向模板文件夹路径
     * @param negative_template_dir 负向模板文件夹路径
     * @param build_mean_embedding_from_json_file 从JSON文件构建均值嵌入的回调函数
     * @return 完整决策结果（过滤指令+详细得分）
     */
    Decision Evaluate(
        const std::vector<float>& comment_mean_emb,
        int mode_value,
        float positive_threshold,
        float negative_threshold,
        const std::wstring& positive_template_dir,
        const std::wstring& negative_template_dir,
        const MeanEmbeddingBuilder& build_mean_embedding_from_json_file
    );

} // namespace SimilarityPolicy