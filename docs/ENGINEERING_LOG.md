# 工程日志

> **这是什么**：按时间线记录本项目的关键技术决策、踩过的坑与修复方式。
>
> **为什么记**：构建产物、调试符号这类东西可以从源码机械重建，留几 GB 没有意义；
> 而"当时为什么这样选、坑是怎么定位的"无法重建，只能用文字留。这份文件只有几 KB，
> 但它记录的信息密度远高于任何生成物。
>
> **诚实声明**：早期阶段（2026-03 ~ 05）的时间点来自**文件时间戳反推**，不是当时的笔记；
> 2026-11-19 之后的记录来自实际操作。凡属推断的地方会标注。

---

## 时间线总览

| 时间 | 阶段 | 主要产物 |
|---|---|---|
| 2026-03-28 | 取得语义模型与分词器 | `model.onnx`、`tokenizer.json` |
| 2026-03-31 | 旧 Win32 原型完成 | `原版\VideoDemo.exe`、`原版\架构.txt` |
| 2026-04-17 | 旧原型源码最后维护 | `VideoDemo\src\` |
| 2026-04-22 ~ 23 | 架构重构：分层 + 静态库 | `VideoFiltering\`（引入 `IApplicationController` 等） |
| 2026-04-25 | Qt 悬浮球版本完成 | `QtFloatingBall\`、纯源码快照、设计开发文档 PDF |
| 2026-05-07 | 参赛材料提交 | `202302020628…\README.txt` |
| 2026-05-10 ~ 11 | 模型加载与主入口调整 | `onnx_infer.cpp`、`main_videofiltering.cpp` |
| **2026-11-19** | **仓库化 + 系统性缺陷修复** | `video-comment-filter/`（详见下文） |

---

## 阶段一：旧 Win32 原型（~2026-03）

- 单体程序，`src/main.cpp` + `clean` / `model` / `utils` 三块。
- **核心问题**：UI 与业务逻辑完全耦合，换界面等于重写。
- 留存证据：`原版\VideoDemo.exe`、`VideoDemo\src\core\app.cpp`（其中把模型路径硬编码为
  `C:\Users\Administrator\Desktop\VideoDemo\x64\Release\models\model.onnx`）。

## 阶段二：架构重构（2026-04-17 ~ 04-23）

引入「微内核」式分层，把业务能力收进抽象接口之后：

```
UI → IApplicationController（纯虚接口）→ ApplicationService（编排）
                                          ├─ StateManager
                                          ├─ ConfigurationManager
                                          └─ FilterEngine
