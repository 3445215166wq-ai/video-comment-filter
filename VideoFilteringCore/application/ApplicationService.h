#pragma once
#include <memory>
#include <thread>
#include <atomic>
#include "IApplicationController.h"
#include "StateManager.h"
#include "ConfigurationManager.h"
#include "FilterEngine.h"

#include <mutex>
#include <vector>
#include <chrono>

/**
 * @brief 应用服务类（业务核心编排层）
 * 实现 IApplicationController 接口，负责整合状态管理、配置、过滤引擎、窗口操作、线程调度
 */
class ApplicationService : public IApplicationController {
public:
    /**
     * @brief 构造函数
     * @param exe_dir 程序可执行文件所在目录
     * @param data_dir 数据存储根目录
     */
    explicit ApplicationService(
        const std::wstring& exe_dir,
        const std::wstring& data_dir
    );

    /**
     * @brief 析构函数：安全停止线程、释放资源
     */
    ~ApplicationService() override;

    /**
     * @brief 初始化服务：加载配置、初始化模型引擎、注册回调
     * @return 初始化成功返回true
     */
    bool Initialize();

    // ===== IApplicationController 实现 =====

    /**
     * @brief 获取当前过滤状态
     * @return 当前状态结构体
     */
    FilterState GetCurrentState() const override;

    /**
     * @brief 设置过滤模式
     * @param mode 模式值（0=关闭,1=正向,2=负向,3=双向）
     */
    void SetFilterMode(int mode) override;

    /**
     * @brief 设置正向模板匹配阈值
     * @param value 相似度阈值
     */
    void SetPositiveThreshold(float value) override;

    /**
     * @brief 设置负向模板匹配阈值
     * @param value 相似度阈值
     */
    void SetNegativeThreshold(float value) override;

    /**
     * @brief 微调正向阈值
     * @param delta 调整增量
     */
    void AdjustPositiveThreshold(float delta) override;

    /**
     * @brief 微调负向阈值
     * @param delta 调整增量
     */
    void AdjustNegativeThreshold(float delta) override;

    /**
     * @brief 启用/禁用数据记录（原始文本、清洗文本、JSON等）
     * @param enable 启用标志
     */
    void SetRecordData(bool enable) override;

    /**
     * @brief 启动后台过滤循环线程
     */
    void StartFilteringLoop() override;

    /**
     * @brief 停止过滤循环，安全退出线程
     */
    void StopFilteringLoop() override;

    /**
     * @brief 根据窗口标题关键词设置目标窗口
     * @param title_keyword 窗口标题包含的字符串
     */
    void SetTargetWindow(const std::wstring& title_keyword) override;

    /**
     * @brief 打开模板文件夹
     * @param is_positive true=正向模板目录，false=负向模板目录
     */
    void OpenTemplateFolder(bool is_positive) override;

    /**
     * @brief 订阅状态变化通知
     * @param callback 状态变化回调函数
     */
    void SubscribeStateChange(StateChangeCallback callback) override;

    /**
     * @brief 订阅日志消息通知
     * @param callback 消息回调函数
     */
    void SubscribeMessage(MessageCallback callback) override;

    /**
     * @brief 重置所有配置为默认值并保存
     */
    void ResetToDefaultSettings() override;

    /**
     * @brief 打开评论解析结果数据目录
     */
    void OpenCommentDataFolder() override;

    /**
     * @brief 注册全局热键（F8）
     * @param hWnd 接收热键消息的窗口句柄
     * @return 注册成功返回true
     */
    bool RegisterGlobalHotKey(HWND hWnd) override;

    /**
     * @brief 注销全局热键
     */
    void UnregisterGlobalHotKey() override;

    /**
     * @brief 打开相似度检测报告目录
     */
    void OpenSimilarityCheckFolder() override;

private:
    std::wstring exe_dir_;                              ///< 程序执行目录
    std::wstring data_dir_;                             ///< 数据根目录

    std::unique_ptr<StateManager> state_;              ///< 状态管理器
    std::unique_ptr<ConfigurationManager> config_;     ///< 配置管理器
    std::unique_ptr<FilterEngine> engine_;             ///< 过滤引擎实例

    std::atomic<bool> loop_active_ = false;            ///< 后台循环运行标志
    std::unique_ptr<std::thread> filter_thread_;       ///< 过滤业务线程

    std::vector<StateChangeCallback> state_subscribers_;   ///< 状态订阅者列表
    std::vector<MessageCallback> message_subscribers_;     ///< 消息订阅者列表
    mutable std::mutex subscribers_mutex_;             ///< 订阅者列表互斥锁

    void BackgroundFilterLoop();                       ///< 后台过滤主循环

    void NotifyStateChange(const FilterState& state);  ///< 通知所有状态订阅者
    void NotifyMessage(const std::string& msg);        ///< 通知所有消息订阅者

    void InitializePaths();                            ///< 初始化模板/数据目录路径

    HWND target_hwnd_ = nullptr;                       ///< 目标窗口句柄
    std::wstring positive_template_dir_;               ///< 正向模板目录路径
    std::wstring negative_template_dir_;               ///< 负向模板目录路径

    HWND hotkey_hwnd_ = nullptr;                       ///< 热键注册窗口句柄
};