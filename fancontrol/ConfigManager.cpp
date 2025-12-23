/**
 * @file ConfigManager.cpp
 * @brief Configuration file manager with table-driven parsing
 * 
 * Refactored to use a dispatch table pattern to reduce cyclomatic complexity
 * from 65 to ~5, while maintaining identical functionality.
 */

#include "_prec.h"
#include "ConfigManager.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <functional>
#include <unordered_map>

// ============================================================================
// Parser Handler Types
// ============================================================================

using IntParser = std::function<void(ConfigManager*, const std::string&)>;
using FloatParser = std::function<void(ConfigManager*, const std::string&)>;
using StringParser = std::function<void(ConfigManager*, const std::string&)>;

// ============================================================================
// Helper Functions
// ============================================================================

namespace {

/**
 * @brief Trim whitespace from a string
 */
inline std::string Trim(const std::string& str) {
    auto copy = str;
    copy.erase(std::remove_if(copy.begin(), copy.end(), isspace), copy.end());
    return copy;
}

/**
 * @brief Create a parser for integer fields
 */
template<typename T>
IntParser MakeIntParser(T ConfigManager::* member) {
    return [member](ConfigManager* cfg, const std::string& value) {
        cfg->*member = std::stoi(value);
    };
}

/**
 * @brief Create a parser for integer fields with hex support
 */
template<typename T>
IntParser MakeHexIntParser(T ConfigManager::* member) {
    return [member](ConfigManager* cfg, const std::string& value) {
        cfg->*member = std::stoi(value, nullptr, 0);
    };
}

/**
 * @brief Create a parser for float fields
 */
FloatParser MakeFloatParser(float ConfigManager::* member) {
    return [member](ConfigManager* cfg, const std::string& value) {
        cfg->*member = std::stof(value);
    };
}

/**
 * @brief Create a parser for string fields
 */
StringParser MakeStringParser(std::string ConfigManager::* member) {
    return [member](ConfigManager* cfg, const std::string& value) {
        cfg->*member = value;
    };
}

/**
 * @brief Create a parser for menu label fields (extracts before '/')
 */
StringParser MakeMenuLabelParser(std::string ConfigManager::* member) {
    return [member](ConfigManager* cfg, const std::string& value) {
        cfg->*member = value.substr(0, value.find('/'));
    };
}

} // anonymous namespace

// ============================================================================
// Parser Dispatch Tables (Static)
// ============================================================================

/**
 * @brief Integer field parser table
 * Maps config key names to their corresponding member variable parsers
 */
static const std::unordered_map<std::string, IntParser>& GetIntParsers() {
    static const std::unordered_map<std::string, IntParser> parsers = {
        {"Active",          MakeIntParser(&ConfigManager::ActiveMode)},
        {"ManFanSpeed",     MakeIntParser(&ConfigManager::ManFanSpeed)},
        {"ProcessPriority", MakeIntParser(&ConfigManager::ProcessPriority)},
        {"cycle",           MakeIntParser(&ConfigManager::Cycle)},
        {"IconCycle",       MakeIntParser(&ConfigManager::IconCycle)},
        {"ReIcCycle",       MakeIntParser(&ConfigManager::ReIcCycle)},
        {"IconFontSize",    MakeIntParser(&ConfigManager::IconFontSize)},
        {"NoExtSensor",     MakeIntParser(&ConfigManager::NoExtSensor)},
        {"SlimDialog",      MakeIntParser(&ConfigManager::SlimDialog)},
        {"NoWaitMessage",   MakeIntParser(&ConfigManager::NoWaitMessage)},
        {"StartMinimized",  MakeIntParser(&ConfigManager::StartMinimized)},
        {"NoBallons",       MakeIntParser(&ConfigManager::NoBallons)},
        {"IconColorFan",    MakeIntParser(&ConfigManager::IconColorFan)},
        {"Lev64Norm",       MakeIntParser(&ConfigManager::Lev64Norm)},
        {"BluetoothEDR",    MakeIntParser(&ConfigManager::BluetoothEDR)},
        {"ManModeExit",     MakeIntParser(&ConfigManager::ManModeExit)},
        {"ShowBiasedTemps", MakeIntParser(&ConfigManager::ShowBiasedTemps)},
        {"MaxReadErrors",   MakeIntParser(&ConfigManager::MaxReadErrors)},
        {"SecWinUptime",    MakeIntParser(&ConfigManager::SecWinUptime)},
        {"SecStartDelay",   MakeIntParser(&ConfigManager::SecStartDelay)},
        {"Log2File",        MakeIntParser(&ConfigManager::Log2File)},
        {"StayOnTop",       MakeIntParser(&ConfigManager::StayOnTop)},
        {"Log2csv",         MakeIntParser(&ConfigManager::Log2csv)},
        {"ShowAll",         MakeIntParser(&ConfigManager::ShowAll)},
        {"ShowTempIcon",    MakeIntParser(&ConfigManager::ShowTempIcon)},
        {"Fahrenheit",      MakeIntParser(&ConfigManager::Fahrenheit)},
        {"MinimizeToSysTray", MakeIntParser(&ConfigManager::MinimizeToSysTray)},
        {"MinimizeOnClose", MakeIntParser(&ConfigManager::MinimizeOnClose)},
        {"UseTWR",          MakeIntParser(&ConfigManager::UseTWR)},
        {"ControlAlgorithm", MakeIntParser(&ConfigManager::ControlAlgorithm)},
    };
    return parsers;
}

