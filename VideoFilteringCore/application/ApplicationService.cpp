#include "ApplicationService.h"
#include <windows.h>
#include <shellapi.h>
#include <thread>
#include <chrono>
#include <algorithm>
#include <vector>
#include <exception>
#include "../model/string_util.h"
// AI辅助生成：DeepSeek-R1, 2026-4-1
// ========== 匿名命名空间：辅助函数 ==========
namespace {

    /**
     * @brief 窗口枚举回调数据结构
     * 用于传递查找关键词和接收结果窗口句柄
     */
    struct FindWindowData {
        const std::wstring* keyword;
        HWND result;
    };

    /**
     * @brief 窗口遍历回调函数，用于按标题子串查找窗口
     * @param hwnd 当前遍历到的窗口句柄
     * @param lparam 自定义数据指针（FindWindowData）
     * @return 继续遍历返回TRUE，找到目标返回FALSE
     */
    BOOL CALLBACK FindWindowEnumProc(HWND hwnd, LPARAM lparam) {
        auto* data = reinterpret_cast<FindWindowData*>(lparam);
        int len = GetWindowTextLengthW(hwnd);
        if (len <= 0) return TRUE;
        std::wstring buf(len + 1, L'\0');
        GetWindowTextW(hwnd, &buf[0], len + 1);
        buf.resize(wcslen(buf.c_str()));
        if (buf.find(*data->keyword) != std::wstring::npos) {
            data->result = hwnd;
            return FALSE;
        }
        return TRUE;
    }

    /**
     * @brief 根据标题子串查找顶层窗口
     * @param keyword 窗口标题包含的关键词
     * @return 找到的窗口句柄，未找到返回nullptr
     */
    HWND FindWindowByTitleSubstring(const std::wstring& keyword) {
        FindWindowData data{ &keyword, nullptr };
        EnumWindows(FindWindowEnumProc, reinterpret_cast<LPARAM>(&data));
        return data.result;
    }

    /**
     * @brief 支持中断检查的等待函数
     * @tparam StopChecker 停止检查回调类型
     * @param total_ms 总等待时间
     * @param should_stop 停止检查函数，返回true则立即中断等待
     * @return 正常等待完成返回true，被中断返回false
     */
    template<typename StopChecker>
    bool WaitWithStopCheck(DWORD total_ms, StopChecker should_stop) {
        DWORD elapsed = 0;
        while (elapsed < total_ms) {
            if (should_stop()) return false;
            const DWORD step = std::min<DWORD>(50, total_ms - elapsed);
            Sleep(step);
            elapsed += step;
        }
        return true;
    }

} // namespace

// ========== ApplicationService 实现 ==========

/**
 * @brief 构造函数：初始化服务实例、路径、状态与引擎
 * @param exe_dir 程序执行目录
 * @param data_dir 数据存储目录
 */
ApplicationService::ApplicationService(
    const std::wstring& exe_dir,
    const std::wstring& data_dir)
    : exe_dir_(exe_dir),
    data_dir_(data_dir),
    state_(std::make_unique<StateManager>()),
    config_(std::make_unique<ConfigurationManager>(data_dir)),
    engine_(std::make_unique<FilterEngine>()) {
    InitializePaths();
}

/**
 * @brief 析构函数：安全停止线程、关闭引擎、释放资源
 */
ApplicationService::~ApplicationService() {
    StopFilteringLoop();
    if (filter_thread_ && filter_thread_->joinable()) {
        filter_thread_->join();
    }
    engine_->Shutdown();
}

/**
 * @brief 初始化应用服务：加载配置、初始化过滤引擎、注册状态回调
 * @return 初始化成功返回true
 */
