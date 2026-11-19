#pragma once
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <windows.h>  

class IApplicationController {
public:
    virtual ~IApplicationController() = default;

    // ===== 状态查询 =====
    struct FilterState {
        int mode = 0;  // 0=None, 1=Positive, 2=Negative, 3=Both
        float positive_threshold = 0.7f;
        float negative_threshold = 0.8f;
        bool is_running = false;
        bool record_data = false;
    };

    virtual FilterState GetCurrentState() const = 0;

    // ===== 模式控制 =====
    virtual void SetFilterMode(int mode) = 0;

    // ===== 阈值控制 =====
    virtual void SetPositiveThreshold(float value) = 0;
    virtual void SetNegativeThreshold(float value) = 0;
    virtual void AdjustPositiveThreshold(float delta) = 0;
    virtual void AdjustNegativeThreshold(float delta) = 0;

    // ===== 数据记录 =====
    virtual void SetRecordData(bool enable) = 0;

    // ===== 循环控制 =====
    virtual void StartFilteringLoop() = 0;
    virtual void StopFilteringLoop() = 0;

    // ===== 窗口设置 =====
    virtual void SetTargetWindow(const std::wstring& title_keyword) = 0;

    // ===== 文件管理 =====
    virtual void OpenTemplateFolder(bool is_positive) = 0;
    virtual void OpenCommentDataFolder() = 0;
    virtual void OpenSimilarityCheckFolder() = 0;

    // ===== 事件订阅 =====
    using StateChangeCallback = std::function<void(const FilterState&)>;
    using MessageCallback = std::function<void(const std::string&)>;

    virtual void SubscribeStateChange(StateChangeCallback callback) = 0;
    virtual void SubscribeMessage(MessageCallback callback) = 0;

    // ===== 恢复默认设置 =====
    virtual void ResetToDefaultSettings() = 0;

    // ===== 全局热键（需要 UI 传入窗口句柄用于接收消息） =====
    virtual bool RegisterGlobalHotKey(HWND hWnd) = 0;
    virtual void UnregisterGlobalHotKey() = 0;
};

// 工厂函数
std::unique_ptr<IApplicationController> CreateApplicationController(
    const std::wstring& exe_dir,
    const std::wstring& data_dir);