/**
 * @brief Hex integer field parser table
 */
static const std::unordered_map<std::string, IntParser>& GetHexIntParsers() {
    static const std::unordered_map<std::string, IntParser> parsers = {
        {"FanSpeedLowByte", MakeHexIntParser(&ConfigManager::FanSpeedLowByte)},
    };
    return parsers;
}

/**
 * @brief Float field parser table
 */
static const std::unordered_map<std::string, FloatParser>& GetFloatParsers() {
    static const std::unordered_map<std::string, FloatParser> parsers = {
        {"PID_Target", MakeFloatParser(&ConfigManager::PID_Target)},
        {"PID_Kp",     MakeFloatParser(&ConfigManager::PID_Kp)},
        {"PID_Ki",     MakeFloatParser(&ConfigManager::PID_Ki)},
        {"PID_Kd",     MakeFloatParser(&ConfigManager::PID_Kd)},
    };
    return parsers;
}

/**
 * @brief String field parser table
 */
static const std::unordered_map<std::string, StringParser>& GetStringParsers() {
    static const std::unordered_map<std::string, StringParser> parsers = {
        {"IgnoreSensors", MakeStringParser(&ConfigManager::IgnoreSensors)},
        {"Language",      MakeStringParser(&ConfigManager::Language)},
    };
    return parsers;
}

/**
 * @brief Menu label parser table
 */
static const std::unordered_map<std::string, StringParser>& GetMenuLabelParsers() {
    static const std::unordered_map<std::string, StringParser> parsers = {
        {"MenuLabelSM1", MakeMenuLabelParser(&ConfigManager::MenuLabelSM1)},
        {"MenuLabelSM2", MakeMenuLabelParser(&ConfigManager::MenuLabelSM2)},
    };
    return parsers;
}

// ============================================================================
// ConfigManager Implementation
// ============================================================================

ConfigManager::ConfigManager() {
    SensorOffsets.resize(16, {0, -1, -1});
}

bool ConfigManager::LoadConfig(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) return false;

    std::string line;
    while (std::getline(file, line)) {
        ParseLine(line);
    }
    return true;
}

/**
 * @brief Parse a single configuration line using dispatch tables
 * 
 * This refactored version uses lookup tables instead of a long if-else chain,
 * reducing cyclomatic complexity from 65 to approximately 5.
 * 
 * @param line The configuration line to parse
 */
void ConfigManager::ParseLine(const std::string& line) {
    // Skip empty lines and comments
    if (line.empty() || line[0] == '/' || line[0] == '#' || line[0] == ';') {
        return;
    }

    // Find the key=value separator
    auto pos = line.find('=');
    if (pos == std::string::npos) return;

    std::string key = Trim(line.substr(0, pos));
    std::string value = line.substr(pos + 1);

    // Try integer parsers
    auto& intParsers = GetIntParsers();
    if (auto it = intParsers.find(key); it != intParsers.end()) {
        it->second(this, value);
        return;
    }

    // Try hex integer parsers
    auto& hexParsers = GetHexIntParsers();
    if (auto it = hexParsers.find(key); it != hexParsers.end()) {
        it->second(this, value);
        return;
    }

    // Try float parsers
    auto& floatParsers = GetFloatParsers();
    if (auto it = floatParsers.find(key); it != floatParsers.end()) {
        it->second(this, value);
        return;
    }

    // Try string parsers
    auto& stringParsers = GetStringParsers();
    if (auto it = stringParsers.find(key); it != stringParsers.end()) {
        it->second(this, value);
        return;
    }

    // Try menu label parsers
    auto& menuParsers = GetMenuLabelParsers();
    if (auto it = menuParsers.find(key); it != menuParsers.end()) {
        it->second(this, value);
        return;
    }

    // Handle special multi-value keys
    ParseSpecialKeys(key, value);
}

/**
 * @brief Parse special configuration keys that require custom handling
 * 
 * @param key The configuration key
 * @param value The configuration value
 */
