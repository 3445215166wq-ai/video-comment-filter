// test_policy.cpp
//
// 决策矩阵断言测试 —— 纯逻辑，不需要模型、不需要分词器、不需要 Qt。
//
// 目的：把 SimilarityPolicy::Evaluate() 的真值表全部钉死，防止"同一业务规则被
// 两处代码分别表达、返回相反结果"这类缺陷复发。
//
// 历史背景：2026-11-21 发现"负向命中"在两种模式下返回了相反的值——
//   仅负向模式：neg_hit ? 1 : 0  （命中滚动，正确）
//   双向模式  ：直接 0            （命中却停在该视频上，错误）
// 两段代码单独看都自洽，只有把 8 种组合并列成表才暴露矛盾。本文件就是那张表。
//
// 构造方法：评论向量固定为 (1,0)；模板向量取 (c, sqrt(1-c^2))，
// 于是两者的余弦恰好等于 c，可精确控制"是否超过阈值"。

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "core/similarity_policy.h"

namespace fs = std::filesystem;

namespace {

constexpr float POS_THRESHOLD = 0.7f;  // 程序默认正向阈值
constexpr float NEG_THRESHOLD = 0.8f;  // 程序默认负向阈值

// 目标余弦（由测试用例设定，回调据此造模板向量）
float g_pos_cos = 0.1f;
float g_neg_cos = 0.1f;

int g_pass = 0;
int g_fail = 0;

// 与评论向量 (1,0) 的余弦恰好为 c 的二维向量
std::vector<float> VecWithCos(float c) {
    // 注意：不使用 std::max —— 项目里全局 NOMINMAX 曾导致裸 max() 报 C3861
    float rest = 1.0f - c * c;
    float s = rest > 0.0f ? std::sqrt(rest) : 0.0f;
    return { c, s };
}

// 替代真实的 JSON 加载：按目录名返回可控向量
bool FakeMeanEmbeddingBuilder(const std::wstring& path, std::vector<float>& out) {
    if (path.find(L"positive") != std::wstring::npos) {
        out = VecWithCos(g_pos_cos);
    }
    else {
        out = VecWithCos(g_neg_cos);
    }
    return true;
}

void Check(const char* name,
           int mode,
           float pos_cos,
           float neg_cos,
           const std::wstring& pos_dir,
           const std::wstring& neg_dir,
           int expect) {
    g_pos_cos = pos_cos;
    g_neg_cos = neg_cos;

    const std::vector<float> comment = { 1.0f, 0.0f };
    SimilarityPolicy::Decision d = SimilarityPolicy::Evaluate(
        comment, mode, POS_THRESHOLD, NEG_THRESHOLD, pos_dir, neg_dir,
        FakeMeanEmbeddingBuilder);

    const bool ok = (d.filter_result == expect);
    std::printf("  [%s] %-42s mode=%d  pos=%.2f neg=%.2f  -> %d (期望 %d)\n",
                ok ? "PASS" : "FAIL", name, mode, pos_cos, neg_cos,
                d.filter_result, expect);
    if (ok) ++g_pass; else ++g_fail;
}

// 造一个只含一个 .json 的模板目录（BuildGroupScores 只遍历 .json）
std::wstring MakeTemplateDir(const fs::path& parent, const std::wstring& name) {
    fs::path dir = parent / name;
    fs::create_directories(dir);
    std::ofstream(dir / "case.json") << "{}";
    return dir.wstring();
}

} // namespace

int main() {
    const int M_NONE = 0;
    const int M_POS = static_cast<int>(SimilarityPolicy::Mode::Positive);                 // 1
    const int M_NEG = static_cast<int>(SimilarityPolicy::Mode::Negative);                 // 2
    const int M_BOTH = M_POS | M_NEG;                                                      // 3

    fs::path root = fs::temp_directory_path() / L"vcf_test_policy";
    fs::remove_all(root);
    const std::wstring pos_dir = MakeTemplateDir(root, L"positive_templates");
    const std::wstring neg_dir = MakeTemplateDir(root, L"negative_templates");

    std::printf("==============================================================\n");
    std::printf("决策矩阵断言（阈值：正向 %.1f / 负向 %.1f）\n", POS_THRESHOLD, NEG_THRESHOLD);
    std::printf("返回值语义：1 = 滚动到下一条   0 = 停在该视频\n");
    std::printf("==============================================================\n\n");

    std::printf("-- 负向命中的情形（本次修复的核心）--\n");
    Check("仅负向 + 负向命中 -> 必须滚动", M_NEG, 0.1f, 0.9f, pos_dir, neg_dir, 1);
    Check("双向   + 负向命中 -> 必须滚动 (原为 0，已修)", M_BOTH, 0.1f, 0.9f, pos_dir, neg_dir, 1);

    std::printf("\n-- 仅负向模式 --\n");
    Check("仅负向 + 负向未命中 -> 停在该视频", M_NEG, 0.1f, 0.1f, pos_dir, neg_dir, 0);

    std::printf("\n-- 双向模式（负向未命中时才看正向）--\n");
    Check("双向 + 负向未命中 + 正向命中 -> 停在该视频", M_BOTH, 0.9f, 0.1f, pos_dir, neg_dir, 0);
    Check("双向 + 负向未命中 + 正向未命中 -> 滚动", M_BOTH, 0.1f, 0.1f, pos_dir, neg_dir, 1);

    std::printf("\n-- 仅正向模式 --\n");
    Check("仅正向 + 正向命中 -> 停在该视频", M_POS, 0.9f, 0.1f, pos_dir, neg_dir, 0);
    Check("仅正向 + 正向未命中 -> 滚动", M_POS, 0.1f, 0.1f, pos_dir, neg_dir, 1);

    std::printf("\n-- 边界 --\n");
    Check("未启用任何模式 -> 停在该视频", M_NONE, 0.9f, 0.9f, pos_dir, neg_dir, 0);

    std::printf("\n==============================================================\n");
    std::printf("结果：%d passed / %d failed\n", g_pass, g_fail);
    std::printf("==============================================================\n");

    fs::remove_all(root);
    return g_fail == 0 ? 0 : 1;
}