bool ApplicationService::Initialize() {
    // 加载配置
    auto saved_state = config_->LoadConfig();
    state_->SetMode(saved_state.mode);
    state_->SetPositiveThreshold(saved_state.positive_threshold);
    state_->SetNegativeThreshold(saved_state.negative_threshold);
    state_->SetRecordData(saved_state.record_data);

    // 设置 FilterEngine 所需目录
    engine_->SetDataDirectory(data_dir_);
    engine_->SetTemplateDirectories(positive_template_dir_, negative_template_dir_);

    // 初始化引擎
    std::wstring model_path = exe_dir_ + L"\\models\\model.onnx";
    if (!engine_->Initialize(model_path)) {
        NotifyMessage("[ERROR] Failed to initialize filter engine");
        return false;
    }

    // 订阅状态变化以自动保存配置
    state_->Subscribe([this](const IApplicationController::FilterState& state) {
        config_->SaveConfig(state);
        NotifyStateChange(state);
        });

    NotifyMessage("[INFO] Application initialized successfully");
    return true;
}

/**
 * @brief 获取当前过滤状态
 * @return 当前状态结构体
 */
IApplicationController::FilterState ApplicationService::GetCurrentState() const {
    return state_->GetState();
}

/**
 * @brief 设置过滤模式
 * @param mode 模式值（0-3）
 */
void ApplicationService::SetFilterMode(int mode) {
    if (mode >= 0 && mode <= 3) {
        state_->SetMode(mode);
    }
}

/**
 * @brief 设置正向匹配阈值
 * @param value 阈值
 */
void ApplicationService::SetPositiveThreshold(float value) {
    state_->SetPositiveThreshold(value);
}

/**
 * @brief 设置负向匹配阈值
 * @param value 阈值
 */
void ApplicationService::SetNegativeThreshold(float value) {
    state_->SetNegativeThreshold(value);
}

/**
 * @brief 增量调整正向阈值
 * @param delta 增量值
 */
void ApplicationService::AdjustPositiveThreshold(float delta) {
    float current = state_->GetPositiveThreshold();
    state_->SetPositiveThreshold(current + delta);
}

/**
 * @brief 增量调整负向阈值
 * @param delta 增量值
 */
void ApplicationService::AdjustNegativeThreshold(float delta) {
    float current = state_->GetNegativeThreshold();
    state_->SetNegativeThreshold(current + delta);
}

/**
 * @brief 启用/禁用数据记录
 * @param enable 启用标志
 */
void ApplicationService::SetRecordData(bool enable) {
    state_->SetRecordData(enable);
}

/**
 * @brief 启动后台过滤循环线程
 */
void ApplicationService::StartFilteringLoop() {
    if (!target_hwnd_) {
        std::wstring wideMsg = L"[WARN] 未设置目标窗口...";
        std::string utf8Msg = wstring_to_utf8(wideMsg);
        NotifyMessage(utf8Msg);
        return;
    }

    if (state_->IsRunning()) {
        NotifyMessage("[WARN] Filtering loop already running");
        return;
    }

    if (state_->GetMode() == 0) {
        NotifyMessage("[ERROR] No filter mode selected");
        return;
    }

    loop_active_ = true;
    state_->SetRunning(true);
    filter_thread_ = std::make_unique<std::thread>([this] { BackgroundFilterLoop(); });

    NotifyMessage("[INFO] Filtering loop started");
}

/**
 * @brief 安全停止过滤循环，等待线程退出
 */
void ApplicationService::StopFilteringLoop() {
    if (!state_->IsRunning()) {
        return;
    }

    state_->SetStopRequested(true);
    loop_active_ = false;

    if (filter_thread_ && filter_thread_->joinable()) {
        filter_thread_->join();
    }

    state_->SetRunning(false);
    NotifyMessage("[INFO] Filtering loop stopped");
}

/**
 * @brief 根据标题关键词设置目标窗口
 * @param title_keyword 窗口标题关键词
 */
void ApplicationService::SetTargetWindow(const std::wstring& title_keyword) {
    target_hwnd_ = FindWindowByTitleSubstring(title_keyword);
    if (target_hwnd_) {
        NotifyMessage("[INFO] Target window set: " + wstring_to_utf8(title_keyword));
    }
    else {
        NotifyMessage("[WARN] Target window not found: " + wstring_to_utf8(title_keyword));
    }
}

