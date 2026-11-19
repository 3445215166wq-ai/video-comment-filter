# 评论文本向量化模块使用说明
## 模块概述
该模块实现从JSON文件加载评论数据，并通过ONNX模型+SentencePiece分词器将评论文本转换为固定维度的浮点型向量（Embedding），适用于中文评论文本的向量化处理（如相似度计算、情感分析等下游NLP任务）。
输出向量格式
类型：std::vector<float>
长度：固定维度（由 ONNX 模型输出层决定，常见 768/512/1024）
内容：模型输出的原始浮点型嵌入向量（Embedding）
对应关系：输入文本顺序 = 输出向量顺序，一一对应
用途：可直接用于相似度计算、聚类、检索、分类等下游任务

## 模块明细

### 1. 评论数据加载模块（comment_loader）
| 项               | 内容                                                                 |
|------------------|----------------------------------------------------------------------|
| 模块名           | comment_loader                                                       |
| 文件路径         | src/model/comment_loader.h、src/model/comment_loader.cpp             |
| 公开函数签名     | `std::vector<Comment> load_comments_from_json(const std::string& file_path);` |
| 重要语义说明     | - 异常：解析JSON行失败时会捕获异常并输出警告，**不抛出异常**，仅跳过错误行<br>- 线程安全：**线程安全**（只读操作，无共享状态）<br>- 前置依赖：无（无需调用init/load_model）<br>- 字符串编码：内部将JSON的string转为wstring（Windows宽字符） |
| 输入示例         | 输入JSON文件路径：`"D:/data/comments.json"`，JSON文件每行格式：<br>`{"username":"自律的敏","content":"加油","time":"1月前","location":"广东","likes":0,"reply_to":null}` |
| 输出示例         | 返回`std::vector<Comment>`，其中一个Comment元素：<br>- username: L"自律的敏"<br>- content: L"加油"<br>- time: "1月前"<br>- location: "广东"<br>- likes: 0<br>- reply_to: "无"<br>- embedding: 空向量（未编码）<br>- encode_success: false |

### 2. 文本预处理模块（text_preproc）
| 项               | 内容                                                                 |
|------------------|----------------------------------------------------------------------|
| 模块名           | text_preproc                                                         |
| 文件路径         | src/model/text_preproc.h、src/model/text_preproc.cpp                 |
| 公开函数签名     | 类成员函数（需实例化`TextPreprocessor`调用）：<br>`bool init();`<br>`void tokenize(const std::wstring& text, std::vector<int64_t>& ids, std::vector<int64_t>& mask);`<br>`void uninit();` |
| 重要语义说明     | - 异常：加载分词模型失败时输出错误日志，**不抛出异常**<br>- 线程安全：`init/uninit`非线程安全，`tokenize`**线程安全**（无共享状态修改）<br>- 前置依赖：调用`tokenize`前**必须先调用init**（加载sentencepiece.bpe.model）<br>- 路径依赖：分词模型固定读取`x64/Release/models/sentencepiece.bpe.model`<br>- 内置能力：包含wstring转UTF8编码逻辑，无需依赖外部工具 |
| 输入示例         | `TextPreprocessor preproc;`<br>`preproc.init();`<br>`std::wstring text = L"加油";`<br>`std::vector<int64_t> ids, mask;`<br>`preproc.tokenize(text, ids, mask);` |
| 输出示例         | - ids（长度128）：`[0, 123, 456, 2, 1, 1, ..., 1]`（0=CLS，2=SEP，1=PAD）<br>- mask（长度128）：`[1, 1, 1, 1, 0, 0, ..., 0]`（1=有效Token，0=PAD） |

### 3. ONNX模型推理模块（onnx_infer）
| 项               | 内容                                                                 |
|------------------|----------------------------------------------------------------------|
| 模块名           | onnx_infer                                                           |
| 文件路径         | src/model/onnx_infer.h、src/model/onnx_infer.cpp                     |
| 公开函数签名     | 全局接口（推荐使用，无需手动实例化）：<br>`bool load_onnx_model(const wchar_t* onnx_path);`<br>`bool encode_text(const std::wstring& text, std::vector<float>& emb);`<br>`void unload_onnx_model();`<br>`bool encode_text_batch(const std::vector<std::wstring>& texts, std::vector<std::vector<float>>& embs);` |
| 重要语义说明     | - 异常：模型加载/推理失败时输出错误日志，**不抛出异常**<br>- 线程安全：所有函数**线程安全**（内部通过`std::mutex`加锁）<br>- 前置依赖：调用`encode_text/encode_text_batch`前**必须先调用load_onnx_model**（加载onnx模型+初始化分词模块）<br>- 路径依赖：推荐传入ONNX模型路径`L"x64/Release/models/model.onnx"`<br>- 内置依赖：内部封装`TextPreprocessor`，无需单独调用文本预处理 |
| 输入示例         | 单文本编码：<br>`load_onnx_model(L"x64/Release/models/model.onnx");`<br>`std::wstring text = L"加油";`<br>`std::vector<float> emb;`<br>`encode_text(text, emb);` |
| 输出示例         | - encode_text返回值：true（成功）/false（失败）<br>- emb（768维示例）：`[0.12f, -0.34f, 0.56f, ..., 1.23f]`（浮点型向量，维度由ONNX模型决定）<br>- 批量编码：`encode_text_batch`返回`std::vector<std::vector<float>>`，顺序与输入texts完全一致 |

## 完整使用流程示例
```cpp
#include "src/model/comment_loader.h"
#include "src/model/onnx_infer.h"
#include <iostream>

int main() {
    // 1. 加载ONNX模型（自动初始化分词模块）
    bool model_loaded = load_onnx_model(L"x64/Release/models/model.onnx");
    if (!model_loaded) {
        std::cerr << "模型加载失败" << std::endl;
        return -1;
    }

    // 2. 加载JSON评论数据
    std::vector<Comment> comments = load_comments_from_json("D:/data/comments.json");
    if (comments.empty()) {
        std::cerr << "评论加载失败" << std::endl;
        unload_onnx_model();
        return -1;
    }

    // 3. 批量编码评论文本
    for (auto& c : comments) {
        c.encode_success = encode_text(c.content, c.embedding);
        std::cout << "用户名：" << std::string(c.username.begin(), c.username.end())
                  << " 编码状态：" << (c.encode_success ? "成功" : "失败")
                  << " 向量维度：" << c.embedding.size() << std::endl;
    }

    // 4. 释放模型资源
    unload_onnx_model();
    return 0;
}
```

## 环境依赖说明
1. 模型文件路径：确保`x64/Release/models/`下存在`model.onnx`和`sentencepiece.bpe.model`；
2. 编译依赖：链接SentencePiece库、ONNX Runtime库；
3. 系统环境：Windows（代码中使用Windows API处理宽字符，需适配其他系统需修改text_preproc中的编码转换逻辑）。

### 总结
- 核心调用流程：`load_onnx_model`（加载模型）→ `load_comments_from_json`（加载评论）→ `encode_text/encode_text_batch`（文本编码）→ `unload_onnx_model`（释放资源）；
- 三个模块职责独立且内聚：评论加载仅管数据读取、文本预处理仅管分词、ONNX推理仅管向量化，且编码转换逻辑已内嵌在预处理模块中；
- 所有公开函数均不抛出异常，通过返回值/日志反馈错误，批量编码时评论与向量的顺序完全一致。