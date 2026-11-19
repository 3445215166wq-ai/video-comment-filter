#pragma once
#include <string>
#include <vector>
#include <memory>
#include <windows.h>

/**
 * @class FilterEngine
 * @brief 文本过滤引擎核心类，负责文本抓取、清洗、解析、嵌入计算、相似度匹配与决策执行
 */
class FilterEngine {
public:
    /**
     * @struct Result
     * @brief 过滤周期执行结果
     * @var success 操作是否成功
     * @var decision 决策结果：0=停止，1=滚动下一个
     * @var message 结果描述信息
     */
    struct Result {
        bool success = false;
        int decision = 0;  // 0=stop, 1=scroll
        std::string message;
    };

    /**
     * @brief 初始化引擎，加载ONNX模型
     * @param model_path 模型文件路径
     * @return 初始化成功返回true，失败返回false
     */
    bool Initialize(const std::wstring& model_path);

    /**
     * @brief 关闭引擎，释放模型资源
     */
    void Shutdown();

    /**
     * @brief 设置数据输出根目录
     * @param data_dir 数据保存目录路径
     */
    void SetDataDirectory(const std::wstring& data_dir);

    /**
     * @brief 设置正向/负向模板文件目录
     * @param positive_dir 正向模板目录路径
     * @param negative_dir 负向模板目录路径
     */
    void SetTemplateDirectories(const std::wstring& positive_dir, const std::wstring& negative_dir);

    /**
     * @brief 执行单次完整过滤流程：抓取→清洗→解析→编码→相似度匹配→执行动作
     * @param target_window 目标窗口句柄
     * @param mode 运行模式
     * @param positive_threshold 正向相似度阈值
     * @param negative_threshold 负向相似度阈值
     * @param record_data 是否记录原始/清洗/解析数据
     * @return 过滤执行结果结构体
     */
    Result ExecuteFilterCycle(
        HWND target_window,
        int mode,
        float positive_threshold,
        float negative_threshold,
        bool record_data
    );

private:
    bool is_initialized_ = false;               ///< 引擎初始化状态
    std::wstring data_dir_;                     ///< 数据输出根目录
    std::wstring positive_template_dir_;        ///< 正向模板文件目录
    std::wstring negative_template_dir_;       ///< 负向模板文件目录
};