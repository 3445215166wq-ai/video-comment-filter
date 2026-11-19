#include "ConfigurationManager.h"
#include <fstream>
#include <filesystem>

ConfigurationManager::ConfigurationManager(const std::wstring& data_dir)
    : data_dir_(data_dir) {}

std::wstring ConfigurationManager::GetConfigPath() const {
    return data_dir_ + L"\\config.json";
}

IApplicationController::FilterState ConfigurationManager::LoadConfig() {
    auto path = GetConfigPath();
    std::ifstream file(path);

    if (!file.is_open()) {
        // 返回默认配置
        return IApplicationController::FilterState();
    }

    try {
        nlohmann::json j;
        file >> j;
        file.close();
        return JsonToState(j);
    } catch (...) {
        // 配置损坏，返回默认配置
        return IApplicationController::FilterState();
    }
}

void ConfigurationManager::SaveConfig(const IApplicationController::FilterState& state) {
    std::filesystem::create_directories(data_dir_);
    auto path = GetConfigPath();

    std::ofstream file(path);
    if (file.is_open()) {
        auto json = StateToJson(state);
        file << json.dump(4);
        file.close();
    }
}

nlohmann::json ConfigurationManager::StateToJson(const IApplicationController::FilterState& state) const {
    nlohmann::json j;
    j["mode"] = state.mode;
    j["positive_threshold"] = state.positive_threshold;
    j["negative_threshold"] = state.negative_threshold;
    j["record_data"] = state.record_data;
    return j;
}

IApplicationController::FilterState ConfigurationManager::JsonToState(const nlohmann::json& j) const {
    IApplicationController::FilterState state;

    if (j.contains("mode")) {
        state.mode = j["mode"].get<int>();
    }
    if (j.contains("positive_threshold")) {
        state.positive_threshold = j["positive_threshold"].get<float>();
    }
    if (j.contains("negative_threshold")) {
        state.negative_threshold = j["negative_threshold"].get<float>();
    }
    if (j.contains("record_data")) {
        state.record_data = j["record_data"].get<bool>();
    }

    return state;
}
