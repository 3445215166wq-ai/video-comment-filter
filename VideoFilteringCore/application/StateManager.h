#pragma once
#include <atomic>
#include <vector>
#include <memory>
#include "IApplicationController.h"
#include <mutex>       // std::mutex
#include <functional>  // std::function
#include <algorithm>   // std::clamp
/**
 * @brief 状态管理器 - 管理全局可观察状态
 */
class StateManager {
public:
    explicit StateManager(const IApplicationController::FilterState& initial_state = {});

    // 状态更新方法
    void SetMode(int mode);
    void SetPositiveThreshold(float value);
    void SetNegativeThreshold(float value);
    void SetRecordData(bool enable);
    void SetRunning(bool is_running);
    void SetStopRequested(bool requested);

    // 状态查询方法
    IApplicationController::FilterState GetState() const;
    int GetMode() const;
    float GetPositiveThreshold() const;
    float GetNegativeThreshold() const;
    bool IsRecordingData() const;
    bool IsRunning() const;
    bool IsStopRequested() const;

    // 事件订阅
    using Callback = std::function<void(const IApplicationController::FilterState&)>;
    void Subscribe(Callback callback);

private:
    std::atomic<int> mode_ = 0;
    std::atomic<float> positive_threshold_ = 0.7f;
    std::atomic<float> negative_threshold_ = 0.8f;
    std::atomic<bool> is_running_ = false;
    std::atomic<bool> stop_requested_ = false;
    std::atomic<bool> record_data_ = false;

    std::vector<Callback> subscribers_;
    mutable std::mutex subscribers_mutex_;

    void NotifySubscribers();
};
