#include "_prec.h"

#include "ECManager.h"
#include "FanController.h"
#include "SensorManager.h"
#include "PawnIOProvider.h"
#include "Version.h"

#include <windows.h>
#include <conio.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <format>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {

std::atomic_bool g_running{true};
FanController* g_fanController = nullptr;
bool g_dualFanMode = true;

// Safe shutdown: restore EC automatic curve (0x80) on both fan channels
void SafeRestoreEC() {
    if (g_fanController) {
        g_fanController->SetDualFanMode(g_dualFanMode);
        g_fanController->SetFanLevel(0x80, g_dualFanMode);
    }
}

BOOL WINAPI ConsoleHandler(DWORD signal) {
    switch (signal) {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
        g_running.store(false);
        SafeRestoreEC();
        std::cout << "\033[?1049l\033[?25h\033[0m" << std::flush;
        return TRUE;
    default:
        return FALSE;
    }
}

std::string GetSystemModel() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\BIOS", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t buffer[128] = {0};
        DWORD size = sizeof(buffer);
        if (RegQueryValueExW(hKey, L"SystemFamily", nullptr, nullptr, (LPBYTE)buffer, &size) == ERROR_SUCCESS && wcslen(buffer) > 0) {
            RegCloseKey(hKey);
            int len = WideCharToMultiByte(CP_UTF8, 0, buffer, -1, nullptr, 0, nullptr, nullptr);
            std::string model(len - 1, 0);
            WideCharToMultiByte(CP_UTF8, 0, buffer, -1, model.data(), len, nullptr, nullptr);
            return model;
        }
        size = sizeof(buffer);
        if (RegQueryValueExW(hKey, L"SystemProductName", nullptr, nullptr, (LPBYTE)buffer, &size) == ERROR_SUCCESS && wcslen(buffer) > 0) {
            RegCloseKey(hKey);
            int len = WideCharToMultiByte(CP_UTF8, 0, buffer, -1, nullptr, 0, nullptr, nullptr);
            std::string model(len - 1, 0);
            WideCharToMultiByte(CP_UTF8, 0, buffer, -1, model.data(), len, nullptr, nullptr);
            return model;
        }
        RegCloseKey(hKey);
    }
    return "ThinkPad";
}

// Sparkline generator (using Unicode block elements:  ▂▃▄▅▆▇█)
std::string MakeSparkline(const std::vector<int>& history, int maxVal = 8000, size_t width = 16) {
    static const char* const blocks[] = { " ", " ", "▂", "▃", "▄", "▅", "▆", "▇", "█" };
    std::string spark;
    if (history.empty()) return std::string(width, ' ');

    size_t start = (history.size() > width) ? history.size() - width : 0;
    while (history.size() - start < width) {
        spark += " ";
        width--;
    }

    for (size_t i = start; i < history.size(); ++i) {
        int v = (std::clamp)(history[i], 0, maxVal);
        int idx = (int)std::round((float)v / maxVal * 8.0f);
        idx = (std::clamp)(idx, 0, 8);
        spark += blocks[idx];
    }
    return spark;
}

