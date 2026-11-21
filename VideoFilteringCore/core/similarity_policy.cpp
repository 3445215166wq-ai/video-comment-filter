#include "core/similarity_policy.h"
#include "utils/utils.h"
#include <filesystem>

namespace SimilarityPolicy {

    /**
     * @brief 构建单个模板分组的相似度得分
     * @param group_name 分组名称（如正向/负向模板组）
     * @param folder_path 模板文件所在文件夹路径
     * @param comment_mean_emb 待检测评论的均值嵌入向量
     * @param build_mean_embedding_from_json_file 回调函数：从JSON文件构建均值嵌入
     * @return 包含所有模板文件名与对应相似度的分组结果
     */
    static GroupScores BuildGroupScores(
        const std::wstring& group_name,
        const std::wstring& folder_path,
        const std::vector<float>& comment_mean_emb,
        const MeanEmbeddingBuilder& build_mean_embedding_from_json_file
    ) {
        GroupScores group;
        group.group_name = group_name;

        try {
            std::filesystem::path dir(folder_path);
            // 目录不存在/非目录，直接返回空分组
            if (!std::filesystem::exists(dir) || !std::filesystem::is_directory(dir)) return group;

            for (auto& p : std::filesystem::directory_iterator(dir)) {
                // 仅处理常规文件，跳过目录/链接等
                if (!p.is_regular_file()) continue;
                auto ext = p.path().extension().wstring();
                // 仅加载 .json / .JSON 后缀的模板文件
                if (ext != L".json" && ext != L".JSON") continue;

                std::vector<float> mean_emb;
                // 嵌入构建失败/为空，跳过当前模板
                if (!build_mean_embedding_from_json_file(p.path().wstring(), mean_emb)) continue;
                if (mean_emb.empty()) continue;

                TemplateScore score;
                score.template_name = p.path().filename().wstring();
                // 计算评论与模板的余弦相似度
                score.similarity = Utils::calc_similarity(comment_mean_emb, mean_emb);
                group.scores.push_back(std::move(score));
            }
        }
        catch (...) {
            // 文件/目录异常，直接返回当前已计算结果
            return group;
        }

        return group;
    }

    /**
     * @brief 执行相似度策略评估，根据正负模板匹配结果输出过滤决策
     * @param comment_mean_emb 待检测评论均值嵌入
     * @param mode_value 运行模式（位运算组合：Positive / Negative）
     * @param positive_threshold 正向模板匹配阈值
     * @param negative_threshold 负向模板匹配阈值
     * @param positive_template_dir 正向模板文件夹路径
     * @param negative_template_dir 负向模板文件夹路径
     * @param build_mean_embedding_from_json_file 模板嵌入构建回调
     * @return 完整决策结果：分组得分 + 最终过滤指令
     */
    Decision Evaluate(
        const std::vector<float>& comment_mean_emb,
        int mode_value,
        float positive_threshold,
        float negative_threshold,
        const std::wstring& positive_template_dir,
        const std::wstring& negative_template_dir,
        const MeanEmbeddingBuilder& build_mean_embedding_from_json_file
    ) {
        // 位运算解析启用模式：支持单独正向、单独负向、双向检测
        const bool positive_enabled = (mode_value & static_cast<int>(Mode::Positive)) != 0;
        const bool negative_enabled = (mode_value & static_cast<int>(Mode::Negative)) != 0;

        Decision decision;

        // 负向模板优先级最高：先检测负向
        bool neg_hit = false;
        if (negative_enabled) {
            auto neg_group = BuildGroupScores(L"negative_templates", negative_template_dir, comment_mean_emb, build_mean_embedding_from_json_file);
            // 任意负向模板超过阈值即判定命中
            for (const auto& score : neg_group.scores) {
                if (score.similarity >= negative_threshold) {
                    neg_hit = true;
                    break;
                }
            }
            decision.groups.push_back(std::move(neg_group));
        }

        // 规则一：负向命中 → 一律滚动到下一条(1)，与启用哪种模式无关。
        //
        // 历史缺陷：此前的实现把同一件事写在了两处，且返回值相反——
        //   "仅负向"分支是 neg_hit ? 1 : 0（命中滚动，正确）
        //   "双向"分支则是直接 0（命中却停在该视频上，错误）
        // 两段单独看都自洽，放进同一张真值表才暴露矛盾。现合并为唯一一处表达。
        // 命中后立即返回，因此双向模式下仍省去一次正向计算（短路行为不变）。
        if (negative_enabled && neg_hit) {
            decision.filter_result = 1;
            return decision;
        }

        // 规则二：仅启用负向且未命中 → 没有理由滚动，停在该视频(0)
        if (negative_enabled && !positive_enabled) {
            decision.filter_result = 0;
            return decision;
        }

        // 正向检测：仅当负向未命中/未启用时执行
        bool pos_hit = false;
        if (positive_enabled) {
            auto pos_group = BuildGroupScores(L"positive_templates", positive_template_dir, comment_mean_emb, build_mean_embedding_from_json_file);
            // 任意正向模板超过阈值即判定命中
            for (const auto& score : pos_group.scores) {
                if (score.similarity >= positive_threshold) {
                    pos_hit = true;
                    break;
                }
            }
            decision.groups.push_back(std::move(pos_group));
        }

        // 规则三：正向命中 → 停在该视频(0)；未命中 → 滚动下一条(1)
        if (positive_enabled) {
            decision.filter_result = pos_hit ? 0 : 1;
            return decision;
        }

        // 默认：无任何模式启用 → 停止(0)
        decision.filter_result = 0;
        return decision;
    }

} // namespace SimilarityPolicy