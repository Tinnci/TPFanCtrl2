#include "ConfigManager.h"
#include <fstream>
#include <sstream>
#include <algorithm>

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

void ConfigManager::ParseLine(const std::string& line) {
    if (line.empty() || line[0] == '/' || line[0] == '#' || line[0] == ';') return;

    auto pos = line.find('=');
    if (pos == std::string::npos) return;

    std::string key = line.substr(0, pos);
    std::string value = line.substr(pos + 1);

    // Trim whitespace
    key.erase(std::remove_if(key.begin(), key.end(), isspace), key.end());
    
    if (key == "Active") ActiveMode = std::stoi(value);
    else if (key == "ManFanSpeed") ManFanSpeed = std::stoi(value);
    else if (key == "ProcessPriority") ProcessPriority = std::stoi(value);
    else if (key == "cycle") Cycle = std::stoi(value);
    else if (key == "IconCycle") IconCycle = std::stoi(value);
    else if (key == "ReIcCycle") ReIcCycle = std::stoi(value);
    else if (key == "IconFontSize") IconFontSize = std::stoi(value);
    else if (key == "FanSpeedLowByte") FanSpeedLowByte = std::stoi(value, nullptr, 0);
    else if (key == "NoExtSensor") NoExtSensor = std::stoi(value);
    else if (key == "SlimDialog") SlimDialog = std::stoi(value) != 0 ? 1 : 0;
    else if (key == "NoWaitMessage") NoWaitMessage = std::stoi(value);
    else if (key == "StartMinimized") StartMinimized = std::stoi(value);
    else if (key == "NoBallons") NoBallons = std::stoi(value);
    else if (key == "IconColorFan") IconColorFan = std::stoi(value);
    else if (key == "Lev64Norm") Lev64Norm = std::stoi(value);
    else if (key == "BluetoothEDR") BluetoothEDR = std::stoi(value);
    else if (key == "ManModeExit") ManModeExit = std::stoi(value);
    else if (key == "ShowBiasedTemps") ShowBiasedTemps = std::stoi(value);
    else if (key == "MaxReadErrors") MaxReadErrors = std::stoi(value);
    else if (key == "SecWinUptime") SecWinUptime = std::stoi(value);
    else if (key == "SecStartDelay") SecStartDelay = std::stoi(value);
    else if (key == "Log2File") Log2File = std::stoi(value);
    else if (key == "StayOnTop") StayOnTop = std::stoi(value);
    else if (key == "Log2csv") Log2csv = std::stoi(value);
    else if (key == "ShowAll") ShowAll = std::stoi(value);
    else if (key == "ShowTempIcon") ShowTempIcon = std::stoi(value);
    else if (key == "Fahrenheit") Fahrenheit = std::stoi(value);
    else if (key == "MinimizeToSysTray") MinimizeToSysTray = std::stoi(value);
    else if (key == "MinimizeOnClose") MinimizeOnClose = std::stoi(value);
    else if (key == "UseTWR") UseTWR = std::stoi(value);
    else if (key == "IgnoreSensors") IgnoreSensors = value;
    else if (key == "MenuLabelSM1") MenuLabelSM1 = value.substr(0, value.find('/'));
    else if (key == "MenuLabelSM2") MenuLabelSM2 = value.substr(0, value.find('/'));
    else if (key == "level") {
        SmartLevel sl;
        if (sscanf_s(value.c_str(), "%d %d %d %d", &sl.temp, &sl.fan, &sl.hystUp, &sl.hystDown) >= 2) {
            SmartLevels1.push_back(sl);
        }
    }
    else if (key == "level2") {
        SmartLevel sl;
        if (sscanf_s(value.c_str(), "%d %d %d %d", &sl.temp, &sl.fan, &sl.hystUp, &sl.hystDown) >= 2) {
            SmartLevels2.push_back(sl);
        }
    }
    else if (key == "fanbeep") {
        sscanf_s(value.c_str(), "%d %d", &FanBeepFreq, &FanBeepDura);
    }
    else if (key == "iconlevels") {
        sscanf_s(value.c_str(), "%d %d %d", &IconLevels[0], &IconLevels[1], &IconLevels[2]);
    }
    else if (key.find("HK_") == 0) {
        if (key == "HK_BIOS") ParseHotkey(value, "HK_BIOS", HK_BIOS);
        else if (key == "HK_Manual") ParseHotkey(value, "HK_Manual", HK_Manual);
        else if (key == "HK_Smart") ParseHotkey(value, "HK_Smart", HK_Smart);
        else if (key == "HK_SM1") ParseHotkey(value, "HK_SM1", HK_SM1);
        else if (key == "HK_SM2") ParseHotkey(value, "HK_SM2", HK_SM2);
        else if (key == "HK_TG_BS") ParseHotkey(value, "HK_TG_BS", HK_TG_BS);
        else if (key == "HK_TG_BM") ParseHotkey(value, "HK_TG_BM", HK_TG_BM);
        else if (key == "HK_TG_MS") ParseHotkey(value, "HK_TG_MS", HK_TG_MS);
        else if (key == "HK_TG_12") ParseHotkey(value, "HK_TG_12", HK_TG_12);
    }
    else if (key.find("SensorOffset") == 0) {
        int idx = std::stoi(key.substr(12)) - 1;
        if (idx >= 0 && idx < 16) {
            sscanf_s(value.c_str(), "%d %d %d", &SensorOffsets[idx].offset, &SensorOffsets[idx].hystMin, &SensorOffsets[idx].hystMax);
        }
    }
}

void ConfigManager::ParseHotkey(const std::string& value, const std::string& prefix, Hotkey& hk) {
    // Original logic: HK_BIOS=5 F10
    // buf[8] - 0x30 is method
    // buf[10] is key
    if (value.length() >= 3) {
        hk.method = value[0] - '0';
        hk.key = value[2];
        // Special handling for F-keys if needed (original code had some)
        if (hk.key == 'F' && value.length() >= 4) {
            // This is a bit simplified compared to original but follows the pattern
            int fnum = std::stoi(value.substr(3));
            hk.key = 0x70 + fnum - 1;
        }
    }
}
