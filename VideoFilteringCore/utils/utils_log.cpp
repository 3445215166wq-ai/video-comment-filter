#include "utils.h"
#include "utils_internal.h"
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string>

/**
 * @file logging.cpp
 * @brief 调试日志工具（输出到调试器 + 写入 UTF-8 日志文件）
 */

namespace Utils
{
    /**
     * @brief 写入调试日志（ANSI / UTF-8 自动识别）
     * 输出到 DebugView，并追加到 videodemo_debug.txt（自动带 BOM）
     */
    void DebugLog(const char* fmt, ...)
    {
        char buf[2048] = { 0 };
        va_list ap;
        va_start(ap, fmt);
        vsprintf_s(buf, sizeof(buf), fmt, ap);
        va_end(ap);

        size_t buflen = strlen(buf);
        bool buf_is_utf8 = buflen > 0 && is_valid_utf8(buf, buflen);

        // 输出到调试器
        int wlen = MultiByteToWideChar(buf_is_utf8 ? CP_UTF8 : CP_ACP, 0, buf, -1, NULL, 0);
        if (wlen > 0) {
            std::wstring wbuf((size_t)wlen - 1, L'\0');
            MultiByteToWideChar(buf_is_utf8 ? CP_UTF8 : CP_ACP, 0, buf, -1, &wbuf[0], wlen);
            OutputDebugStringW(wbuf.c_str());
        }
        else {
            OutputDebugStringA(buf);
        }

        // 写入日志文件（自动 UTF-8 + BOM）
        FILE* f = nullptr;
        errno_t err = fopen_s(&f, "videodemo_debug.txt", "ab");
        if (err == 0 && f) {
            long pos = ftell(f);
            if (pos == 0) { // 空文件则写入 UTF-8 BOM
                const unsigned char bom[] = { 0xEF, 0xBB, 0xBF };
                fwrite(bom, 1, sizeof(bom), f);
            }

            if (buf_is_utf8) {
                fwrite(buf, 1, buflen, f);
            }
            else {
                // ANSI 转 UTF-8 再写入
                int wlen2 = MultiByteToWideChar(CP_ACP, 0, buf, (int)buflen, NULL, 0);
                if (wlen2 > 0) {
                    std::wstring tmp((size_t)wlen2, L'\0');
                    MultiByteToWideChar(CP_ACP, 0, buf, (int)buflen, &tmp[0], wlen2);

                    int outlen = WideCharToMultiByte(CP_UTF8, 0, tmp.c_str(), (int)tmp.size(), NULL, 0, NULL, NULL);
                    if (outlen > 0) {
                        std::string out((size_t)outlen, '\0');
                        WideCharToMultiByte(CP_UTF8, 0, tmp.c_str(), (int)tmp.size(), &out[0], outlen, NULL, NULL);
                        fwrite(out.data(), 1, out.size(), f);
                    }
                }
            }

            fwrite("\n", 1, 1, f);
            fclose(f);
        }
    }

    /**
     * @brief 宽字符版本日志（直接写入 UTF-8 日志）
     */
    void DebugLogW(const wchar_t* fmt, ...)
    {
        wchar_t wbuf[2048] = { 0 };
        va_list ap;
        va_start(ap, fmt);
        vswprintf_s(wbuf, sizeof(wbuf) / sizeof(wchar_t), fmt, ap);
        va_end(ap);

        OutputDebugStringW(wbuf);

        // 转 UTF-8 写入文件
        int utf8_len = WideCharToMultiByte(CP_UTF8, 0, wbuf, -1, NULL, 0, NULL, NULL);
        if (utf8_len > 0) {
            std::string out((size_t)utf8_len - 1, '\0');
            WideCharToMultiByte(CP_UTF8, 0, wbuf, -1, &out[0], utf8_len, NULL, NULL);

            FILE* f = nullptr;
            errno_t err = fopen_s(&f, "videodemo_debug.txt", "ab");
            if (err == 0 && f) {
                long pos = ftell(f);
                if (pos == 0) {
                    const unsigned char bom[] = { 0xEF, 0xBB, 0xBF };
                    fwrite(bom, 1, sizeof(bom), f);
                }

                fwrite(out.c_str(), 1, out.size(), f);
                fwrite("\n", 1, 1, f);
                fclose(f);
            }
        }
    }

}