// Generate colored progress bar: [██████░░░░░░]
std::string MakeBar(float percent, int barWidth = 20, bool isTemp = false, int tempVal = 0) {
    percent = (std::clamp)(percent, 0.0f, 1.0f);
    int filled = (int)std::round(percent * barWidth);
    filled = (std::clamp)(filled, 0, barWidth);

    std::string colorCode;
    if (isTemp) {
        if (tempVal < 50) colorCode = "\033[38;2;60;220;120m";       // Cool Green
        else if (tempVal < 70) colorCode = "\033[38;2;240;220;60m";  // Warm Yellow
        else if (tempVal < 85) colorCode = "\033[38;2;255;140;40m";  // Hot Orange
        else colorCode = "\033[38;2;255;60;60m\033[1m";             // Alert Red
    } else {
        if (percent < 0.35f) colorCode = "\033[38;2;60;200;240m";    // Cyan
        else if (percent < 0.70f) colorCode = "\033[38;2;60;220;120m"; // Green
        else if (percent < 0.90f) colorCode = "\033[38;2;255;180;40m"; // Orange
        else colorCode = "\033[38;2;255;70;70m";                     // Red
    }

    std::string out = colorCode + "[";
    for (int i = 0; i < filled; ++i) out += "█";
    out += "\033[38;2;70;75;90m"; // dim track
    for (int i = filled; i < barWidth; ++i) out += "░";
    out += colorCode + "]\033[0m";
    return out;
}

} // namespace

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    // Enable Virtual Terminal Processing for ANSI sequence support on Windows
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    if (GetConsoleMode(hOut, &dwMode)) {
        SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }

    // Initialize hardware backend (PawnIO)
    auto pawn = std::make_shared<PawnIOProvider>([](const char*) {});
    if (!pawn->Initialize()) {
        std::cerr << "Failed to initialize PawnIO hardware I/O backend.\n"
                  << "Please run TPFanCtrl2-top as Administrator with PawnIO installed ('winget install namazso.PawnIO').\n";
        return 1;
    }

    auto ec = std::make_shared<ECManager>(pawn, [](const char*) {});
    auto fan = std::make_unique<FanController>(ec);
    auto sensor = std::make_unique<SensorManager>(ec);

    g_fanController = fan.get();
    g_dualFanMode = true;
    fan->SetDualFanMode(g_dualFanMode);

    static const char* defaultNames[] = {
        "CPU", "APS", "PCM", "GPU", "BAT1", "X7D",
        "BAT2", "X7F", "BUS", "PCI", "PWR", "XC3"
    };
    for (int i = 0; i < 12; ++i) {
        sensor->SetSensorName(i, defaultNames[i]);
    }

    SetConsoleCtrlHandler(ConsoleHandler, TRUE);

    // Switch to Alternate Screen Buffer and hide cursor
    std::cout << "\033[?1049h\033[?25l\033[2J" << std::flush;

    std::string systemModel = GetSystemModel();
    std::string statusNotice = "Ready. Press [0-7] to set manual fan speed.";
    auto statusNoticeUntil = std::chrono::steady_clock::now() + std::chrono::seconds(4);

    std::vector<int> fan1History;
    std::vector<int> fan2History;
    fan1History.reserve(32);
    fan2History.reserve(32);

    int manualLevel = -1; // -1 means auto

    while (g_running.load()) {
        // 1. Fetch hardware status
        fan->RefreshCurrentLevel();
        int currentLevel = fan->GetCurrentLevel();

        int f1 = 0, f2 = 0;
        fan->GetFanSpeeds(f1, f2);
        fan1History.push_back(f1);
        fan2History.push_back(f2);
        if (fan1History.size() > 30) fan1History.erase(fan1History.begin());
        if (fan2History.size() > 30) fan2History.erase(fan2History.begin());

        sensor->UpdateSensors(false, false, false);
        int maxIndex = -1;
        int maxTemp = sensor->GetMaxTemp(maxIndex, "");
        std::string maxSensorName = (maxIndex >= 0 && maxIndex < (int)sensor->GetSensors().size())
            ? sensor->GetSensor(maxIndex).name : "Unknown";

        // Safety trip: if temperature exceeds 90°C, force automatic firmware recovery
        if (maxTemp >= 90 && currentLevel != 0x80) {
            SafeRestoreEC();
            manualLevel = -1;
            statusNotice = "CRITICAL ALERT: Temp >= 90°C! Auto mode forced for safety!";
            statusNoticeUntil = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        }

        // 2. Render TUI Frame
        std::string buf;
        buf.reserve(4096);
        buf += "\033[H"; // Cursor to top-left

        // Color Constants
        const std::string cBorder = "\033[38;2;65;110;190m";
        const std::string cTitle  = "\033[38;2;80;230;255m\033[1m";
        const std::string cWhite  = "\033[38;2;240;245;255m";
        const std::string cDim    = "\033[38;2;120;130;150m";
        const std::string cGreen  = "\033[38;2;70;230;130m";
        const std::string cYellow = "\033[38;2;250;220;60m";
        const std::string cRed    = "\033[38;2;255;70;70m\033[1m";
        const std::string cReset  = "\033[0m";

        // Title Header Bar
        std::string modeStr = (currentLevel == 0x80)
            ? (cGreen + "[EC FIRMWARE AUTO]" + cReset)
            : (cYellow + "[MANUAL LEVEL " + std::to_string(currentLevel) + "]" + cReset);

        buf += cBorder + "╭─ " + cTitle + "TPFanCtrl2 Top" + cBorder + " ── " + cWhite + systemModel + cBorder + " ── " + cDim + "v" + std::string(AppVersion::Version) + cBorder + " ── " + modeStr + cBorder + " ─────────────╮\n" + cReset;
        buf += cBorder + "│ " + cDim + "Backend: " + cWhite + "PawnIO (WHQL Signed)" + cDim + " │ Poll: " + cWhite + "1.0s" + cDim + " │ Arch: " + cWhite + (g_dualFanMode ? "Dual-Fan (0x31 Mux)" : "Single-Fan") + cBorder + "                │\n" + cReset;
        buf += cBorder + "├──────────────────────────────────────────────────────────────────────────────┤\n" + cReset;

        // Fan Section
        buf += cBorder + "│ " + cTitle + "🌀 FANS & TACHOMETERS" + cBorder + "                                                          │\n" + cReset;

        float f1Pct = (float)f1 / 7500.0f;
        std::string f1Bar = MakeBar(f1Pct, 18, false);
        std::string f1Spark = MakeSparkline(fan1History, 7500, 14);
        buf += cBorder + "│ " + cWhite + " Fan 1 (CPU) " + f1Bar + " " + std::format("{:4d} RPM", f1) + "  Trend: " + "\033[38;2;80;200;255m" + f1Spark + cReset + cBorder + "   │\n" + cReset;

        if (g_dualFanMode || f2 > 0) {
            float f2Pct = (float)f2 / 7500.0f;
            std::string f2Bar = MakeBar(f2Pct, 18, false);
            std::string f2Spark = MakeSparkline(fan2History, 7500, 14);
            buf += cBorder + "│ " + cWhite + " Fan 2 (GPU) " + f2Bar + " " + std::format("{:4d} RPM", f2) + "  Trend: " + "\033[38;2;80;200;255m" + f2Spark + cReset + cBorder + "   │\n" + cReset;
        }

        buf += cBorder + "├──────────────────────────────────────────────────────────────────────────────┤\n" + cReset;

        // Temperatures Section
        std::string maxColor = (maxTemp >= 85) ? cRed : ((maxTemp >= 70) ? cYellow : cGreen);
        buf += cBorder + "│ " + cTitle + "🌡️ TEMPERATURE SENSORS " + cDim + "(Peak Hotspot: " + maxColor + std::format("{}°C [{}]", maxTemp, maxSensorName) + cDim + ")" + cBorder + "                    │\n" + cReset;

        std::vector<SensorData> active;
        for (const auto& s : sensor->GetSensors()) {
            if (s.isAvailable && s.rawTemp >= 15 && s.rawTemp < 128) {
                active.push_back(s);
            }
        }

        for (size_t i = 0; i < active.size(); i += 2) {
            buf += cBorder + "│ ";
            // Col 1
            const auto& s1 = active[i];
            float p1 = (float)s1.rawTemp / 100.0f;
            std::string bar1 = MakeBar(p1, 12, true, s1.rawTemp);
            buf += std::format("{:4s} {} {:2d}°C ", s1.name, bar1, s1.rawTemp);

            // Col 2 (if exists)
            if (i + 1 < active.size()) {
                const auto& s2 = active[i + 1];
                float p2 = (float)s2.rawTemp / 100.0f;
                std::string bar2 = MakeBar(p2, 12, true, s2.rawTemp);
                buf += std::format("│ {:4s} {} {:2d}°C", s2.name, bar2, s2.rawTemp);
            } else {
                buf += "│                           ";
            }
            buf += cBorder + "   │\n" + cReset;
        }

        buf += cBorder + "├──────────────────────────────────────────────────────────────────────────────┤\n" + cReset;

        // Status & Hotkey Bar
        std::string notice = (std::chrono::steady_clock::now() < statusNoticeUntil) ? statusNotice : "Monitoring active. All sensors responsive.";
        buf += cBorder + "│ " + cDim + "Msg: " + cWhite + std::format("{:<70s}", notice.substr(0, 70)) + cBorder + " │\n" + cReset;
        buf += cBorder + "├──────────────────────────────────────────────────────────────────────────────┤\n" + cReset;
        buf += cBorder + "│ " + cTitle + "[0-7]" + cWhite + " Set Level  " + cTitle + "[A]" + cWhite + " EC Auto  " + cTitle + "[D]" + cWhite + " DualFan  " + cTitle + "[R]" + cWhite + " Refresh  " + cTitle + "[Q/Esc]" + cWhite + " Quit & Restore" + cBorder + "  │\n" + cReset;
        buf += cBorder + "╰──────────────────────────────────────────────────────────────────────────────╯\n" + cReset;

        std::cout << buf << std::flush;

        // 3. Sleep & Non-blocking Keyboard Handling
        for (int slice = 0; slice < 20; ++slice) {
            if (!g_running.load()) break;

            if (_kbhit()) {
                int ch = _getch();
                if (ch == 'q' || ch == 'Q' || ch == 27) { // Q or Esc
                    g_running.store(false);
                    break;
                } else if (ch >= '0' && ch <= '7') {
                    manualLevel = ch - '0';
                    fan->SetDualFanMode(g_dualFanMode);
                    fan->SetFanLevel(manualLevel, g_dualFanMode);
                    statusNotice = std::format("Manual level set to {} (Dual-fan: {})", manualLevel, g_dualFanMode ? "ON" : "OFF");
                    statusNoticeUntil = std::chrono::steady_clock::now() + std::chrono::seconds(4);
                    break;
                } else if (ch == 'a' || ch == 'A') {
                    SafeRestoreEC();
                    manualLevel = -1;
                    statusNotice = "Restored EC firmware automatic fan curve (0x80).";
                    statusNoticeUntil = std::chrono::steady_clock::now() + std::chrono::seconds(4);
                    break;
                } else if (ch == 'd' || ch == 'D') {
                    g_dualFanMode = !g_dualFanMode;
                    fan->SetDualFanMode(g_dualFanMode);
                    if (manualLevel >= 0) {
                        fan->SetFanLevel(manualLevel, g_dualFanMode);
                    }
                    statusNotice = std::format("Dual-fan mode switched to: {}", g_dualFanMode ? "ENABLED" : "DISABLED");
                    statusNoticeUntil = std::chrono::steady_clock::now() + std::chrono::seconds(4);
                    break;
                } else if (ch == 'r' || ch == 'R') {
                    break; // Trigger immediate re-render
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }

    // Cleanup: restore EC control, restore screen and cursor
    SafeRestoreEC();
    std::cout << "\033[?1049l\033[?25h\033[0m" << std::flush;
    std::cout << "TPFanCtrl2 Top exited cleanly. Fan control safely returned to EC firmware.\n";
    return 0;
}
