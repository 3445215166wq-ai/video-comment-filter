#include "StateManager.h"
#include <mutex>

StateManager::StateManager(const IApplicationController::FilterState& initial_state)
    : mode_(initial_state.mode),
      positive_threshold_(initial_state.positive_threshold),
      negative_threshold_(initial_state.negative_threshold),
      is_running_(initial_state.is_running),
      record_data_(initial_state.record_data) {}

void StateManager::SetMode(int mode) {
    mode_ = mode;
    NotifySubscribers();
}

void StateManager::SetPositiveThreshold(float value) {
    positive_threshold_ = std::clamp(value, 0.0f, 1.0f);
    NotifySubscribers();
}

void StateManager::SetNegativeThreshold(float value) {
    negative_threshold_ = std::clamp(value, 0.0f, 1.0f);
    NotifySubscribers();
}

void StateManager::SetRecordData(bool enable) {
    record_data_ = enable;
    NotifySubscribers();
}

void StateManager::SetRunning(bool is_running) {
    is_running_ = is_running;
    if (!is_running) {
        stop_requested_ = false;  // 停止时自动清除停止请求标记
    }
    NotifySubscribers();
}

void StateManager::SetStopRequested(bool requested) {
    stop_requested_ = requested;
}

IApplicationController::FilterState StateManager::GetState() const {
    return {
        mode_.load(),
        positive_threshold_.load(),
        negative_threshold_.load(),
        is_running_.load(),
        record_data_.load()
    };
}

int StateManager::GetMode() const {
    return mode_.load();
}

float StateManager::GetPositiveThreshold() const {
    return positive_threshold_.load();
}

float StateManager::GetNegativeThreshold() const {
    return negative_threshold_.load();
}

bool StateManager::IsRecordingData() const {
    return record_data_.load();
}

bool StateManager::IsRunning() const {
    return is_running_.load();
}

bool StateManager::IsStopRequested() const {
    return stop_requested_.load();
}

void StateManager::Subscribe(Callback callback) {
    std::lock_guard<std::mutex> lock(subscribers_mutex_);
    subscribers_.push_back(callback);
}

void StateManager::NotifySubscribers() {
    std::lock_guard<std::mutex> lock(subscribers_mutex_);
    auto state = GetState();
    for (auto& cb : subscribers_) {
        cb(state);
    }
}
