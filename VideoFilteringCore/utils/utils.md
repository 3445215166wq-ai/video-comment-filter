---

**Utils_API.md**
```markdown
# Utils 通用工具模块
- **模块名称**：Utils 基础工具库
- **文件存放路径**：`src/utils/`
- **入口头文件**：`src/utils/utils.h`
- **内部依赖文件**：
  - `src/utils/utils.cpp`
  - `src/utils/utils_clipboard.cpp`
  - `src/utils/utils_file.cpp`
  - `src/utils/utils_input.cpp`
  - `src/utils/utils_log.cpp`
  - `src/utils/utils_math.cpp`
  - `src/utils/utils_internal.h`（内部私有头文件，外部无需引入）
- **平台**：Windows 仅
- **命名空间**：`Utils`
- **初始化**：无 Init / 无 LoadModel / 开箱即用

# Utils 工具库 API 文档

```
版本：v1.1
平台：Windows 仅
命名空间：Utils
依赖库：user32.lib、kernel32.lib、shell32.lib、shlwapi.lib
初始化：无 Init / 无 LoadModel / 无对象 / 直接调用
异常：所有函数均不抛 C++ 异常
补充：多线程场景下，日志、剪贴板、键鼠函数调用，必须手动加互斥锁做串行保护，避免行为错乱
```

---

## 目录
1. [模块总览](#1-模块总览)
2. [全局规则](#2-全局规则)
3. [日志 API](#3-日志-api)
4. [剪贴板 API](#4-剪贴板-api)
5. [文件/目录 API](#5-文件目录-api)
6. [鼠标键盘模拟 API](#6-鼠标键盘模拟-api)
7. [数学计算 API](#7-数学计算-api)
8. [完整使用示例](#8-完整使用示例)

---

## 1. 模块总览
Utils 是 Windows 平台轻量级工具集，提供**日志输出、剪贴板读写、文件/文件夹扫描、JSON模板加载、键鼠模拟、向量相似度**等常用封装，无类、无状态、开箱即用。

- 所有函数放在 `Utils` 命名空间
- 头文件只需包含 `#include "utils.h"`
- 无初始化、无释放、无依赖顺序
- 失败以返回值/空字符串/空列表表示，不崩溃

---

## 2. 全局规则
### 2.1 线程安全
- **线程安全**：文件/目录、数学计算
- **非线程安全**：日志、剪贴板、键鼠模拟（全局系统资源）
- 非安全函数需**外部加锁**或**串行调用**

### 2.2 异常保证
- **全部函数不抛 C++ 异常**
- 文件不存在、打开失败、剪贴板失败等均返回空/ false，不崩溃

### 2.3 调用要求
- 无需 Init/Load/Create
- 无需清理/Close/Release
- 直接调用即可

---

## 3. 日志 API
### 3.1 `void Utils::DebugLog(const char* fmt, ...)`
- **签名**：`void Utils::DebugLog(const char* fmt, ...)`
- **功能**：输出 UTF-8/ANSI 日志到调试器 + 写入 `videodemo_debug.txt`
- **线程安全**：否
- **抛异常**：否
- **初始化**：否
- **示例**：
```cpp
Utils::DebugLog("程序启动，当前计数：%d", 10);
```

### 3.2 `void Utils::DebugLogW(const wchar_t* fmt, ...)`
- **签名**：`void Utils::DebugLogW(const wchar_t* fmt, ...)`
- **功能**：宽字符日志输出
- **线程安全**：否
- **抛异常**：否
- **示例**：
```cpp
Utils::DebugLogW(L"剪贴板内容：%s", my_text.c_str());
```

---

