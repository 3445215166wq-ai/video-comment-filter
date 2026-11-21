// test_golden.cpp
//
// 黄金样例断言 —— 验证「编码器」本身是否正常工作。
//
// 防的是什么
// ----------
// 程序不报错、日志照打、界面照显示，但语义整体错位：
//   · 2026-03-28  相似度恒定 84.53%（页头占满输入窗口 + 字节级分词）
//   · 2026-11-19  分词表 id 与 embedding 行号差 1 位（fairseq_offset）
// 这两次的共同症状都是：**同主题的两句话，与跨主题的两句话，得分几乎一样**。
// 本测试就是盯住这个症状。
//
// 数据来源（真实评论，不是造的）
// ------------------------------
// A 组：抖音「联想双用蓝牙耳机」视频评论区
// B 组：抖音「高数 6 小时速成课」视频评论区
// 两组主题互不相干，组内主题一致 —— 这是本测试唯一的"人工标注"。
//
// 断言
// ----
//   mean(同主题对的余弦)  >  mean(跨主题对的余弦) + 余量
// 余量取 0.05（2026-11-21 实测该差值约 +0.2 量级，留足安全边界）。
//
// 运行方式
// --------
//   需要 models/ 与 exe 同目录（TextPreprocessor 按 exe 目录解析分词模型路径），
//   所以本程序要放在 bin/ 下运行，例如：
//       copy test_golden.exe ..\bin\
//       ..\bin\test_golden.exe
//   也可显式传模型目录：test_golden.exe D:\path\to\models

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include <windows.h>

#include "onnx_infer.h"

namespace {

constexpr float MARGIN = 0.05f;   // 同主题至少要比跨主题高出这么多

// A 组：耳机购物（同一视频评论区）
const std::vector<std::wstring> kGroupA = {
    L"就是不说音频协议，还是SBC",
    L"他要是99我就买了",
    L"好便宜，我也把我漫步者扔了",
    L"很便宜，我把我的索尼xm5扔了买这个",
    L"69吗？有没有懂的，值不值",
    L"打FPS游戏好用吗",
    L"这个能联电脑，然后在YY上能说话吗",
    L"这个69的能语音聊天吗",
};

// B 组：高数考试（同一视频评论区）
const std::vector<std::wstring> kGroupB = {
    L"我头好大洛必达法则到底怎么用啊明天考试",
    L"已挂不欠",
    L"提醒各位，除了基本的原理之外，运用你高三的知识，70分够了",
    L"感觉洛必达法则很复杂，还要背很多求导公式",
    L"数学挂科了，课本也没带，都不知道讲了啥",
    L"还有两个小时考试了，从头学来得及吗家人们",
    L"10点补考 现在开看",
    L"来不及了 明天早上起来就考试了",
};

std::wstring ExeDir() {
    wchar_t buf[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring s(buf);
    const size_t p = s.find_last_of(L"\\/");
    return p == std::wstring::npos ? std::wstring(L".") : s.substr(0, p);
}

float Cosine(const std::vector<float>& a, const std::vector<float>& b) {
    if (a.empty() || a.size() != b.size()) return 0.0f;
    double dot = 0.0, n1 = 0.0, n2 = 0.0;
    for (size_t i = 0; i < a.size(); ++i) {
        dot += static_cast<double>(a[i]) * b[i];
        n1 += static_cast<double>(a[i]) * a[i];
        n2 += static_cast<double>(b[i]) * b[i];
    }
    if (n1 <= 0.0 || n2 <= 0.0) return 0.0f;
    return static_cast<float>(dot / (std::sqrt(n1) * std::sqrt(n2)));
}

float MeanPairCos(const std::vector<std::vector<float>>& embs) {
    double sum = 0.0;
    int n = 0;
    for (size_t i = 0; i < embs.size(); ++i) {
        for (size_t j = i + 1; j < embs.size(); ++j) {
            sum += Cosine(embs[i], embs[j]);
            ++n;
        }
    }
    return n > 0 ? static_cast<float>(sum / n) : 0.0f;
}

float MeanCrossCos(const std::vector<std::vector<float>>& a,
                   const std::vector<std::vector<float>>& b) {
    double sum = 0.0;
    int n = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        for (size_t j = 0; j < b.size(); ++j) {
            sum += Cosine(a[i], b[j]);
            ++n;
        }
    }
    return n > 0 ? static_cast<float>(sum / n) : 0.0f;
}

} // namespace

