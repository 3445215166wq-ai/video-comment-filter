<!--
  本文件于 2026-11-21 从 NTFS MFT 恢复。
  原文件于 2026-11-20 01:36:43 在项目目录整合时被误删（USN 日志可证）。
  恢复方法：直接解析 \ 记录、读取 \ 属性；内容是逐字节原文，未作任何编辑。
-->
# 抖音评论处理模块接口文档
> 项目路径：`VideoFiltering/src/clean/`
> 文档版本：v1.0
> 职责分工：**cleaners（清洗原始文本） + parser（结构化解析生成JSON）**

---

## 📁 项目文件清单
| 文件名 | 模块职责 | 文件完整相对路径 |
|---|---|---|
| `cleaners.h` | 清洗模块头文件 | `src/clean/cleaners.h` |
| `cleaners.cpp` | 清洗模块实现 | `src/clean/cleaners.cpp` |
| `parser.h` | 解析模块头文件 | `src/clean/parser.h` |
| `parser.cpp` | 解析模块实现 | `src/clean/parser.cpp` |

---

## 🧹 模块一：cleaners 文本清洗模块
### 模块概述
负责抖音原始爬取文本的预处理，完成区间截取、垃圾内容过滤，输出干净可解析的纯评论文本，实现 `原始文本 → 清洗后文本`（1.txt → 2.txt）

### 公开函数签名
```cpp
// cleaners.h
namespace clean {

/**
 * @brief 清洗抖音评论原始网页文本
 * @param raw_text 输入：完整原始宽字符网页文本
 * @return std::wstring 输出：过滤完成的纯净评论文本
 */
std::wstring clean_douyin_comment_raw(const std::wstring& raw_text);

}
```

### 核心语义说明
1.  **功能逻辑**
    - 自动截取：仅保留「全部评论」起始，到「留下你的精彩评论吧」结束的有效区间
    - 自动删除：读屏提示、版权声明、ICP备案、许可证、举报/协议、分享、回复、展开回复、加载占位文本
    - 保留全部有效评论、用户名、时间、地点、点赞数字段
2.  **异常安全**
    - 不会主动抛出任何C++异常，边界错误、匹配失败全部内部兜底兼容
3.  **线程安全**
    - ✅ 完全线程安全，无全局静态变量、无共享可变状态，可多线程并发调用
4.  **初始化要求**
    - ❌ 无需提前init、无需加载模型、无需全局配置，开箱即用
5.  **外部依赖**
    - 标准库：`<string>` `<vector>` `<regex>`
    - 无第三方强依赖

### 输入/输出示例
#### 原始输入（片段）
```text
开启读屏标签读屏标签已关闭
下载抖音精选
2026 © 抖音 京ICP备16016397号-3
全部评论(648)
木木禾~头像
木木禾~
好讨厌这个排列组合[黑脸]
2周前·山东
2035
分享
回复
展开10条回复
留下你的精彩评论吧
```

#### 清洗后输出
```text
全部评论(648)
木木禾~头像
木木禾~
好讨厌这个排列组合[黑脸]
2周前·山东
2035
```

---

## 🔍 模块二：parser 评论结构化解析模块
### 模块概述
接收清洗完成的纯评论文本，拆分单条评论、提取全部元数据，最终生成标准规范JSON数组，实现 `清洗文本 → 结构化JSON`（2.txt → 3.json）

### 数据结构体定义
```cpp
// parser.h
namespace parse {

struct DouyinComment {
    std::string username;   // 评论用户名
    std::string content;    // 评论正文内容
    std::string time;       // 发布时间（如：2周前）
    std::string location;    // 发布地点（如：山东）
    int likes = 0;          // 点赞数量
    std::nullptr_t reply_to = nullptr; // 回复目标，固定null

    /**
     * @brief 将单条评论转为标准JSON对象
     * @return nlohmann::json 格式化JSON
     */
    nlohmann::json to_json() const;
};

}
```

### 公开函数签名
```cpp
// parser.h
namespace parse {

/**
 * @brief 批量解析清洗文本，生成完整评论JSON数组
 * @param cleaned_text 输入：cleaners模块输出的清洗完成文本
 * @return std::string 输出：格式化缩进的完整JSON数组字符串
 */
std::string comment_text_to_json(const std::wstring& cleaned_text);

}
```

### 核心语义说明
1.  **字段提取规则**
    - 用户名：自动识别「XXX头像」主评论、无头像楼中楼回复用户名
    - 时间：匹配含「前」的时间行
    - 地点：时间行「·」分隔符后半段文本
    - 点赞：识别纯数字独立行
    - 正文：用户名与时间之间的全部内容
2.  **异常安全**
    - 数字解析失败自动兜底赋值0
    - 字段缺失自动填充空字符串/null，解析中断不崩溃
3.  **线程安全**
    - ✅ 完全线程安全，无全局状态
4.  **初始化要求**
    - ❌ 无需任何前置初始化、资源加载
5.  **外部依赖**
    - `nlohmann/json`（已内置在项目`nlohmann`文件夹）
    - Windows宽字符转码API

### 输入/输出示例
#### 清洗文本输入
```text
木木禾~头像
木木禾~
好讨厌这个排列组合[黑脸]
2周前·山东
2035
```

#### JSON标准输出
```json
[
    {
        "username": "木木禾~",
        "content": "好讨厌这个排列组合[黑脸]",
        "time": "2周前",
        "location": "山东",
        "likes": 2035,
        "reply_to": null
    }
]
```

---

## 🚀 主程序标准调用示例
```cpp
#include "clean/cleaners.h"
#include "clean/parser.h"
#include <fstream>
#include <iostream>

int main()
{
    // 1. 读取原始评论文本
    std::wifstream raw_file("1.txt");
    std::wstring raw_text(
        std::istreambuf_iterator<wchar_t>(raw_file),
        std::istreambuf_iterator<wchar_t>()
    );

    // 2. 第一步：执行文本清洗
    std::wstring cleaned = clean::clean_douyin_comment_raw(raw_text);

    // 3. 保存清洗后的中间文本 2.txt
    std::wofstream cleaned_file("2.txt");
    cleaned_file << cleaned;

    // 4. 第二步：解析生成最终JSON
    std::string json_result = parse::comment_text_to_json(cleaned);

    // 5. 输出最终结果 3.json
    std::ofstream json_file("3.json");
    json_file << json_result;

    std::cout << "评论处理全部完成！" << std::endl;
    return 0;
}
```

---
