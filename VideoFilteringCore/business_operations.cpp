#include "business_operations.h"
#include "utils/utils.h"

namespace BusinessOps
{
    // 业务1：移到右边→点击激活→全选→复制→读取剪贴板
    bool CopyPageContent(HWND hWnd, std::wstring& outClipboardText)
    {
        if (hWnd == NULL)
        {
            Utils::DebugLog("[错误] CopyPageContent：窗口句柄为空");
            return false;
        }

        // 步骤1：鼠标移到窗口右侧（激活页面焦点）
        Utils::MoveMouseToWindowRight(hWnd);
        // 步骤2：左键单击激活页面
        Utils::LeftClickMouse();
        // 步骤3：全选（Ctrl+A）
        Utils::CtrlA_Only();
        // 步骤4：复制（Ctrl+C）
        Utils::CtrlC_Only();
        // 步骤5：读取剪贴板内容
        bool isSuccess = Utils::ReadTextFromClipboard(outClipboardText);

        if (isSuccess)
        {
            Utils::DebugLogW(L"[成功] 读取剪贴板内容：%s", outClipboardText.c_str());
        }
        else
        {
            Utils::DebugLog("[错误] 读取剪贴板失败");
        }

        return isSuccess;
    }

    // 业务2：移到中心→滚动→回到右边
    void ScrollAndRestorePosition(HWND hWnd, int scrollTimes)
    {
        if (hWnd == NULL || scrollTimes <= 0)
        {
            Utils::DebugLog("[错误] ScrollAndRestorePosition：窗口句柄为空或滚动次数无效");
            return;
        }

        // 步骤1：鼠标移到窗口中心
        Utils::MoveMouseToWindowCenter(hWnd);
        // 步骤2：向下滚动指定次数（复用原有MouseScrollDown）
        for (int i = 0; i < scrollTimes; i++)
        {
            Utils::MouseScrollDown();
        }
        // 步骤3：回到窗口右侧位置
        Utils::MoveMouseToWindowRight(hWnd);

        Utils::DebugLog("[成功] 完成滚动并恢复位置，滚动次数：%d", scrollTimes);
    }
}