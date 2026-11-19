#pragma once
#include <windows.h>
#include <string>

// 业务层封装：复用Utils底层函数，组装成业务逻辑
namespace BusinessOps
{
    // ===== 文本复制业务（移到右边→点击激活→全选→复制→读取剪贴板）=====
    bool CopyPageContent(HWND hWnd, std::wstring& outClipboardText);

    // ===== 滚动业务（移到中心→滚动→回到右边）=====
    void ScrollAndRestorePosition(HWND hWnd, int scrollTimes = 1);
}