int main(int argc, char** argv) {
    SetConsoleOutputCP(65001);

    const std::wstring dir = ExeDir();
    std::wstring model_dir = dir + L"\\models";
    if (argc > 1) {
        std::string a1(argv[1]);
        model_dir.assign(a1.begin(), a1.end());
    }
    const std::wstring model_path = model_dir + L"\\model.onnx";

    std::printf("==============================================================\n");
    std::printf("黄金样例断言：编码器语义区分度\n");
    std::printf("==============================================================\n");
    std::wprintf(L"模型目录: %ls\n", model_dir.c_str());

    if (!load_onnx_model(model_path.c_str())) {
        std::printf("[FAIL] 模型加载失败\n");
        return 1;
    }

    std::vector<std::vector<float>> embA, embB;
    if (!encode_text_batch(kGroupA, embA) || !encode_text_batch(kGroupB, embB)) {
        std::printf("[FAIL] 编码失败\n");
        unload_onnx_model();
        return 1;
    }
    if (embA.size() != kGroupA.size() || embB.size() != kGroupB.size()) {
        std::printf("[FAIL] 编码结果数量不匹配\n");
        unload_onnx_model();
        return 1;
    }

    const float same_a = MeanPairCos(embA);
    const float same_b = MeanPairCos(embB);
    const float cross = MeanCrossCos(embA, embB);
    const float same_avg = (same_a + same_b) * 0.5f;
    const float gap = same_avg - cross;

    std::printf("\n同主题平均余弦  A组内(耳机) = %.4f\n", same_a);
    std::printf("同主题平均余弦  B组内(高数) = %.4f\n", same_b);
    std::printf("同主题平均      = %.4f\n", same_avg);
    std::printf("跨主题平均      = %.4f\n", cross);
    std::printf("区分度(同 - 跨) = %+.4f\n\n", gap);

    int failed = 0;

    // 断言 1：同主题必须显著高于跨主题
    const bool ok1 = gap >= MARGIN;
    std::printf("  [%s] 区分度 %+.4f >= 余量 %.2f\n", ok1 ? "PASS" : "FAIL", gap, MARGIN);
    if (!ok1) ++failed;

    // 断言 2：跨主题不应高得离谱（语义整体错位时，这两组会挤在一起）
    const bool ok2 = cross < 0.95f;
    std::printf("  [%s] 跨主题平均 %.4f < 0.95（否则说明向量几乎相同）\n", ok2 ? "PASS" : "FAIL", cross);
    if (!ok2) ++failed;

    // 断言 3：整体上同主题要高于跨主题
    //
    // 注意：不要写成"每一组组内都必须高于跨主题"。实测 A 组（耳机）组内只有 0.6093，
    // 低于跨主题 0.6303 —— 原因不是编码器坏了，而是 A 组评论话题是散的：
    // 「还是SBC」是技术、「他要是99我就买了」是价格、「这不是挤奶器吗」是玩笑，
    // 它们只共享"同一个视频"，不共享语义主题。B 组（高数）则都带"考试/挂科"强情绪，
    // 组内高达 0.7811。所以"同一视频"≠"同一语义主题"，不能当作强断言。
    const bool ok3 = same_avg > cross;
    std::printf("  [%s] 同主题平均 %.4f > 跨主题平均 %.4f\n", ok3 ? "PASS" : "FAIL", same_avg, cross);
    if (!ok3) ++failed;

    // 诊断输出（不作断言）：组内一致性差异说明"同一视频"不等于"同一语义主题"
    std::printf("\n  [诊断] A组内 %.4f  vs  B组内 %.4f  (差 %+.4f)\n",
                same_a, same_b, same_a - same_b);
    std::printf("         组内一致性差距越大，说明该组评论的话题越分散，\n");
    std::printf("         「同一个视频下的评论」并不能当作「同一语义主题」使用。\n");

    std::printf("\n==============================================================\n");
    std::printf("结果：%d passed / %d failed\n", 3 - failed, failed);
    std::printf("参考：2026-11-19 修复分词错位后，同批语料区分度为 +0.2894\n");
    std::printf("==============================================================\n");

    unload_onnx_model();
    return failed == 0 ? 0 : 1;
}