```

- **决策**：依赖方向单向、UI 只见接口。理由是可替换 UI、可单独测试核心。
- 证据：`VideoFiltering\ARCHITECTURE.md`（04-22）。

## 阶段三：Qt 悬浮球 UI（2026-04-25）

- 新增 Qt 表现层；核心编译为 **静态库 `VideoFilteringCore.lib`，无 Qt 依赖**。
- 交互形态定为两级：悬浮球 → 一级快捷面板 → 二级完整参数面板；全局热键 `F8`。
- 同日出纯源码快照与《设计与开发文档》PDF。

## 阶段四：参赛提交（2026-05-07）

材料整理为「作品文件夹 / 代码与素材 / 设计文档 / 演示视频」四段式。

---

## 阶段五：仓库化与系统性缺陷修复（2026-11-19）

这是本项目投入最集中的一次工程整理，按发现顺序记录。

### 5.1 仓库化：4 GB → 25 MB

原工作区 9 个目录、21,967 个文件、4.25 GB，同一套代码复制了 5 份，
`model.onnx`（448 MB）存了 5 份。剔除以下内容后建仓：

| 剔除项 | 体积 | 理由 |
|---|---|---|
| 重复 `model.onnx` | 897 MB | 走 Release 附件 |
| 构建产物 / VS 缓存 | 984 MB | 可由源码重建 |
| 旧仓库 `.git` | 445 MB | 远程仍在 |
| 调试符号 `.pdb` | 414 MB | 可重建 |
| Doxygen 生成 HTML | 253 MB | 可重生成（且已陈旧） |
| 第三方 SDK 整棵源码树 | 124 MB | 公开可下载 |

保留约 23 MB 的**构建必需**依赖（ONNX Runtime 头文件/导入库/DLL、SentencePiece 静态库、
nlohmann），做到「克隆即可编译」。

### 5.2 修复：硬编码绝对路径（两处，都会致命）

| 位置 | 症状 |
|---|---|
| `text_preproc.h` | `sp_path_` 写死为 `C:\Users\Administrator\Desktop\VideoDemo\...\sentencepiece.bpe.model` |
| `QtFloatingBall.vcxproj` | include/lib 目录写死，**指向一个连本机都不存在的路径**——工程在该位置根本编译不过 |

**修法**：分词路径改为按 exe 目录动态解析；工程文件全部改用 `$(ProjectDir)`。

### 5.3 修复：中文路径下分词器加载失败

- **现象**：`Not found: "...\sentencepiece.bpe.model": No such file or directory`，而文件就在那里。
- **根因**：`SentencePieceProcessor::Load()` 接收**窄字符路径**，Windows 下按 ANSI 码页
  （GBK/936）解析。路径含中文就必然失败。原代码能跑，纯粹因为当年的目录是纯英文。
- **修法**：用宽字符 API 读取原始字节，再 `LoadFromSerializedProto()`。

### 5.4 修复：工程与构建配置（一串小坑）

| 问题 | 修法 |
|---|---|
| 重复的 `ProjectReference`，其中一条指向不存在的 `..\VideoFilteringCore` | 删掉错误那条 |
| `include` 指向不存在的 `sentencepiece\include` | 改为实际存在的目录 |
| 构建后未复制 `onnxruntime.dll` | 增加 post-build 复制（vcxproj 与 CMake 都配） |
| `CMakeLists.txt` 指向不存在的 `../VideoFilteringLib`，形同废纸 | 按真实布局重写 |
| MSVC 编译中文源码报 C4819 / C2001 | 为目标显式加 `/utf-8` |
| 全局定义 `NOMINMAX` 导致裸 `max()` 报 C3861 | 移除全局宏，让源文件自行 define |

### 5.5 修正：模型身份判断（我判断错了一次）

- **错误推断**：看到 `model.onnx` 有 448 MB，与英文 MiniLM-L6（约 90 MB）差距过大，
  就断定"这不是 MiniLM"。
- **实测结果**：117,505,920 参数（117.5M）/ 隐藏维 **384** / **12 层** / 词表 250,037
  → 是 **多语言 MiniLM-L12-H384**。
- **为什么体积大**：`250037 × 384` 的嵌入矩阵单独就占 96M 参数（约 384 MB），与层数无关。
- **教训**：不能用文件大小判断模型家族，要测维度、词表、参数量。

### 5.6 根治：分词表 ID 与模型 embedding 行号错位（本项目最严重的缺陷）

- **现象**：程序不报错、不崩溃、照常有输出，但语义筛选失效——相关句对与无关句对的
  平均余弦几乎相等，方向还略微相反（区分度 **−0.0149**）。
- **根因（2026-11-19 晚更正）**：本项目用的 `sentencepiece.bpe.model` 是**上游原厂文件**
  （SHA256 `CFC8146A…`，与 `sentence-transformers/paraphrase-multilingual-MiniLM-L12-v2`
  仓库发布的完全一致），并非本地转换产物。真正的问题是**该仓库同时发布的两套分词表互相矛盾**：
  `tokenizer.json` 用 `<s>=0/<pad>=1/</s>=2/<unk>=3/<mask>=250001`，
  而自带的 `sentencepiece.bpe.model` 用 `<unk>=0/<s>=1/</s>=2` 且没有 `<pad>`。
  模型权重按前者训练 —— 于是"用 HF AutoTokenizer"正常，"直接用 SentencePiece 原生库"必然错位 1 位。
  本项目属于后者。**早期"被第三方转换工具搞坏"的结论已推翻。**
- **一个反直觉的比例**：出错的是 4 个特殊词元（占词表 0.002%），被破坏的是 99.998%。
- **修法（方案二）**：不采用"代码里 +1 补偿"，而是用
  `tools/rebuild_sentencepiece_model.py` 按 `tokenizer.json` 的原始顺序重建 ModelProto，
  显式写入 `unk_id=3 / bos_id=0 / eos_id=2 / pad_id=1`，score 沿用原文件以保证切分不变。
- **验证（两级，缺一不可）**：
  1. `tools/spm_probe.cpp` 用项目实际链接的 `sentencepiece.lib` 对比新旧文件 →
     9 句 piece 字符串序列**逐字一致**，id 映射违规 **0 条**
  2. 语义区分度 **−0.0149 → +0.2894**
- 完整复盘见 [`TOKENIZER_ALIGNMENT.md`](TOKENIZER_ALIGNMENT.md)。

### 5.7 表述修正：把"自研"改成能站住的说法

| 原表述 | 改为 | 原因 |
|---|---|---|
| 自研多实例语义匹配算法 | 语义原型向量匹配（模板均值池化 + 余弦阈值） | 实际做法是均值池化 + 余弦比较，属工程方案而非算法新颖性；原说法经不起"创新点是什么"的追问 |
| （无） | 新增「实现分工与 AI 使用说明」章节 | 原 README 声称 AI 仅用于语法纠错，而代码里保留了 `// AI辅助生成：DeepSeek-R1` 注释——两处矛盾会被当场撞见 |

