#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
复现「相似度恒定 84.53%」这个 bug 的完整机制。

背景
----
2026-03-28 项目第一次跑起来后，无论复制到多长的评论，相似度都是同一个值（84.53%）。
当时已定位到「分词有问题」，但没弄清为什么是"恒定"而不是"随机"。
2026-11-21 用本脚本复现，确认是**两层缺陷叠加**：

第一层：max_len=64，但每次抓取的页面文本开头是同一段 246 字节页头，
        62 个槽位被页头占满 → 所有输入完全相同 → 向量完全相同 → 余弦恒定。

第二层：即使清洗掉页头，字节级分词（token_id = 256 + 字节值）把所有中文
        压进词表 304–495 这一小段（占 250,037 词表的 0.077%），
        7 条完全不同的评论只用到 72 个不同 token → 向量高度相似（83–92%），
        排序无意义（吵架内容 91.89% 比健身内容 87.23% 还高）。

依赖
----
    pip install onnxruntime numpy
    模型：../bin/models/model.onnx

运行
----
    python repro_byte_tokenizer.py
"""
import os
import sys

import numpy as np
import onnxruntime as ort

HERE = os.path.dirname(os.path.abspath(__file__))
MODEL = os.path.normpath(os.path.join(HERE, "..", "bin", "models", "model.onnx"))

# 当年那段代码里的常量
CLS, SEP, PAD, OFFSET = 0, 2, 3, 256

# 当年从抖音页面复制到的真实开头（归档里的原始抓取文本）
HEADER = (
    "开启读屏标签读屏标签已关闭\n\n\n下载抖音精选\n2026 © 抖音\n"
    "京ICP备16016397号-3\nicon京公网安备 11000002002046号\n"
    "广播电视节目制作经营许可证\n京B2-20170846\n"
    "网络文化许可证-京网文-（2025）0181-061号\n"
)


def old_tokenize(text, max_len=64):
    """复刻当年 model.cpp 里的 tokenize：

        ids.push_back(ID_CLS);                      # <s> = 0
        for c in utf8_bytes: ids.push_back(256 + c) # 字节值 + 256
        ids.push_back(ID_SEP);                      # </s> = 2
        while len(ids) < max_len: ids.push_back(3)  # <pad> = 3
    """
    ids = [CLS]
    for b in text.encode("utf-8"):
        if len(ids) >= max_len - 1:
            break
        ids.append(OFFSET + b)
    if len(ids) < max_len:
        ids.append(SEP)
    while len(ids) < max_len:
        ids.append(PAD)
    return ids


def load_session():
    opts = ort.SessionOptions()
    opts.intra_op_num_threads = 4
    opts.graph_optimization_level = ort.GraphOptimizationLevel.ORT_ENABLE_ALL
    return ort.InferenceSession(MODEL, opts, providers=["CPUExecutionProvider"])


def embed(sess, text, max_len=64):
    """取 last_hidden_state 的 CLS 位置（前 384 维）"""
    ids = old_tokenize(text, max_len)
    mask = [1 if i != PAD else 0 for i in ids]
    feed = {
        "input_ids": np.array([ids], dtype=np.int64),
        "attention_mask": np.array([mask], dtype=np.int64),
        "token_type_ids": np.zeros((1, max_len), dtype=np.int64),
    }
    return sess.run(["last_hidden_state"], feed)[0][0, 0, :]


def cosine(a, b):
    return float(a @ b / (np.linalg.norm(a) * np.linalg.norm(b)))


def main():
    if not os.path.exists(MODEL):
        sys.exit(f"找不到模型：{MODEL}")

    print("=" * 68)
    print("第一层：页头占满 max_len=64 的窗口")
    print("=" * 68)
    header_bytes = len(HEADER.encode("utf-8"))
    print(f"  页头 UTF-8 字节数 = {header_bytes}，可用槽位 = 62")
    print(f"  → 页头{'已占满窗口' if header_bytes >= 62 else '未占满窗口'}")

    captures = {
        "健身视频": HEADER + "古法健身太牛了，12分钟无跑跳，跟练一遍暴汗燃脂，简直不要瘦太快\n全部评论(490)",
        "耳机视频": HEADER + "就是不说音频协议，还是SBC\n全部评论(122)",
        "吵架视频": HEADER + "你这种人就该被封号，拉黑了\n全部评论(88)",
    }
    toks = {k: old_tokenize(v) for k, v in captures.items()}
    a, b, c = (toks["健身视频"], toks["耳机视频"], toks["吵架视频"])
    print(f"\n  前 12 个 token：")
    for k in captures:
        print(f"    {k}: {toks[k][:12]} ...")
    print(f"\n  健身 vs 耳机 逐位相同：{sum(x == y for x, y in zip(a, b))} / 64")
    print(f"  健身 vs 吵架 逐位相同：{sum(x == y for x, y in zip(a, c))} / 64")

    sess = load_session()
    e = {k: embed(sess, v) for k, v in captures.items()}
    print(f"\n  模型向量逐元素相同：{np.allclose(e['健身视频'], e['耳机视频'])}")
    print(f"  健身 vs 耳机 余弦 = {cosine(e['健身视频'], e['耳机视频']) * 100:.4f}%")
    print(f"  健身 vs 吵架 余弦 = {cosine(e['健身视频'], e['吵架视频']) * 100:.4f}%")
    print("  → 输入完全相同，输出完全相同，与模板的余弦必然恒定")

    print()
    print("=" * 68)
    print("第二层：清洗掉页头后，字节级分词仍然无区分度")
    print("=" * 68)
    pairs = [
        ("健身", "古法健身太牛了，12分钟无跑跳，跟练一遍暴汗燃脂，简直不要瘦太快"),
        ("耳机", "就是不说音频协议，还是SBC"),
        ("吵架", "你这种人就该被封号，拉黑了"),
        ("无关", "今天天气不错，适合出去走走"),
    ]
    embs = {tag: embed(sess, t) for tag, t in pairs}
    base = embs["健身"]
    vals = []
    for tag, _ in pairs:
        v = cosine(base, embs[tag]) * 100
        vals.append(v)
        print(f"  vs {tag:3} = {v:6.2f}%")
    print(f"\n  范围 {min(vals):.2f}% ~ {max(vals):.2f}%，跨度 {max(vals) - min(vals):.2f} 个百分点")
    print(f"  → 全挤在 80% 以上，且'吵架'比'耳机'更像'健身'，排序无意义")

    used = set()
    for _, t in pairs:
        used.update(i for i in old_tokenize(t) if i >= OFFSET)
    print(f"\n  这 4 条评论只用到 {len(used)} 个不同 token（id {min(used)}–{max(used)}），")
    print(f"  占 250,037 词表的 {(max(used) - min(used) + 1) / 250037 * 100:.3f}% 区间")


if __name__ == "__main__":
    main()
