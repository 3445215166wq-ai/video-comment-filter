# 参赛与答辩材料存档

> 比赛已经结束，这里只做**留痕**：保存不可再生的东西（自己写的文档、录的视频），
> 不保存可再生产的东西（模型、安装包）。

| 目录 | 内容 | 说明 |
|---|---|---|
| `答辩/` | 答辩用 PPT（5.3 MB） | 作品名《MiniLM 语义驱动：短视频智能筛选系统》 |
| `设计文档/` | 设计与开发文档 PDF ×2 + DOCX ×1（1.4 MB） | 参赛时提交的设计开发文档 |
| `演示视频/` | 演示视频 ×3（24 MB） | 功能演示录屏 |
| `源码存档/` | `yuanma.zip`（24 KB） | 早期 Win32 原型源码，仅作历史留痕；**当前源码见仓库根目录** |

## 当初的参赛信息

- 作品名称：基于 MiniLM 的短视频个性化智能筛选系统
- 类型：软件应用类 · AI 智能桌面工具
- 平台：Windows / C++17 / Qt 6 / ONNX Runtime / MiniLM 离线语义模型
- 团队：3 人

## 没有搬过来的东西（可再生，已删）

| 项 | 大小 | 为什么不留 |
|---|---|---|
| `model.onnx` | 448.5 MB | 已托管在 [Releases](https://github.com/3445215166wq-ai/video-comment-filter/releases)，本地 `bin/models/` 也有一份 |
| `onnxruntime-win-x64-1.14.1.zip` | 39.8 MB | 官方可下载 |
| `cmake-4.3.1-windows-x86_64.msi` | 36.4 MB | 官方可下载 |
| `sentencepiece-0.2.1-Windows-AMD64.7z` | 1.2 MB | 官方可下载 |
| `sentencepiece.bpe.model` | 4.8 MB | 仓库 `models/` 已有（上游原版） |

## 相关文档

- 项目说明：[`../README.md`](../../README.md)
- 架构设计：[`../ARCHITECTURE.md`](../ARCHITECTURE.md)
- 工程日志：[`../ENGINEERING_LOG.md`](../ENGINEERING_LOG.md)
- 故障复盘：[`../TOKENIZER_ALIGNMENT.md`](../TOKENIZER_ALIGNMENT.md)
- 经验教训：[`../LESSONS.md`](../LESSONS.md)
