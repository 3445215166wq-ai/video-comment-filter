#pragma once
#include <windows.h>
#include <string>
#include <vector>

/**
 * @file utils.h
 * @brief 公共工具模块总接口（日志、键鼠、剪贴板、数学、文件工具）
 */

namespace Utils
{
	// ------------------------------
	// 日志输出
	// ------------------------------
	void DebugLog(const char* fmt, ...);
	void DebugLogW(const wchar_t* fmt, ...);

	// ------------------------------
	// 键盘 / 鼠标模拟
	// ------------------------------
	void CtrlA_Only();
	void CtrlC_Only();
	void Press_C_Only();
	void RightClickPage();
	void LeftClickMouse();
	void MoveMouseToWindowCenter(HWND hWnd);
	void MoveMouseToWindowRight(HWND hWnd);
	void MoveMouseToWindowRightTopThird(HWND hWnd);
	void MouseScrollDown();
	bool IsKeyPress_P();
	void SendEscapeKey();

	// ------------------------------
	// 剪贴板操作
	// ------------------------------
	bool ReadTextFromClipboard(std::wstring& outText);
	void ResetClipboard();

	// ------------------------------
	// 数学计算（余弦相似度）
	// ------------------------------
	float calc_similarity(const std::vector<float>& vec1, const std::vector<float>& vec2);

	// ------------------------------
	// 文件 / 路径工具
	// ------------------------------
	std::wstring GetAppDir();
	std::wstring GetPositiveTemplateFolder();
	std::wstring GetNegativeTemplateFolder();
	std::wstring ReadUtf8File(const std::wstring& filePath);
	std::vector<std::wstring> GetAllJsonFilesInFolder(const std::wstring& folderPath);
	std::wstring ReadPositiveTemplate(const std::wstring& fileName);
	std::wstring ReadNegativeTemplate(const std::wstring& fileName);
}

// 保持原有全局调用兼容
using namespace Utils;