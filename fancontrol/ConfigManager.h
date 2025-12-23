#pragma once

#include <string>
#include <vector>
#include <map>
#include "CommonTypes.h"

class ConfigManager {
public:
    ConfigManager();
    bool LoadConfig(const std::string& filename);

    // Configuration Variables
    int ActiveMode = 0;
    int ManFanSpeed = 7;
    int ProcessPriority = 2;
    int Cycle = 5;
    int IconCycle = 1;
    int ReIcCycle = 0;
    int IconFontSize = 8;
    std::string MenuLabelSM1 = "Smart Level 1";
    std::string MenuLabelSM2 = "Smart Level 2";
    int FanSpeedLowByte = 0x84;
    int NoExtSensor = 0;
    int SlimDialog = 0;
    int FanBeepFreq = 440;
    int FanBeepDura = 50;
    int NoWaitMessage = 1;
    int StartMinimized = 0;
    int NoBallons = 0;
    int IconColorFan = 0;
    int Lev64Norm = 0;
    int BluetoothEDR = 0;
    int ManModeExit = 80;
    int ShowBiasedTemps = 0;
    int MaxReadErrors = 10;
    int SecWinUptime = 0;
    int SecStartDelay = 0;
    int Log2File = 0;
    int StayOnTop = 0;
    int Log2csv = 0;
    int ShowAll = 0;
    int ShowTempIcon = 1;
    int Fahrenheit = 0;
    int MinimizeToSysTray = 1;
    int MinimizeOnClose = 1;
    int UseTWR = 0;

    // PID Settings
    float PID_Target = 60.0f;
    float PID_Kp = 0.5f;
    float PID_Ki = 0.01f;
    float PID_Kd = 0.1f;
    int ControlAlgorithm = 0; // 0: Step, 1: PID

    std::vector<SmartLevel> SmartLevels1;
    std::vector<SmartLevel> SmartLevels2;
    std::vector<int> IconLevels = {50, 55, 60};
    std::vector<SensorOffset> SensorOffsets;
    std::string IgnoreSensors = "";

    // Hotkeys
    struct Hotkey {
        int method;
        int key;
    };
    Hotkey HK_BIOS = {0, 0};
    Hotkey HK_Manual = {0, 0};
    Hotkey HK_Smart = {0, 0};
    Hotkey HK_SM1 = {0, 0};
    Hotkey HK_SM2 = {0, 0};
    Hotkey HK_TG_BS = {0, 0};
    Hotkey HK_TG_BM = {0, 0};
    Hotkey HK_TG_MS = {0, 0};
    Hotkey HK_TG_12 = {0, 0};

    bool SaveConfig(const std::string& filename);

private:
    void ParseLine(const std::string& line);
    void ParseHotkey(const std::string& line, const std::string& prefix, Hotkey& hk);
};
