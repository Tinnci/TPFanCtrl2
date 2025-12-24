/**
 * @file ConfigManager.cpp
 * @brief Modernized Configuration Manager using JSON
 */

#include "_prec.h"
#include "ConfigManager.h"
#include <fstream>
#include <iomanip>
#include <spdlog/spdlog.h>

using json = nlohmann::json;

ConfigManager::ConfigManager() {
    // Default Smart Levels (ThinkPad standard-ish)
    SmartLevels1 = {
        {45, 0, 2, 2},
        {50, 1, 2, 2},
        {55, 3, 2, 2},
        {65, 7, 2, 2},
        {75, 64, 2, 2}
    };
    
    SensorWeights.assign(16, 1.0f);
    SensorNames.assign(16, "");
}

void ConfigManager::to_json(json& j) const {
    j = json{
        {"ActiveMode", ActiveMode},
        {"ManFanSpeed", ManFanSpeed},
        {"ProcessPriority", ProcessPriority},
        {"Cycle", Cycle},
        {"StartMinimized", StartMinimized},
        {"MinimizeToSysTray", MinimizeToSysTray},
        {"MinimizeOnClose", MinimizeOnClose},
        {"Language", Language},
        {"PID", {
            {"Target", PID_Target},
            {"Kp", PID_Kp},
            {"Ki", PID_Ki},
            {"Kd", PID_Kd},
            {"Algorithm", ControlAlgorithm}
        }},
        {"SmartLevels1", SmartLevels1},
        {"SmartLevels2", SmartLevels2},
        {"SensorWeights", SensorWeights},
        {"SensorNames", SensorNames},
        {"IgnoreSensors", IgnoreSensors}
    };
}

void ConfigManager::from_json(const json& j) {
    if (j.contains("ActiveMode")) ActiveMode = j.at("ActiveMode").get<int>();
    if (j.contains("ManFanSpeed")) ManFanSpeed = j.at("ManFanSpeed").get<int>();
    if (j.contains("Cycle")) Cycle = j.at("Cycle").get<int>();
    if (j.contains("StartMinimized")) StartMinimized = j.at("StartMinimized").get<int>();
    if (j.contains("MinimizeToSysTray")) MinimizeToSysTray = j.at("MinimizeToSysTray").get<int>();
    if (j.contains("MinimizeOnClose")) MinimizeOnClose = j.at("MinimizeOnClose").get<int>();
    if (j.contains("Language")) Language = j.at("Language").get<std::string>();
    
    if (j.contains("PID")) {
        const auto& p = j.at("PID");
        if (p.contains("Target")) PID_Target = p.at("Target").get<float>();
        if (p.contains("Kp")) PID_Kp = p.at("Kp").get<float>();
        if (p.contains("Ki")) PID_Ki = p.at("Ki").get<float>();
        if (p.contains("Kd")) PID_Kd = p.at("Kd").get<float>();
        if (p.contains("Algorithm")) ControlAlgorithm = p.at("Algorithm").get<int>();
    }

    if (j.contains("SmartLevels1")) SmartLevels1 = j.at("SmartLevels1").get<std::vector<SmartLevel>>();
    if (j.contains("SmartLevels2")) SmartLevels2 = j.at("SmartLevels2").get<std::vector<SmartLevel>>();
    if (j.contains("SensorWeights")) SensorWeights = j.at("SensorWeights").get<std::vector<float>>();
    if (j.contains("SensorNames")) SensorNames = j.at("SensorNames").get<std::vector<std::string>>();
    if (j.contains("IgnoreSensors")) IgnoreSensors = j.at("IgnoreSensors").get<std::string>();
}

bool ConfigManager::LoadConfig(const std::string& filename) {
    try {
        std::ifstream file(filename);
        if (!file.is_open()) {
            spdlog::warn("Config file not found: {}. Using defaults.", filename);
            return false;
        }

        json j;
        file >> j;
        from_json(j);
        spdlog::info("Config loaded from JSON: {}", filename);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to load JSON config: {}. Error: {}", filename, e.what());
        return false;
    }
}

bool ConfigManager::SaveConfig(const std::string& filename) {
    try {
        json j;
        to_json(j);
        std::ofstream file(filename);
        if (!file.is_open()) return false;
        file << std::setw(4) << j << std::endl;
        spdlog::info("Config saved to JSON: {}", filename);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to save JSON config: {}. Error: {}", filename, e.what());
        return false;
    }
}