## 4. 剪贴板 API
### 4.1 `bool Utils::ReadTextFromClipboard(std::wstring& outText)`
- **签名**：`bool Utils::ReadTextFromClipboard(std::wstring& outText)`
- **功能**：读取剪贴板文本，支持：Unicode、ANSI、UTF-8、HTML、文件路径
- **线程安全**：否（全局剪贴板）
- **抛异常**：否
- **返回**：true=成功读到内容
- **示例**：
```cpp
std::wstring text;
bool ok = Utils::ReadTextFromClipboard(text);
if (ok) {
    Utils::DebugLogW(L"内容：%s", text.c_str());
}
```

### 4.2 `void Utils::ResetClipboard()`
- **签名**：`void Utils::ResetClipboard()`
- **功能**：清空剪贴板
- **线程安全**：否
- **抛异常**：否
- **示例**：
```cpp
Utils::ResetClipboard();
```

---

## 5. 文件/目录 API
### 5.1 `std::wstring Utils::GetAppDir()`
- **签名**：`std::wstring Utils::GetAppDir()`
- **功能**：获取当前 exe 所在目录
- **线程安全**：是
- **抛异常**：否
- **示例**：
```cpp
std::wstring dir = Utils::GetAppDir();
```

### 5.2 `std::wstring Utils::GetPositiveTemplateFolder()`
- **签名**：`std::wstring Utils::GetPositiveTemplateFolder()`
- **功能**：获取正向模板目录 `data/positive_templates/`
- **线程安全**：是
- **抛异常**：否
- **示例**：
```cpp
auto folder = Utils::GetPositiveTemplateFolder();
```

### 5.3 `std::wstring Utils::GetNegativeTemplateFolder()`
- **签名**：`std::wstring Utils::GetNegativeTemplateFolder()`
- **功能**：获取负向模板目录 `data/negative_templates/`
- **线程安全**：是
- **抛异常**：否
- **示例**：
```cpp
auto folder = Utils::GetNegativeTemplateFolder();
```

### 5.4 `std::vector<std::wstring> Utils::GetAllJsonFilesInFolder(const std::wstring& folderPath)`
- **签名**：`std::vector<std::wstring> Utils::GetAllJsonFilesInFolder(const std::wstring& folderPath)`
- **功能**：扫描指定文件夹，返回所有 .json 文件名列表
- **线程安全**：是
- **抛异常**：否
- **示例**：
```cpp
auto files = Utils::GetAllJsonFilesInFolder(folder);
```

### 5.5 `std::wstring Utils::ReadUtf8File(const std::wstring& filePath)`
- **签名**：`std::wstring Utils::ReadUtf8File(const std::wstring& filePath)`
- **功能**：读取任意 UTF-8 编码文件（JSON/TXT 通用）
- **线程安全**：是
- **抛异常**：否
- **示例**：
```cpp
auto content = Utils::ReadUtf8File(L"test.json");
```

### 5.6 `std::wstring Utils::ReadPositiveTemplate(const std::wstring& fileName)`
- **签名**：`std::wstring Utils::ReadPositiveTemplate(const std::wstring& fileName)`
- **功能**：读取正向模板目录下指定 JSON 文件
- **线程安全**：是
- **抛异常**：否
- **示例**：
```cpp
auto json = Utils::ReadPositiveTemplate(L"template1.json");
```

### 5.7 `std::wstring Utils::ReadNegativeTemplate(const std::wstring& fileName)`
- **签名**：`std::wstring Utils::ReadNegativeTemplate(const std::wstring& fileName)`
- **功能**：读取负向模板目录下指定 JSON 文件
- **线程安全**：是
- **抛异常**：否
- **示例**：
```cpp
auto json = Utils::ReadNegativeTemplate(L"template2.json");
```

---

## 6. 鼠标键盘模拟 API
所有键鼠函数**非线程安全**、**不抛异常**、**无需初始化**。

### 6.1 快捷键
- `void Utils::CtrlA_Only()`：模拟 Ctrl+A 全选
- `void Utils::CtrlC_Only()`：模拟 Ctrl+C 复制
- `void Utils::Press_C_Only()`：单击 C 键
- `void Utils::SendEscapeKey()`：单击 ESC 键

