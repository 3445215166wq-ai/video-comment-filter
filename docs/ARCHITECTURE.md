# 架构说明

## 设计目标

1. **UI 与算法彻底解耦** —— 更换 UI 不需要碰任何算法代码
2. **核心层零 Qt 依赖** —— 核心可被任意前端（Qt / 控制台 / 服务）复用
3. **依赖方向单向无环** —— 编译期即可约束分层，而非靠约定
4. **离线可用** —— 全部推理在本地完成，不引入任何网络依赖

## 分层总览

```
┌──────────────────────────────────────────────────────────┐
│  UI 表现层                                                │
│  main_videofiltering.cpp    程序入口 / 依赖注入            │
│  VideoFilteringWidget.cpp   悬浮球窗口、面板、滑轨、日志     │
└────────────────────────┬─────────────────────────────────┘
                         │ 只认识这个纯虚接口
┌────────────────────────▼─────────────────────────────────┐
│  IApplicationController（抽象接口，无实现）                │
│  状态查询 / 模式与阈值控制 / 循环启停 / 窗口绑定            │
│  文件管理 / 事件订阅 / 全局热键                             │
└────────────────────────┬─────────────────────────────────┘
                         │ 由工厂函数返回实现
                  CreateApplicationController(exe_dir, data_dir)
                         │
┌────────────────────────▼─────────────────────────────────┐
│  核心引擎层（编译为 VideoFilteringCore.lib，无 Qt 依赖）    │
│                                                          │
│  ApplicationService      业务编排，实现上述接口              │
│   ├─ StateManager        筛选状态 + 变更订阅                │
│   ├─ ConfigurationManager 配置读写（data/config.json）      │
│   └─ FilterEngine        单个过滤周期的调度编排              │
│                                                          │
│  SimilarityPolicy        多实例语义匹配策略（纯函数）        │
│  OnnxInferencer          ONNX Runtime 会话 + 批量推理       │
│  TextPreprocessor        SentencePiece 分词                │
│  clean/                  评论文本清洗与解析                 │
│  utils/                  剪贴板 / 文件 / 键盘输入 / 日志 / 数学│
└────────────────────────┬─────────────────────────────────┘
                         │
              ONNX Runtime · SentencePiece · nlohmann_json
```

## 模块职责

| 模块 | 位置 | 职责 |
|---|---|---|
| `ApplicationService` | `application/` | 实现 `IApplicationController`；管理过滤线程生命周期；把 UI 操作翻译成对引擎的调用；向上转发状态与日志事件 |
| `StateManager` | `application/` | 持有 `FilterState`，提供变更订阅（观察者模式），状态一改就通知 UI 并落盘 |
| `ConfigurationManager` | `application/` | `data/config.json` 的读写与容错（配置损坏时回退默认值） |
| `FilterEngine` | `application/` | 单次过滤周期的调度：抓取 → 清洗 → 解析 → 编码 → 相似度 → 动作 |
| `SimilarityPolicy` | `core/` | 纯逻辑：按模式与阈值评估正/负模板相似度，产出 `filter_result`（0=停止，1=滚动） |
| `OnnxInferencer` | `model/` | ONNX Runtime 会话管理、单条/批量推理、CPU 线程数配置、图优化 |
| `TextPreprocessor` | `model/` | SentencePiece 分词，输出 `input_ids` / `attention_mask`（长度 128） |
| `clean/` | `clean/` | 评论原文清洗与结构化解析 |
| `utils/` | `utils/` | 剪贴板、文件、模拟键盘（滚动动作）、日志、余弦相似度 |

## 接口契约

`IApplicationController` 按职责分七组方法：

| 分组 | 方法 |
|---|---|
| 状态查询 | `GetCurrentState()` |
| 模式控制 | `SetFilterMode(int)` |
| 阈值控制 | `SetPositiveThreshold` / `SetNegativeThreshold` / `AdjustPositiveThreshold` / `AdjustNegativeThreshold` |
| 循环控制 | `StartFilteringLoop` / `StopFilteringLoop` |
| 窗口绑定 | `SetTargetWindow(title_keyword)` |
| 文件管理 | `OpenTemplateFolder` / `OpenCommentDataFolder` / `OpenSimilarityCheckFolder` |
| 事件订阅 | `SubscribeStateChange` / `SubscribeMessage` |
| 热键与重置 | `RegisterGlobalHotKey(HWND)` / `UnregisterGlobalHotKey` / `ResetToDefaultSettings` |

