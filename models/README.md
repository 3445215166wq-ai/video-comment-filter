# 模型文件说明

本程序运行需要两个模型文件，**均不纳入版本库**（合计约 453MB，超出 GitHub 单文件 100MB 上限）：

| 文件 | 大小 | 作用 |
|---|---|---|
| `model.onnx` | 448MB | 语义编码模型，把评论文本编码成句向量 |
| `sentencepiece.bpe.model` | 4.8MB | 分词器，把中文文本切成模型能吃的 token id |

放好之后的目录结构：

```
<可执行文件目录>/
├── QtFloatingBall.exe
├── onnxruntime.dll
├── models/
│   ├── model.onnx
│   └── sentencepiece.bpe.model
└── data/
    ├── config.json
    ├── positive_templates/
    └── negative_templates/
```

---

## ⚠️ 重要：这两个文件必须成对使用

`model.onnx` 和 `sentencepiece.bpe.model` 是**绑定**的：分词器产出的 token id 必须落在模型词表的编号空间内。换任何一个，token id 就对不上，句向量完全失效，而程序**不会报错**，只会输出垃圾相似度。

因此不能用 "随便找个 MiniLM 模型" 的方式替换。

### 本项目实际使用的词表体系

`model.onnx` 由 PyTorch 2.5.0 导出（`producer: pytorch`），标准 BERT 结构（`embeddings.word_embeddings` / `embeddings.token_type_embeddings`）。

`sentencepiece.bpe.model` 检测结果：

| 特殊词元 | 是否使用 |
|---|---|
| `<s>` / `</s>` / `<pad>` / `<unk>` | ✅ 使用（XLM-R 风格） |
| `[CLS]` / `[SEP]` / `[PAD]` | ❌ 不使用 |
| `▁` 空格前缀（含 `▁的` 等中文词元） | ✅ 使用 |

结论：**必须使用 SentencePiece / XLM-R 体系的词表**。
`all-MiniLM-L6-v2`、`bert-base-chinese` 这类 WordPiece（`[CLS]/[SEP]`）模型**不可用**。

---

## 1. `model.onnx`

代码对模型的硬性要求（见 `VideoFilteringCore/model/onnx_infer.cpp`）：

| 项 | 要求 |
|---|---|
| 输入 1 | `input_ids`，int64，形状 `[batch, 128]` |
| 输入 2 | `token_type_ids`，int64，形状 `[batch, 128]`（代码固定填 0） |
| 输入 3 | `attention_mask`，int64，形状 `[batch, 128]` |
| 输出 | `last_hidden_state`，float32，形状 `[batch, 128, hidden]` |
| 序列长度 | 固定 128（`max_len_`） |
| 池化方式 | 取位置 0（CLS）的向量 |
| 向量维度 | **不写死**，从输出张量 `dims.back()` 动态读取 |

因为维度是动态读取的，`hidden` 可以是 384 / 768 等任意值。但张量名、序列长度 128、三个输入张量缺一不可。

## 2. `sentencepiece.bpe.model`

必须是与 `model.onnx` 同源的 SentencePiece 分词模型，放置后文件名必须是 `sentencepiece.bpe.model`。

---

## 获取方式

这两个文件是项目自用模型，未随仓库分发。获取途径：

1. 项目的 GitHub Release 附件（如有）；
2. 联系作者获取。

拿到后直接放进 `models/` 目录即可。

---

## 路径解析逻辑

程序不依赖任何绝对路径，两个模型都按**可执行文件所在目录**相对定位：

| 文件 | 代码位置 | 解析规则 |
|---|---|---|
| `model.onnx` | `VideoFilteringCore/application/ApplicationService.cpp` | `<exe目录>\models\model.onnx` |
| `sentencepiece.bpe.model` | `VideoFilteringCore/model/text_preproc.cpp` | `<exe目录>\models\sentencepiece.bpe.model` |

从 Visual Studio 或 CMake 构建时，请把 `models` 目录复制到输出目录（通常是 `x64\Debug` 或 `x64\Release`）。

## 缺少模型时的表现

- 缺 `sentencepiece.bpe.model`：控制台打印 `[ERROR] 加载SentencePiece失败` 及期望路径，悬浮球无法进入筛选状态；
- 缺 `model.onnx`：ONNX Runtime 创建会话失败，语义相似度计算不可用。

程序不会崩溃，但筛选功能不可用。