### 6.2 鼠标操作
- `void Utils::LeftClickMouse()`：左键单击
- `void Utils::RightClickPage()`：右键单击
- `void Utils::MouseScrollDown()`：滚轮向下
- `void Utils::MoveMouseToWindowCenter(HWND hWnd)`：移到窗口中心
- `void Utils::MoveMouseToWindowRight(HWND hWnd)`：移到窗口右侧
- `void Utils::MoveMouseToWindowRightTopThird(HWND hWnd)`：移到窗口右上 1/3

### 6.3 按键检测
- `bool Utils::IsKeyPress_P()`：检测 P 键是否按下

### 示例
```cpp
HWND hWnd = GetForegroundWindow();
Utils::MoveMouseToWindowCenter(hWnd);
Utils::LeftClickMouse();
Utils::CtrlA_Only();
Utils::CtrlC_Only();
```

---

## 7. 数学计算 API
### 7.1 `float Utils::calc_similarity(const std::vector<float>& vec1, const std::vector<float>& vec2)`
- **签名**：`float Utils::calc_similarity(const std::vector<float>& vec1, const std::vector<float>& vec2)`
- **功能**：计算两向量余弦相似度
- **线程安全**：是
- **抛异常**：否
- **返回**：-1.0 ~ 1.0
- **示例**：
```cpp
std::vector<float> a = {1,2,3};
std::vector<float> b = {1,2,3};
float sim = Utils::calc_similarity(a, b);
```

---

## 8. 完整使用示例
```cpp
#include "src/utils/utils.h"
#include <windows.h>

int main()
{
    // 日志
    Utils::DebugLog("=== 开始运行 ===");

    // 获取程序目录
    std::wstring dir = Utils::GetAppDir();
    Utils::DebugLogW(L"程序目录：%s", dir.c_str());

    // 获取正负模板目录
    auto posFolder = Utils::GetPositiveTemplateFolder();
    auto negFolder = Utils::GetNegativeTemplateFolder();
    Utils::DebugLogW(L"正向模板目录：%s", posFolder.c_str());

    // 获取所有 JSON 文件
    auto jsonFiles = Utils::GetAllJsonFilesInFolder(posFolder);
    Utils::DebugLog(L"正向模板数量：%d", (int)jsonFiles.size());

    // 读取一个 JSON
    if (!jsonFiles.empty()) {
        auto content = Utils::ReadPositiveTemplate(jsonFiles[0]);
        Utils::DebugLogW(L"读取内容长度：%d", (int)content.size());
    }

    // 键鼠模拟
    HWND hWnd = GetDesktopWindow();
    Utils::MoveMouseToWindowCenter(hWnd);
    Utils::LeftClickMouse();
    Utils::CtrlA_Only();
    Utils::CtrlC_Only();

    // 读取剪贴板
    std::wstring clip_text;
    Utils::ReadTextFromClipboard(clip_text);
    Utils::DebugLogW(L"剪贴板：%s", clip_text.c_str());

    // 清空剪贴板
    Utils::ResetClipboard();

    // 向量相似度
    std::vector<float> v1 = {1, 2, 3};
    std::vector<float> v2 = {4, 5, 6};
    float sim = Utils::calc_similarity(v1, v2);
    Utils::DebugLog("相似度：%.2f", sim);

    Utils::DebugLog("=== 运行结束 ===");
    return 0;
}
```

---

## 9. 简明速查
| 模块       | 核心函数                               | 线程安全 | 抛异常 |
|------------|--------------------------------------- |----------|--------|
| 日志       | DebugLog / DebugLogW                   | 否       | 否     |
| 剪贴板     | ReadTextFromClipboard / ResetClipboard | 否       | 否     |
| 文件/目录  | 全部文件函数                           | 是       | 否     |
| 键鼠模拟   | 全部键鼠函数                           | 否       | 否     |
| 数学       | calc_similarity                        | 是       | 否     |