UI 层**只**通过这个接口工作，看不到 `FilterEngine`、`OnnxInferencer` 等任何具体类型。入口处由工厂函数完成注入：

```cpp
// main_videofiltering.cpp
std::wstring exeDir  = QCoreApplication::applicationDirPath().toStdWString();
std::wstring dataDir = exeDir + L"/data";

auto ctrl = CreateApplicationController(exeDir, dataDir);   // 只返回接口指针
std::shared_ptr<IApplicationController> controller(std::move(ctrl));
VideoFilteringWidget widget(controller);                    // UI 依赖注入
```

## 筛选执行流程

`FilterEngine::ExecuteFilterCycle()` 是单次周期的主干：

```
① 抓取    绑定目标窗口，取评论（utils/ 剪贴板与键盘自动化）
② 清洗    clean/ 去除噪声、切分条目
③ 解析    结构化评论对象；record_data 为真时落盘到 data/raw、data/cleaned、data/parser
④ 编码    TextPreprocessor 分词 → OnnxInferencer 批量推理（一次 batch，非逐条）
⑤ 匹配    SimilarityPolicy：对每个模板组求均值嵌入，算余弦相似度
⑥ 动作    decision = 0 停止 / 1 滚动下一个
```

第 ④ 步是性能关键：评论按批送入同一次 `Run()`，而非逐条推理。

## 决策矩阵

`SimilarityPolicy::Evaluate()` 按"负向优先"的顺序裁决：

| 启用模式 | 负向命中 | 正向命中 | 结果 |
|---|---|---|---|
| 仅负向（`mode & 2`） | 是 | — | **0 停止** |
| 仅负向（`mode & 2`） | 否 | — | 1 滚动 |
| 双向（`mode == 3`） | 是 | 不检测 | **0 停止**（短路，省一次正向计算） |
| 双向（`mode == 3`） | 否 | 是 | **0 停止** |
| 双向（`mode == 3`） | 否 | 否 | 1 滚动 |
| 仅正向（`mode & 1`） | 不检测 | 是 | **0 停止** |
| 仅正向（`mode & 1`） | 不检测 | 否 | 1 滚动 |
| 未启用任何模式 | — | — | 0 停止 |

"多实例"体现在第 ⑤ 步：一个模板 JSON 内含**一组**示例评论，先各自编码再取均值，得到一个语义原型向量，评论只需与它算一次余弦相似度。

## 线程模型

- **UI 线程**：Qt 事件循环，响应悬浮球交互与全局热键
- **过滤线程**：`StartFilteringLoop()` 启动，按周期重复 `ExecuteFilterCycle()`
- **推理互斥**：`OnnxInferencer` 内部 `std::mutex` 保护会话，批量与单条推理不会并发进入同一个 `OrtSession`

状态变更通过 `StateManager` 的订阅回调抛回 UI 线程，UI 不与引擎共享可变状态。

## 构建产物关系

```
VideoFilteringCore (STATIC)  ──►  QtFloatingBall.exe
        │                              │
        │ 链接                          │ 链接
        ├─ sentencepiece.lib           ├─ Qt6::Core / Gui / Widgets
        └─ onnxruntime.lib             └─ onnxruntime.lib
                     │
                     └─ 构建后自动复制 onnxruntime.dll 到输出目录
```

工程文件与 `CMakeLists.txt` 描述同一套目标关系，二选一即可。

## 扩展点

| 想做的事 | 改哪里 |
|---|---|
| 换 UI（Web / CLI / 托盘） | 新写一个前端，复用 `IApplicationController`，核心层不动 |
| 换语义模型 | 替换 `models/` 下两个文件；张量名与 128 长度需匹配（见 `models/README.md`） |
| 加筛选维度 | 在 `SimilarityPolicy` 增加模板分组与阈值字段 |
| 换抓取方式 | 只改 ① 步对应的 `utils/` 实现 |

## 相关文档

- [`models/README.md`](../models/README.md) —— 模型张量接口与获取方式
- [`data/README.md`](../data/README.md) —— 配置字段与模板格式
- 各模块内 `model.md` / `clean.md` / `utils.md` —— 模块级细节