/**
 * @brief 打开相似度检测报告目录
 */
void ApplicationService::OpenSimilarityCheckFolder() {
    std::wstring similarityDir = data_dir_ + L"\\SimilarityCheck";
    std::filesystem::create_directories(similarityDir);
    ShellExecuteW(NULL, L"open", similarityDir.c_str(), NULL, NULL, SW_SHOW);
}

/**
 * @brief 打开模板文件夹
 * @param is_positive true=正向模板，false=负向模板
 */
void ApplicationService::OpenTemplateFolder(bool is_positive) {
    auto path = is_positive ? positive_template_dir_ : negative_template_dir_;
    ShellExecuteW(NULL, L"open", path.c_str(), NULL, NULL, SW_SHOW);
}

/**
 * @brief 打开解析结果数据目录
 */
void ApplicationService::OpenCommentDataFolder() {
    std::wstring parser_dir = data_dir_ + L"\\parser";
    std::filesystem::create_directories(parser_dir);
    ShellExecuteW(NULL, L"open", parser_dir.c_str(), NULL, NULL, SW_SHOW);
}

/**
 * @brief 订阅状态变化通知
 * @param callback 状态变化回调函数
 */
void ApplicationService::SubscribeStateChange(StateChangeCallback callback) {
    std::lock_guard<std::mutex> lock(subscribers_mutex_);
    state_subscribers_.push_back(callback);
    callback(GetCurrentState());
}

/**
 * @brief 订阅日志消息通知
 * @param callback 消息回调函数
 */
void ApplicationService::SubscribeMessage(MessageCallback callback) {
    std::lock_guard<std::mutex> lock(subscribers_mutex_);
    message_subscribers_.push_back(callback);
}

/**
 * @brief 重置所有配置为默认值并保存
 */
void ApplicationService::ResetToDefaultSettings() {
    state_->SetMode(0);
    state_->SetPositiveThreshold(0.7f);
    state_->SetNegativeThreshold(0.8f);
    state_->SetRecordData(false);
    NotifyMessage("[INFO] Settings reset to defaults and saved.");
}

/**
 * @brief 后台过滤循环：持续执行页面抓取、清洗、匹配、决策
 */
void ApplicationService::BackgroundFilterLoop() {
    // 提示：过滤循环开始，等待2秒
    NotifyMessage("[INFO] Filter loop starting, waiting 2 seconds...");
    auto shouldStop = [this]() { return state_->IsStopRequested(); };

    // 启动前等待2秒，支持中途停止
    if (!WaitWithStopCheck(2000, shouldStop)) {
        state_->SetRunning(false);
        NotifyMessage("[INFO] Filter loop cancelled before start.");
        return;
    }

    // ===================== 批量窗口配置 =====================
    // 批量处理的窗口列表，可添加多个窗口句柄
    std::vector<HWND> batch_windows = {
        target_hwnd_,  // 默认窗口（必须保留）
        // 可继续添加：FindWindowByTitleSubstring(L"窗口标题")
    };

    // 主循环：持续运行直到手动停止
    while (loop_active_ && state_->IsRunning()) {
        if (shouldStop()) break;

        // ====================== 性能计时开始 ======================
        LARGE_INTEGER freq, start;
        QueryPerformanceFrequency(&freq);  // 获取系统计时频率
        QueryPerformanceCounter(&start);  // 记录批次开始时间

        // 已处理窗口数量统计
        int processed = 0;

        // ====================== 批量遍历所有窗口 ======================
        for (HWND hwnd : batch_windows) {
            if (shouldStop()) break;  // 检查是否需要停止

            try {
                // 执行单窗口过滤（核心业务逻辑，不修改）
                auto result = engine_->ExecuteFilterCycle(
                    hwnd,                          // 当前批量窗口
                    state_->GetMode(),
                    state_->GetPositiveThreshold(),
                    state_->GetNegativeThreshold(),
                    state_->IsRecordingData()
                );

                processed++;  // 统计成功处理的窗口数

                // 输出提示信息
                if (!result.message.empty())
                    NotifyMessage(result.message);

                // 执行失败：跳过当前窗口
                if (!result.success) {
                    WaitWithStopCheck(500, shouldStop);
                    continue;
                }

                // 匹配成功：停止全部循环
                if (result.decision == 0) {
                    state_->SetRunning(false);
                    NotifyMessage("[INFO] Match succeeded, stopping loop.");
                    goto END_LOOP;  // 跳出所有循环
                }

                // 窗口间间隔，避免过快执行
                if (!WaitWithStopCheck(500, shouldStop))
                    break;
            }
            catch (...) {
                // 异常捕获：跳过错误窗口
                NotifyMessage("[ERROR] Exception, skipping current window.");
                if (!WaitWithStopCheck(500, shouldStop)) break;
            }
        }

        // ====================== 计时结束 & 打印批次耗时 ======================
        LARGE_INTEGER end;
        QueryPerformanceCounter(&end);
        double ms = (end.QuadPart - start.QuadPart) * 1000.0 / freq.QuadPart;

        // 输出批次性能日志（你能直接看到速度）
        NotifyMessage("[INFO] Batch processed: count=" + std::to_string(processed)
            + ",total time=" + std::to_string((int)ms) + "ms");

        // 批次完成后休息1秒
        if (!WaitWithStopCheck(1000, shouldStop)) break;
    }

END_LOOP:
    // 循环退出，重置运行状态
    state_->SetRunning(false);
    NotifyMessage("[INFO] Filter loop exited.");
}