### 5.8 全工作区同步

| 对象 | 动作 |
|---|---|
| 5 份 `sentencepiece.bpe.model`（含参赛材料） | 全部替换为重建的正确版，逐份用探针验证 `<pad>=1` |
| 3 份源码快照的 `text_preproc.{h,cpp}` | 同步为修复版（原件已备份） |
| 打包成品 `QtFloatingBall.exe`（2026-04-25 构建） | **实测发现它根本启动不了**（仍带 5.2 的硬编码路径）→ 用修复源码重建并验证可运行 |

### 5.9 文档与 Doxygen

- 新增 `ARCHITECTURE.md`（按真实代码重写，替换原先描述不存在的 `ConsoleUI`/`WpfUI` 的旧文）、
  `TOKENIZER_ALIGNMENT.md`、`models/README.md`、`data/README.md`、本文件。
- **修复 Doxyfile**：原配置用 `EXCLUDE = ./onnx ./sentencepiece` 这样的相对路径，而 `EXCLUDE`
  是相对 doxygen 运行目录解析的，第三方库实际在 `VideoFilteringCore/` 下 → 排除规则全部失效，
  把 12,533 页第三方文档收了进来。改用 `EXCLUDE_PATTERNS` 后：

  | | 修复前 | 修复后 |
  |---|---|---|
  | 文件数 | 18,016 | **705** |
  | 体积 | 250.3 MB | **3.84 MB** |
  | 第三方页面 | 12,533 | **0** |

---

## 已知未完成项

| 项 | 现状 |
|---|---|
| 单元测试 | **0 个**；无 CI |
| attention mask | 仍与 pad id 耦合（当前 pad=1 正确，但应改为按真实长度生成以彻底解耦） |
| 筛选准确率 | 无人工标注集，无法给出 P/R 指标 |
| `VideoFilteringWidget.cpp` | 1,094 行（占自有代码 27%），未拆分 |
| `VideoDemo\src\core\app.cpp` | 仍有硬编码模型路径（旧原型，已被 Qt 版取代，未修） |
| 高价值回归测试 | 尚无"黄金样例"断言（相关对余弦均值 > 无关对）——这是防止 5.6 类问题复发的关键 |

---

## 环境与工具链

| 项 | 版本 / 位置 |
|---|---|
| 编译器 | MSVC 19.51（VS 18 BuildTools） |
| Qt | 6.11.0 msvc2022_64，`E:\Qt\6.11.0\msvc2022_64` |
| 构建 | CMake 4.3.1 / MSBuild；`"Visual Studio 18 2026"` 生成器 |
| ONNX Runtime | 1.14.1（CPU） |
| SentencePiece | 0.2.1（静态库） |
| 文档 | Doxygen 1.16.1 + Graphviz |

---

## 可迁移的经验

1. **结论的作用域不得大于证据的作用域**——一个端点失败推不出全部失败，
   一个进程挂着连接推不出它在偷流量，一个文件体积推不出模型家族。
2. **凡是「按位置编号」的数据（词表、embedding 行、数组索引、行号），"转换"前先问：
   它保留的是字节，还是语义？** 格式能互转 ≠ 编号等价。
3. **验证要用真实组件**。排查 5.6 时我先用贪婪匹配近似分词，得到 −0.1337 并据此说
   "语义系统性反转"；换成真实 `sentencepiece.lib` 后只有 −0.0149，定性应修正为"区分度失效"。
   近似品只能用来快速排除嫌疑。
4. **生成物必须可重生成，且不得比源码陈旧**。一份展示着已修复 bug 的 HTML 文档是负资产。
5. **"看起来正常"是一个待验证项**。本项目的两个最严重缺陷（5.3、5.6）都不报错。