void ConfigManager::ParseSpecialKeys(const std::string& key, const std::string& value) {
    // Smart levels (fan curve points)
    if (key == "level") {
        SmartLevel sl;
        if (sscanf_s(value.c_str(), "%d %d %d %d", 
                     &sl.temp, &sl.fan, &sl.hystUp, &sl.hystDown) >= 2) {
            SmartLevels1.push_back(sl);
        }
        return;
    }

    if (key == "level2") {
        SmartLevel sl;
        if (sscanf_s(value.c_str(), "%d %d %d %d", 
                     &sl.temp, &sl.fan, &sl.hystUp, &sl.hystDown) >= 2) {
            SmartLevels2.push_back(sl);
        }
        return;
    }

    // Fan beep settings
    if (key == "fanbeep") {
        sscanf_s(value.c_str(), "%d %d", &FanBeepFreq, &FanBeepDura);
        return;
    }

    // Icon temperature levels
    if (key == "iconlevels") {
        sscanf_s(value.c_str(), "%d %d %d", 
                 &IconLevels[0], &IconLevels[1], &IconLevels[2]);
        return;
    }

    // Hotkey configurations
    if (key.find("HK_") == 0) {
        ParseHotkeyByName(key, value);
        return;
    }

    // Sensor offset configurations
    if (key.find("SensorOffset") == 0) {
        int idx = std::stoi(key.substr(12)) - 1;
        if (idx >= 0 && idx < 16) {
            sscanf_s(value.c_str(), "%d %d %d", 
                     &SensorOffsets[idx].offset, 
                     &SensorOffsets[idx].hystMin, 
                     &SensorOffsets[idx].hystMax);
        }
        return;
    }
}

/**
 * @brief Parse hotkey configuration by key name
 * 
 * @param key The hotkey key name (e.g., "HK_BIOS")
 * @param value The hotkey value string
 */
void ConfigManager::ParseHotkeyByName(const std::string& key, const std::string& value) {
    // Hotkey dispatch table
    static const std::unordered_map<std::string, Hotkey ConfigManager::*> hotkeyMap = {
        {"HK_BIOS",   &ConfigManager::HK_BIOS},
        {"HK_Manual", &ConfigManager::HK_Manual},
        {"HK_Smart",  &ConfigManager::HK_Smart},
        {"HK_SM1",    &ConfigManager::HK_SM1},
        {"HK_SM2",    &ConfigManager::HK_SM2},
        {"HK_TG_BS",  &ConfigManager::HK_TG_BS},
        {"HK_TG_BM",  &ConfigManager::HK_TG_BM},
        {"HK_TG_MS",  &ConfigManager::HK_TG_MS},
        {"HK_TG_12",  &ConfigManager::HK_TG_12},
    };

    auto it = hotkeyMap.find(key);
    if (it != hotkeyMap.end()) {
        ParseHotkey(value, key, this->*(it->second));
    }
}

/**
 * @brief Parse a hotkey value string
 * 
 * Format: "method key" (e.g., "5 F10")
 * 
 * @param value The hotkey value string
 * @param prefix The hotkey prefix (for logging)
 * @param hk Reference to the hotkey to populate
 */
void ConfigManager::ParseHotkey(const std::string& value, const std::string& prefix, Hotkey& hk) {
    if (value.length() < 3) return;

    hk.method = value[0] - '0';
    hk.key = value[2];

    // Handle F-keys (F1-F12)
    if (hk.key == 'F' && value.length() >= 4) {
        int fnum = std::stoi(value.substr(3));
        hk.key = 0x70 + fnum - 1;  // VK_F1 = 0x70
    }
}

/**
 * @brief Save configuration to file
 * 
 * @param filename The output file path
 * @return true if successful, false otherwise
 */
bool ConfigManager::SaveConfig(const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) return false;

    // Header
    file << "; TPFanCtrl2 Configuration\n";
    file << "; Generated by TPFanCtrl2\n\n";

    // General Settings
    file << "; === General Settings ===\n";
    file << "Active=" << ActiveMode << "\n";
    file << "ManFanSpeed=" << ManFanSpeed << "\n";
    file << "cycle=" << Cycle << "\n";
    file << "StartMinimized=" << StartMinimized << "\n";
    file << "MinimizeToSysTray=" << MinimizeToSysTray << "\n";
    file << "MinimizeOnClose=" << MinimizeOnClose << "\n";
    file << "ShowBiasedTemps=" << ShowBiasedTemps << "\n";
    file << "NoExtSensor=" << NoExtSensor << "\n";
    file << "UseTWR=" << UseTWR << "\n";
    file << "IgnoreSensors=" << IgnoreSensors << "\n";
    file << "Language=" << Language << "\n";
    
    // PID Settings
    file << "\n; === PID Control Settings ===\n";
    file << "ControlAlgorithm=" << ControlAlgorithm << "\n";
    file << "PID_Target=" << PID_Target << "\n";
    file << "PID_Kp=" << PID_Kp << "\n";
    file << "PID_Ki=" << PID_Ki << "\n";
    file << "PID_Kd=" << PID_Kd << "\n";

    // Smart Levels (Fan Curve)
    file << "\n; === Smart Levels (Fan Curve) ===\n";
    file << "; Format: level=temp fan hystUp hystDown\n";
    for (const auto& sl : SmartLevels1) {
        file << "level=" << sl.temp << " " << sl.fan << " " 
             << sl.hystUp << " " << sl.hystDown << "\n";
    }

    // Smart Levels 2 (Secondary Fan Curve)
    if (!SmartLevels2.empty()) {
        file << "\n; === Smart Levels 2 (Secondary Fan) ===\n";
        for (const auto& sl : SmartLevels2) {
            file << "level2=" << sl.temp << " " << sl.fan << " " 
                 << sl.hystUp << " " << sl.hystDown << "\n";
        }
    }

    file << "\n; === End of Config ===\n";
    return true;
}
