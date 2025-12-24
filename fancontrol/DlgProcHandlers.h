#pragma once

// DlgProcHandlers.h - Extracted message handlers for DlgProc
// These functions reduce the complexity of the main DlgProc switch statement

#include <windows.h>
#include <format>
#include <string>

namespace DlgHandlers {

// Timer IDs for WM_TIMER handling
namespace TimerID {
    constexpr int UpdateFanState = 1;
    constexpr int UpdateWindowTitle = 2;
    constexpr int NamedPipeSession = 3;
    constexpr int RenewTempIcon = 4;
}

// Hotkey IDs for WM_HOTKEY handling
namespace HotkeyID {
    constexpr int SetBIOS = 1;
    constexpr int SetSmart = 2;
    constexpr int SetManual = 3;
    constexpr int SmartMode1 = 4;
    constexpr int SmartMode2 = 5;
    constexpr int ToggleBIOSSmart = 6;
    constexpr int ToggleBIOSManual = 7;
    constexpr int ToggleManualSmart = 8;
    constexpr int ToggleSmartModes = 9;
}

// Command IDs for WM_COMMAND handling
namespace CommandID {
    // Mode selection
    constexpr int ModeToggleFull = 7001;
    constexpr int ModeToggleMini = 7002;
    
    // Menu items
    constexpr int ShowAbout = 5001;
    constexpr int ApplySettings = 5002;
    constexpr int ExitApp = 5020;
    
    // Mode radio buttons (these need to match resource IDs)
    // Add more as needed based on actual resource definitions
}

// Icon color indices (for taskbar icon)
namespace IconColor {
    constexpr int Gray = 10;    // BIOS mode
    constexpr int Blue = 11;    // Active mode (base)
    constexpr int Green = 12;   // Cool temperature
    constexpr int Yellow = 13;  // Warm temperature
    constexpr int Red = 14;     // Hot temperature
}

// Build temperature display string
inline std::string BuildTempDisplayString(int maxTemp, bool useFahrenheit) {
    int displayTemp = useFahrenheit ? (maxTemp * 9 / 5 + 32) : maxTemp;
    return std::format("{}", displayTemp);
}

// Build status message for Named Pipe communication
inline std::string BuildPipeStatusMessage(int mode, int temp, const std::string& sensorName, 
                                          int iconColor, int fan1Speed, int fanCtrl) {
    return std::format("{} {} {} {} {} {} ", mode, temp, sensorName, iconColor, fan1Speed, fanCtrl);
}

// Calculate icon color based on temperature and defined levels
inline int GetIconColorForTemp(int maxTemp, const std::vector<int>& iconLevels, int baseGrayIcon = 10) {
    int iconOffset = 1;  // Start at blue
    
    for (size_t i = 0; i < iconLevels.size(); i++) {
        if (maxTemp >= iconLevels[i]) {
            iconOffset = static_cast<int>(i) + 1;
        } else {
            break;
        }
    }
    
    return baseGrayIcon + iconOffset;
}

} // namespace DlgHandlers
