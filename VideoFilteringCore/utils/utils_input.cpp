#include "utils.h"
#include <windows.h>

/**
 * @file input.cpp
 * @brief 键盘鼠标模拟工具（Windows 平台）
 */

namespace Utils
{
    /**
     * @brief 模拟 Ctrl+A 全选
     */
    void CtrlA_Only()
    {
        INPUT input[2] = { 0 };
        input[0].type = INPUT_KEYBOARD;
        input[0].ki.wVk = VK_CONTROL;
        input[1].type = INPUT_KEYBOARD;
        input[1].ki.wVk = 'A';
        SendInput(2, input, sizeof(INPUT));
        Sleep(100);
        input[0].ki.dwFlags = KEYEVENTF_KEYUP;
        input[1].ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(2, input, sizeof(INPUT));
        Sleep(200);
    }

    /**
     * @brief 模拟 Ctrl+C 复制
     */
    void CtrlC_Only()
    {
        INPUT in[2] = { 0 };
        in[0].type = INPUT_KEYBOARD; in[0].ki.wVk = VK_CONTROL;
        in[1].type = INPUT_KEYBOARD; in[1].ki.wVk = 'C';
        SendInput(2, in, sizeof(INPUT));
        Sleep(100);
        in[0].ki.dwFlags = KEYEVENTF_KEYUP;
        in[1].ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(2, in, sizeof(INPUT));
        Sleep(200);
    }

    /**
     * @brief 单独按下并松开 C 键
     */
    void Press_C_Only()
    {
        INPUT input = { 0 };
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = 'C';
        SendInput(1, &input, sizeof(INPUT));
        Sleep(150);
        input.ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(1, &input, sizeof(INPUT));
        Sleep(200);
    }

    /**
     * @brief 模拟鼠标右键单击
     */
    void RightClickPage()
    {
        mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, 0);
        Sleep(100);
        mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, 0);
        Sleep(300);
    }

    /**
     * @brief 模拟鼠标左键单击
     */
    void LeftClickMouse()
    {
        mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
        Sleep(50);
        mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
        Sleep(500);
    }

    /**
     * @brief 移动鼠标到窗口正中心
     * @param hWnd 目标窗口句柄
     */
    void MoveMouseToWindowCenter(HWND hWnd)
    {
        RECT r;
        GetWindowRect(hWnd, &r);
        int x = (r.left + r.right) / 2;
        int y = (r.top + r.bottom) / 2;
        SetCursorPos(x, y);
        Sleep(200);
    }

    /**
     * @brief 移动鼠标到窗口右侧位置
     * @param hWnd 目标窗口句柄
     */
    void MoveMouseToWindowRight(HWND hWnd)
    {
        RECT r;
        GetWindowRect(hWnd, &r);
        int width = r.right - r.left;
        int x = r.right - width / 20;
        int y = (r.top + r.bottom) / 2;
        SetCursorPos(x, y);
        Sleep(200);
    }

    /**
     * @brief 移动鼠标到窗口右上侧 1/3 高度位置
     * @param hWnd 目标窗口句柄
     */
    void MoveMouseToWindowRightTopThird(HWND hWnd)
    {
        RECT r;
        GetWindowRect(hWnd, &r);
        int width = r.right - r.left;
        int height = r.bottom - r.top;
        int margin = max(8, width / 40);
        int x = r.right - margin;
        int y = r.top + height / 3;
        SetCursorPos(x, y);
        Sleep(200);
    }

    /**
     * @brief 鼠标滚轮向下滚动
     */
    void MouseScrollDown()
    {
        mouse_event(MOUSEEVENTF_WHEEL, 0, 0, -120, 0);
        Sleep(100);
    }

    /**
     * @brief 判断 P 键是否正在按下
     * @return 按下返回 true
     */
    bool IsKeyPress_P()
    {
        return (GetAsyncKeyState('P') & 0x8000) != 0;
    }

    /**
     * @brief 模拟按下 ESC 键
     */
    void SendEscapeKey()
    {
        INPUT k[2] = { 0 };
        k[0].type = INPUT_KEYBOARD; k[0].ki.wVk = VK_ESCAPE;
        k[1].type = INPUT_KEYBOARD; k[1].ki.wVk = VK_ESCAPE; k[1].ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(2, k, sizeof(INPUT));
        Sleep(200);
    }

}