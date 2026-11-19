# 基于 MiniLM 的短视频个性化智能筛选系统

一款面向短视频运营场景的 Windows 桌面端 AI 助手：悬浮球一键启动，自动抓取视频评论，用**离线语义模型**按你的意图精准筛选，把人工数小时的工作压缩到分钟级。

全程本地推理，**无任何网络请求，数据不出设备**。

> **模型文件不随仓库分发**（约 453MB，超出 GitHub 单文件 100MB 限制）。
> 请从 [Releases](https://github.com/3445215166wq-ai/video-comment-filter/releases) 下载 `model.onnx` 与 `sentencepiece.bpe.model`，放入 `models/` 目录。详见 [`models/README.md`](models/README.md)。

---

## 目录

- [核心功能](#核心功能)
- [系统架构](#系统架构)
- [技术栈](#技术栈)
- [目录结构](#目录结构)
- [构建](#构建)
- [运行准备](#运行准备)
- [技术亮点](#技术亮点)
- [文档](#文档)

---

## 核心功能

1. **评论一键抓取** —— 绑定目标窗口，自动抓取并解析短视频评论区
2. **语义级筛选** —— 不是关键词匹配，而是理解"这条评论在说什么"
3. **阈值可视化调节** —— 滑轨实时调整正/负向筛选尺度，黑盒 AI 变可控决策
4. **悬浮球交互** —— 拖拽、悬停展开面板、`F8` 全局热键，不遮挡视频画面
5. **完全离线** —— ONNX Runtime 本地推理，无联网、无云端调用

### 筛选决策逻辑

负向模板优先级最高，命中即停止（`FilterEngine` / `SimilarityPolicy::Evaluate`）：

```
                 ┌─ 负向模板命中 ─────────────────────────────→ 停止（过滤掉）
评论 ─→ 编码 ─→ ┤
                 └─ 负向未命中 ─→ 正向模板命中? ─ 是 ─→ 停止（过滤掉）
                                                └ 否 ─→ 滚动（保留）
```

支持三种模式（位掩码，见 `data/config.json` 的 `mode`）：仅正向 `1`、仅负向 `2`、双向 `3`。

---

## 系统架构

采用**严格分层 + 静态库解耦**，UI 与核心算法物理隔离：

```
┌──────────────────────────────────────┐
│  UI 表现层（Qt 悬浮球）               │  不包含任何算法/推理逻辑
│  main_videofiltering.cpp             │
│  VideoFilteringWidget.cpp/h          │
└──────────────────┬───────────────────┘
                   │ 仅依赖抽象接口
┌──────────────────▼───────────────────┐
│  IApplicationController（纯虚接口）    │
└──────────────────┬───────────────────┘
                   │ 实现
┌──────────────────▼───────────────────┐
│  核心引擎层 VideoFilteringCore.lib    │  独立静态库，无 Qt 依赖
│  ├─ ApplicationService（业务编排）     │
│  ├─ StateManager      （状态）         │
│  ├─ ConfigurationManager（配置）       │
│  ├─ FilterEngine      （筛选调度）     │
│  ├─ SimilarityPolicy  （多实例匹配）   │
│  └─ OnnxInferencer    （语义推理）     │
└──────────────────┬───────────────────┘
                   │
        ONNX Runtime · SentencePiece · nlohmann_json
```

依赖方向单向无环：`UI → 接口 → 核心库 → 第三方静态库`。

核心算法被编译为静态库 `VideoFilteringCore.lib`，UI 层**看不到**任何推理细节，只通过 `IApplicationController` 调用。

详见 [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md)：模块职责、接口契约、筛选执行流程、决策矩阵与线程模型。

---

## 技术栈

| 项 | 选型 |
|---|---|
| 语言 | C++17 |
| UI | Qt 6.11（Core / Gui / Widgets） |
| 推理引擎 | ONNX Runtime 1.14.1（CPU，本地离线） |
| 语义模型 | MiniLM / BERT 结构，SentencePiece 分词 |
| 其他依赖 | SentencePiece 0.2.1、nlohmann_json |
| 平台 | Windows x64 |
| 构建 | Visual Studio 2022/2026（.vcxproj/.sln）或 CMake |

---

## 目录结构

```
.
├── CMakeLists.txt                  # CMake 构建脚本（等价于 .sln）
├── QtFloatingBall.sln              # Visual Studio 解决方案（主构建入口）
├── QtFloatingBall.vcxproj          # UI 表现层工程
├── main_videofiltering.cpp         # 程序入口
├── VideoFilteringWidget.cpp/.h     # 悬浮球主窗口
│
├── VideoFilteringCore/             # 核心引擎层（静态库）
│   ├── VideoFilteringCore.vcxproj
│   ├── application/                # 接口 + 业务编排 + 配置 + 状态 + 调度
│   ├── core/                       # 多实例语义匹配策略
│   ├── clean/                      # 评论文本清洗与解析
│   ├── model/                      # ONNX 推理 + SentencePiece 分词
│   ├── utils/                      # 剪贴板/文件/输入/日志/数学工具
│   ├── include/                    # 第三方头文件 + sentencepiece.lib
│   ├── nlohmann/                   # json.hpp
│   └── onnx/                       # ONNX Runtime SDK（头文件 + 导入库 + DLL）
│
├── data/                           # 配置与语义模板（含示例）
│   ├── config.json
│   ├── positive_templates/
│   └── negative_templates/
│
├── models/                         # 模型目录（文件需自行获取）
│   └── README.md
│
└── docs/                           # 架构与类图文档
```

---

## 构建

### 环境要求

- Windows 10/11 x64
- Visual Studio 2022 或更高（含 C++ 桌面开发工作负载）
- Qt 6.11 MSVC 2022 64-bit

### 方式一：Visual Studio（推荐）

1. 用 VS 打开 `QtFloatingBall.sln`
2. 选择 `Release | x64`
3. 生成解决方案

工程内所有路径均使用 `$(ProjectDir)` 相对路径，克隆到任意目录均可编译。第三方库（ONNX Runtime、SentencePiece）随仓库提供，**无需额外安装**。

> 需要安装 VS 的 Qt 扩展（Qt VS Tools），因为 `VideoFilteringWidget.h` 依赖 `moc` 生成。若未安装，请改用方式二。

### 方式二：CMake

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH="C:/Qt/6.11.0/msvc2022_64"
cmake --build build --config Release
```

CMake 使用 `AUTOMOC` 自动处理 Qt 元对象，不依赖 VS 扩展。

---

## 运行准备

1. 编译产物位于 `x64\Release\`（或 `build\Release\`）
2. 按 [`models/README.md`](models/README.md) 获取两个模型文件
3. 把 `models/` 和 `data/` 目录复制到可执行文件同级目录
4. 若在未安装 Qt 环境的机器上运行，用 `windeployqt` 补齐 Qt 运行时：

   ```bash
   C:\Qt\6.11.0\msvc2022_64\bin\windeployqt.exe QtFloatingBall.exe
   ```

5. 双击运行，按 `F8` 唤出悬浮球

---

## 技术亮点

**1. 自研多实例语义匹配算法**

借鉴多实例学习（MIL）思路：把一组正/负例评论抽象为一个"语义原型向量"，筛选时只需算一次余弦相似度即可判定。相比逐条比对，把单次筛选压到百毫秒级。

**2. 全离线本地推理**

ONNX Runtime 直接加载本地模型，全程无网络调用。相比 Python 原生推理，首次加载速度提升约 8 倍，内存占用降至约四分之一。

**3. UI 与内核物理隔离**

核心算法编译为无 Qt 依赖的静态库，UI 层只透过 `IApplicationController` 单向调用。可移植、可测试、可替换 UI。

**4. 人机协同透明决策**

阈值滑轨实时生效，配置自动持久化（`data/config.json`），AI 判定不再黑盒。

**5. 无干扰轻量化交互**

悬浮球分层设计（一级快捷面板 / 二级完整设置），双窗口绑定，最大程度减少对视频画面的遮挡。

---

## 文档

| 文档 | 内容 |
|---|---|
| [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) | 分层架构、模块职责、接口契约、执行流程、决策矩阵 |
| [`models/README.md`](models/README.md) | 模型获取与张量接口要求 |
| [`data/README.md`](data/README.md) | 配置字段与模板格式 |

---

## 许可

MIT License，详见 [`LICENSE`](LICENSE)。

第三方组件遵循各自许可：ONNX Runtime（MIT）、SentencePiece（Apache-2.0）、nlohmann_json（MIT）、Qt（LGPL-3.0）。
