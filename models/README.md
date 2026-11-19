# 模型文件说明

本程序运行需要两个模型文件：

| 文件 | 大小 | 作用 | 存放 |
|---|---|---|---|
| `model.onnx` | 448MB | 语义编码模型，把评论文本编码成句向量 | 需从 [Releases](https://github.com/3445215166wq-ai/video-comment-filter/releases) 下载（超 GitHub 单文件 100MB 上限） |
| `sentencepiece.bpe.model` | 5.3MB | 分词器，把文本切成模型能吃的 token id | **已随仓库分发**，见本目录 |

> `sentencepiece.bpe.model` 由 `tools/rebuild_sentencepiece_model.py` 从原始 `tokenizer.json` 重建而成，
> 保证 piece 顺序与模型 embedding 行号一致。
>
> ⚠️ **不要直接使用上游仓库自带的 `sentencepiece.bpe.model`。** XLM-R 系的 spm 有一个已知特性：
> **它的原始 id 与模型实际使用的 token id 相差 1 位**（`<unk>=0/<s>=1/</s>=2/','=3…` vs 模型的
> `<s>=0/<pad>=1/</s>=2/<unk>=3/','=4…`）。HuggingFace 在 `XLMRobertaTokenizer` 里用
> `fairseq_offset = 1` 显式补偿；**绕过 HF、直接用 SentencePiece 原生库时，调用方必须自己加这个偏移**，
> 否则整体错位 1 位且**不报错**。本仓库的分词表已把偏移固化进文件，故 C++ 侧无需再做补偿。
> 详见 [`docs/TOKENIZER_ALIGNMENT.md`](../docs/TOKENIZER_ALIGNMENT.md)。

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

因此替换时必须同族配对，不能混搭。

### 上游来源（已确认）

| 项 | 值 |
|---|---|
| HuggingFace 仓库 | **`sentence-transformers/paraphrase-multilingual-MiniLM-L12-v2`** |
| 确认依据 1 | 本项目的 `tokenizer.json` 与该仓库发布的 **SHA256 完全一致**（`2c3387be76557bd4…`） |
| 确认依据 2 | 该仓库 `config.json` 的 `vocab_size` = **250037**，与 ONNX `word_embeddings` 行数精确吻合 |
| 确认依据 3 | hidden 384 / 12 层 / `max_position_embeddings` 512 / `type_vocab_size` 2 全部对上 |

需要微调时，应从该仓库获取 `model.safetensors`（约 449 MB）+ `config.json`，而不是拿 `model.onnx` 反推。

### 本项目模型的实测规格

以下数值由直接解析 `model.onnx` 的初始张量与推理输出得到，非推测：

| 项 | 实测值 |
|---|---|
| 总参数量 | 117,505,920（117.5M） |
| 隐藏维 | **384**（输出 `last_hidden_state` 形状 `[batch, seq, 384]`） |
| 编码层数 | **12** |
| `word_embeddings` | (250037, 384) → 词表 250,037 行 |
| `position_embeddings` | (512, 384) |
| `token_type_embeddings` | (2, 384) |
| fp32 理论体积 | 448.2 MB（文件实际 448.5 MB，吻合） |
| 导出工具 | PyTorch（`producer: pytorch`，图名 `main_graph`） |

384 维 + 12 层 + 25 万多语言词表 + 117.5M 参数，对应 **多语言 MiniLM-L12-H384** 这一类模型。

文件体积之所以达到 448MB，是因为 `250037 × 384` 的嵌入矩阵本身就占 96M 参数（约 384MB），而不是层数多——这一点常被误判为"BERT-base 量级"。

### 分词表实测结果

| 项 | 实测值 |
|---|---|
| 词表规模 | 250,002 个 piece |
| `<s>` | id **0**（类型 CONTROL） |
| `<pad>` | id **1**（类型 CONTROL） |
| `</s>` | id **2**（类型 CONTROL） |
| `<unk>` | id **3**（类型 UNKNOWN） |
| `<mask>` | id **250001**（类型 CONTROL） |
| `[CLS]` / `[SEP]` / `[PAD]` | 不存在 |

两点结论：

1. 这套 `<s>` / `<pad>` / `</s>` / `<unk>` 属于 **XLM-R 体系的 SentencePiece**，与多语言 MiniLM 原生配对。
2. 特殊词元的 id 排布**必须与模型 `word_embeddings` 的行序一致**。本仓库历史上直接沿用了上游自带的 `sentencepiece.bpe.model`，而 XLM-R 系 spm 的原始 id 比模型 id **少 1 位**（HF 用 `fairseq_offset = 1` 补偿），C++ 侧没有补偿，导致 249,996 个正常词元**整体前移 1 位**，语义筛选失效且全程不报错。现已用 `tools/rebuild_sentencepiece_model.py` 按 `tokenizer.json` 顺序重建（偏移已固化），修复细节见 [`docs/TOKENIZER_ALIGNMENT.md`](../docs/TOKENIZER_ALIGNMENT.md)。

### 哪些模型可以替换

词表规模必须与模型 `word_embeddings` 的行数一致，且分词器必须同族。

- ✅ 可替换：其他同族多语言 MiniLM（XLM-R 词表，词表规模匹配）
- ❌ 不可替换：`all-MiniLM-L6-v2`（词表 30522）、`bert-base-chinese`（词表 21128）等 WordPiece `[CLS]/[SEP]` 模型——词表规模与 id 空间都对不上

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

`sentencepiece.bpe.model` 已随仓库分发（5.3MB），无需下载。

只有 `model.onnx`（448MB，超出 GitHub 单文件 100MB 上限）需要单独获取，作为 Release 附件提供：

**https://github.com/3445215166wq-ai/video-comment-filter/releases/latest**

```bash
gh release download v1.0.0 --repo 3445215166wq-ai/video-comment-filter --pattern model.onnx --dir models
```

拿到后放进 `models/` 目录即可。

## 重建分词表（仅在做模型迁移时需要）

若更换了模型（`model.onnx`），其 `word_embeddings` 行序可能变化，此时必须同步重建分词表：

```bash
python tools/rebuild_sentencepiece_model.py \
    --tokenizer-json <原始 tokenizer.json> \
    --spm  models/sentencepiece.bpe.model \
    --out  models/sentencepiece.bpe.model.new
```

脚本会以 `tokenizer.json` 的 piece 顺序为准、沿用现有 score、显式写入 `unk_id/bos_id/eos_id/pad_id`，并在写出后自动回读校验。

**不要直接用上游仓库自带的 spm 文件**（它的特殊词元编号与模型权重不匹配），理由见 [`docs/TOKENIZER_ALIGNMENT.md`](../docs/TOKENIZER_ALIGNMENT.md)。

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
