#pragma once
#include <string>
#include <nlohmann/json.hpp>
#include "IApplicationController.h"

/**
 * @brief 配置管理器 - 处理配置的持久化和加载
 * 注意：与 StateManager 分离，配置是持久化的，状态是运行时的
 */
class ConfigurationManager {
public:
    explicit ConfigurationManager(const std::wstring& data_dir);

    // 加载和保存配置
    IApplicationController::FilterState LoadConfig();
    void SaveConfig(const IApplicationController::FilterState& state);

    // 配置文件路径
    std::wstring GetConfigPath() const;

private:
    std::wstring data_dir_;

    // 序列化/反序列化
    nlohmann::json StateToJson(const IApplicationController::FilterState& state) const;
    IApplicationController::FilterState JsonToState(const nlohmann::json& j) const;
};