/**
 * @brief 向所有订阅者发送状态更新通知
 * @param state 新状态
 */
void ApplicationService::NotifyStateChange(const FilterState& state) {
    std::lock_guard<std::mutex> lock(subscribers_mutex_);
    for (auto& cb : state_subscribers_) {
        cb(state);
    }
}

/**
 * @brief 向所有订阅者发送日志消息
 * @param msg 日志字符串
 */
void ApplicationService::NotifyMessage(const std::string& msg) {
    std::lock_guard<std::mutex> lock(subscribers_mutex_);
    for (auto& cb : message_subscribers_) {
        cb(msg);
    }
}

/**
 * @brief 初始化模板目录路径
 */
void ApplicationService::InitializePaths() {
    positive_template_dir_ = data_dir_ + L"\\positive_templates";
    negative_template_dir_ = data_dir_ + L"\\negative_templates";
}

/**
 * @brief 注册全局热键（F8）
 * @param hWnd 接收热键消息的窗口句柄
 * @return 注册成功返回true
 */
bool ApplicationService::RegisterGlobalHotKey(HWND hWnd) {
    if (!hWnd) return false;
    hotkey_hwnd_ = hWnd;
    if (!::RegisterHotKey(hWnd, 1001, 0, VK_F8)) {
        NotifyMessage("[WARN] Global hotkey F8 registration failed");
        return false;
    }
    NotifyMessage("[INFO] Global hotkey F8 registered");
    return true;
}

/**
 * @brief 注销全局热键
 */
void ApplicationService::UnregisterGlobalHotKey() {
    if (hotkey_hwnd_) {
        ::UnregisterHotKey(hotkey_hwnd_, 1001);
        hotkey_hwnd_ = nullptr;
        NotifyMessage("[INFO] Global hotkey F8 unregistered");
    }
}

// ========== 工厂函数 ==========

/**
 * @brief 创建应用控制器实例（工厂方法）
 * @param exe_dir 程序目录
 * @param data_dir 数据目录
 * @return 控制器智能指针，初始化失败返回nullptr
 */
std::unique_ptr<IApplicationController> CreateApplicationController(
    const std::wstring& exe_dir,
    const std::wstring& data_dir) {
    auto service = std::make_unique<ApplicationService>(exe_dir, data_dir);
    if (service->Initialize()) {
        return service;
    }
    return nullptr;
}