#include "utils.h"
#include "utils_internal.h"
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <string>
#pragma comment(lib, "Shell32.lib")

/**
 * @file clipboard.cpp
 * @brief 剪贴板文本读取工具（兼容 UTF8 / ANSI / HTML / 文件路径）
 */

namespace Utils
{
    /**
     * @brief 从剪贴板读取文本（自动识别编码：宽字符、UTF8、ANSI、HTML、文件路径）
     * @param outText 输出读取到的宽字符串
     * @return 读取成功返回 true
     */
    bool ReadTextFromClipboard(std::wstring& outText)
    {
        outText.clear();
        if (!OpenClipboard(NULL))
            return false;

        // 优先读取 Unicode 宽字符
        HANDLE hData = GetClipboardData(CF_UNICODETEXT);
        if (hData)
        {
            wchar_t* pData = (wchar_t*)GlobalLock(hData);
            if (pData)
            {
                outText = pData;
            }
            GlobalUnlock(hData);
            CloseClipboard();
            return !outText.empty();
        }

        // 读取普通文本（尝试自动编码识别）
        hData = GetClipboardData(CF_TEXT);
        if (hData)
        {
            SIZE_T rawSize = GlobalSize(hData);
            char* pData = (char*)GlobalLock(hData);
            if (pData && rawSize > 0)
            {
                // 调试：保存原始数据
                FILE* fb = nullptr;
                errno_t ferr = fopen_s(&fb, "clipboard_raw.bin", "wb");
                if (ferr == 0 && fb) {
                    fwrite(pData, 1, rawSize, fb);
                    fclose(fb);
                    DebugLog("[剪贴板] 已保存原始数据到 clipboard_raw.bin (大小=%llu)", (unsigned long long)rawSize);
                }

                std::string s(pData, pData + rawSize);
                GlobalUnlock(hData);
                CloseClipboard();

                // UTF8 BOM 识别
                if (s.size() >= 3 && (unsigned char)s[0] == 0xEF && (unsigned char)s[1] == 0xBB && (unsigned char)s[2] == 0xBF) {
                    std::string t = s.substr(3);
                    int len = MultiByteToWideChar(CP_UTF8, 0, t.c_str(), (int)t.size(), NULL, 0);
                    if (len > 0) {
                        outText.assign(len, 0);
                        MultiByteToWideChar(CP_UTF8, 0, t.c_str(), (int)t.size(), &outText[0], len);
                        return !outText.empty();
                    }
                }

                // 普通 UTF8 识别
                if (is_valid_utf8(s)) {
                    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), NULL, 0);
                    if (len > 0) {
                        outText.assign(len, 0);
                        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &outText[0], len);
                        return !outText.empty();
                    }
                }

                // 最后用系统默认编码
                int len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), (int)s.size(), NULL, 0);
                if (len > 0)
                {
                    outText.assign(len, 0);
                    MultiByteToWideChar(CP_ACP, 0, s.c_str(), (int)s.size(), &outText[0], len);
                    return !outText.empty();
                }
            }
        }

        // 读取 HTML 格式剪贴板
        UINT cf_html = RegisterClipboardFormatA("HTML Format");
        if (cf_html != 0) {
            hData = GetClipboardData(cf_html);
            if (hData) {
                SIZE_T rawSize = GlobalSize(hData);
                char* pData = (char*)GlobalLock(hData);
                if (pData && rawSize > 0) {
                    FILE* fh = nullptr;
                    if (fopen_s(&fh, "clipboard_html.html", "wb") == 0 && fh) {
                        fwrite(pData, 1, rawSize, fh);
                        fclose(fh);
                        DebugLog("[剪贴板] 已保存HTML数据到 clipboard_html.html");
                    }
                    std::string s(pData, pData + rawSize);
                    GlobalUnlock(hData);
                    CloseClipboard();

                    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), NULL, 0);
                    if (len > 0) {
                        outText.assign(len, 0);
                        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &outText[0], len);
                        return !outText.empty();
                    }
                }
            }
        }

        // 读取复制的文件路径
        hData = GetClipboardData(CF_HDROP);
        if (hData) {
            HDROP hDrop = (HDROP)hData;
            UINT count = DragQueryFileW(hDrop, 0xFFFFFFFF, NULL, 0);
            if (count > 0) {
                wchar_t fname[MAX_PATH];
                if (DragQueryFileW(hDrop, 0, fname, MAX_PATH)) {
                    outText = fname;
                    CloseClipboard();
                    return true;
                }
            }
        }

        CloseClipboard();
        return false;
    }

    /**
     * @brief 清空剪贴板
     */
    void ResetClipboard()
    {
        if (OpenClipboard(NULL)) {
            EmptyClipboard();
            CloseClipboard();
        }
        Sleep(100);
    }

}