#include "utils.h"
#include <windows.h>
#include <Shlwapi.h>
#include <string>
#include <fstream>
#include <vector>

#pragma comment(lib, "Shlwapi.lib")

namespace Utils
{
    // 1. 获取当前 exe 所在目录（你原来就有）
    std::wstring GetAppDir()
    {
        wchar_t buf[MAX_PATH];
        GetModuleFileNameW(NULL, buf, MAX_PATH);
        PathRemoveFileSpecW(buf);
        return buf;
    }

    // 2. 获取正向模板目录
    std::wstring GetPositiveTemplateFolder()
    {
        return GetAppDir() + L"\\data\\positive_templates\\";
    }

    // 3. 获取负向模板目录
    std::wstring GetNegativeTemplateFolder()
    {
        return GetAppDir() + L"\\data\\negative_templates\\";
    }

    // 4. 读取任意 UTF8 文本文件（JSON/TXT通用）
    std::wstring ReadUtf8File(const std::wstring& filePath)
    {
        std::ifstream f(filePath, std::ios::binary);
        if (!f) return L"";

        std::string content((std::istreambuf_iterator<char>(f)), {});
        f.close();

        // 去掉 UTF8 BOM
        if (content.size() >= 3 &&
            (unsigned char)content[0] == 0xEF &&
            (unsigned char)content[1] == 0xBB &&
            (unsigned char)content[2] == 0xBF)
        {
            content = content.substr(3);
        }

        if (content.empty())
            return L"";

        int len = MultiByteToWideChar(CP_UTF8, 0, content.data(), (int)content.size(), NULL, 0);
        if (len <= 0)
            return L"";

        std::wstring result(len, 0);
        MultiByteToWideChar(CP_UTF8, 0, content.data(), (int)content.size(), &result[0], len);
        return result;
    }

    // 5. 扫描文件夹，返回所有 .json 文件名（核心！）
    std::vector<std::wstring> GetAllJsonFilesInFolder(const std::wstring& folderPath)
    {
        std::vector<std::wstring> files;

        std::wstring searchPath = folderPath;
        searchPath += L"*.json";

        WIN32_FIND_DATAW findData{};
        HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findData);

        if (hFind == INVALID_HANDLE_VALUE)
            return files;

        do
        {
            // 跳过系统目录 . 和 ..
            if (wcscmp(findData.cFileName, L".") == 0 || wcscmp(findData.cFileName, L"..") == 0)
                continue;

            // 只加.json文件
            files.push_back(findData.cFileName);

        } while (FindNextFileW(hFind, &findData));

        FindClose(hFind);
        return files;
    }

    // 6. 读取 正向模板 中的指定JSON文件
    std::wstring ReadPositiveTemplate(const std::wstring& fileName)
    {
        std::wstring fullPath = GetPositiveTemplateFolder() + fileName;
        return ReadUtf8File(fullPath);
    }

    // 7. 读取 负向模板 中的指定JSON文件
    std::wstring ReadNegativeTemplate(const std::wstring& fileName)
    {
        std::wstring fullPath = GetNegativeTemplateFolder() + fileName;
        return ReadUtf8File(fullPath);
    }

}