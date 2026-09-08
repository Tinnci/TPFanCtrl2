#include "_prec.h"

#include "ECManager.h"
#include "FanController.h"
#include "SensorManager.h"
#include "PawnIOProvider.h"
#include "Version.h"

#include <windows.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <format>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#include <ftxui/component/captured_mouse.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

using namespace ftxui;

namespace {

std::atomic_bool g_running{true};
FanController* g_fanController = nullptr;
bool g_dualFanMode = true;
ScreenInteractive* g_pScreen = nullptr;

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
        if (g_pScreen) {
            g_pScreen->ExitLoopClosure()();
        }
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
std::string MakeSparkline(const std::vector<int>& history, int maxVal = 7500, size_t width = 14) {
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

Color GetTempColor(int temp) {
    if (temp < 50) return Color::RGB(70, 230, 130);       // Cool Green
    if (temp < 70) return Color::RGB(250, 220, 60);       // Warm Yellow
    if (temp < 85) return Color::RGB(255, 140, 40);       // Hot Orange
    return Color::RGB(255, 65, 65);                       // Alert Red
}

Color GetFanColor(float pct) {
    if (pct < 0.35f) return Color::RGB(80, 200, 255);    // Soft Cyan
    if (pct < 0.70f) return Color::RGB(70, 230, 130);    // Green
    if (pct < 0.90f) return Color::RGB(255, 180, 40);    // Orange
    return Color::RGB(255, 75, 75);                       // High Red
}

struct TopDataSnapshot {
    std::string systemModel;
    int currentLevel{0x80};
    int manualLevel{-1};
    int fan1Rpm{0};
    int fan2Rpm{0};
    std::vector<int> fan1History;
    std::vector<int> fan2History;
    int maxTemp{0};
    std::string maxSensorName{"N/A"};
    std::vector<SensorData> activeSensors;
    std::string statusNotice{"Ready. Press [0-7] or click buttons to adjust fan level."};
    std::chrono::steady_clock::time_point statusNoticeUntil{std::chrono::steady_clock::now() + std::chrono::seconds(5)};
    bool dualFanMode{true};
};

} // namespace

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

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

    TopDataSnapshot data;
    data.systemModel = GetSystemModel();
    data.dualFanMode = g_dualFanMode;
    std::mutex dataMutex;

    auto screen = ScreenInteractive::Fullscreen();
    g_pScreen = &screen;

    // Action Helpers
    auto onSetLevel = [&](int lvl) {
        std::lock_guard<std::mutex> lock(dataMutex);
        data.manualLevel = lvl;
        fan->SetDualFanMode(data.dualFanMode);
        fan->SetFanLevel(lvl, data.dualFanMode);
        data.statusNotice = std::format("Manual level set to {} (Dual-fan: {})", lvl, data.dualFanMode ? "ON" : "OFF");
        data.statusNoticeUntil = std::chrono::steady_clock::now() + std::chrono::seconds(4);
        screen.PostEvent(Event::Custom);
    };

    auto onAuto = [&]() {
        std::lock_guard<std::mutex> lock(dataMutex);
        SafeRestoreEC();
        data.manualLevel = -1;
        data.statusNotice = "Restored EC firmware automatic thermal curve (0x80).";
        data.statusNoticeUntil = std::chrono::steady_clock::now() + std::chrono::seconds(4);
        screen.PostEvent(Event::Custom);
    };

    auto onToggleDualFan = [&]() {
        std::lock_guard<std::mutex> lock(dataMutex);
        data.dualFanMode = !data.dualFanMode;
        g_dualFanMode = data.dualFanMode;
        fan->SetDualFanMode(data.dualFanMode);
        if (data.manualLevel >= 0) {
            fan->SetFanLevel(data.manualLevel, data.dualFanMode);
        }
        data.statusNotice = std::format("Dual-fan mode switched to: {}", data.dualFanMode ? "ENABLED" : "DISABLED");
        data.statusNoticeUntil = std::chrono::steady_clock::now() + std::chrono::seconds(4);
        screen.PostEvent(Event::Custom);
    };

    auto onRefresh = [&]() {
        screen.PostEvent(Event::Custom);
    };

    // Hardware Polling Background Worker Thread
    std::thread pollThread([&]() {
        while (g_running.load()) {
            {
                std::lock_guard<std::mutex> lock(dataMutex);

                fan->RefreshCurrentLevel();
                data.currentLevel = fan->GetCurrentLevel();

                int f1 = 0, f2 = 0;
                fan->GetFanSpeeds(f1, f2);
                data.fan1Rpm = f1;
                data.fan2Rpm = f2;
                data.fan1History.push_back(f1);
                data.fan2History.push_back(f2);
                if (data.fan1History.size() > 30) data.fan1History.erase(data.fan1History.begin());
                if (data.fan2History.size() > 30) data.fan2History.erase(data.fan2History.begin());

                sensor->UpdateSensors(false, false, false);
                int maxIndex = -1;
                data.maxTemp = sensor->GetMaxTemp(maxIndex, "");
                data.maxSensorName = (maxIndex >= 0 && maxIndex < (int)sensor->GetSensors().size())
                    ? sensor->GetSensor(maxIndex).name : "Unknown";

                // Safety Trip: if max temp >= 90°C and manual mode is active, force EC Auto
                if (data.maxTemp >= 90 && data.currentLevel != 0x80) {
                    SafeRestoreEC();
                    data.manualLevel = -1;
                    data.statusNotice = "CRITICAL ALERT: Temp >= 90°C! Auto mode forced for safety!";
                    data.statusNoticeUntil = std::chrono::steady_clock::now() + std::chrono::seconds(10);
                }

                data.activeSensors.clear();
                for (const auto& s : sensor->GetSensors()) {
                    if (s.isAvailable && s.rawTemp >= 15 && s.rawTemp < 128) {
                        data.activeSensors.push_back(s);
                    }
                }
            }

            screen.PostEvent(Event::Custom);

            // Sleep 1 second in small slices to respond promptly on shutdown
            for (int i = 0; i < 20 && g_running.load(); ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        }
    });

    // Control UI Buttons
    ButtonOption btnOpt = ButtonOption::Border();
    
    auto btnAuto = Button("Auto [A]", onAuto, btnOpt);
    auto btn0 = Button("0", [&] { onSetLevel(0); }, btnOpt);
    auto btn1 = Button("1", [&] { onSetLevel(1); }, btnOpt);
    auto btn2 = Button("2", [&] { onSetLevel(2); }, btnOpt);
    auto btn3 = Button("3", [&] { onSetLevel(3); }, btnOpt);
    auto btn4 = Button("4", [&] { onSetLevel(4); }, btnOpt);
    auto btn5 = Button("5", [&] { onSetLevel(5); }, btnOpt);
    auto btn6 = Button("6", [&] { onSetLevel(6); }, btnOpt);
    auto btn7 = Button("7", [&] { onSetLevel(7); }, btnOpt);
    auto btnDual = Button("Dual-Fan [D]", onToggleDualFan, btnOpt);
    auto btnRefresh = Button("Refresh [R]", onRefresh, btnOpt);
    auto btnQuit = Button("Quit [Q]", screen.ExitLoopClosure(), btnOpt);

    auto buttonsContainer = Container::Horizontal({
        btnAuto,
        btn0, btn1, btn2, btn3, btn4, btn5, btn6, btn7,
        btnDual,
        btnRefresh,
        btnQuit
    });

    // FTXUI Declarative Renderer
    auto renderer = Renderer(buttonsContainer, [&] {
        TopDataSnapshot snap;
        {
            std::lock_guard<std::mutex> lock(dataMutex);
            snap = data;
        }

        const auto themeBorder = Color::RGB(65, 115, 205);
        const auto themeCyan   = Color::RGB(80, 225, 255);
        const auto themeDim    = Color::RGB(130, 140, 160);

        // 1. Header Panel
        Element modeBadge;
        if (snap.currentLevel == 0x80) {
            modeBadge = hbox({
                text("[EC FIRMWARE AUTO 0x80]") | bold | color(Color::RGB(70, 230, 130))
            });
        } else {
            modeBadge = hbox({
                text(std::format("[MANUAL LEVEL {}]", snap.currentLevel)) | bold | color(Color::RGB(250, 210, 60))
            });
        }

        Element header = vbox({
            hbox({
                text(" TPFanCtrl2 Top ") | bold | color(themeCyan),
                text("│ ") | color(themeDim),
                text(snap.systemModel) | bold | color(Color::White),
                text(" │ ") | color(themeDim),
                text("v" TPFC_VERSION) | color(themeDim),
                filler(),
                modeBadge,
                text(" ")
            }),
            separator() | color(themeBorder),
            hbox({
                text("  Backend: ") | color(themeDim),
                text("PawnIO (WHQL Signed)") | bold | color(Color::White),
                text("  │  Poll: ") | color(themeDim),
                text("1.0s") | color(Color::White),
                text("  │  Dual-Fan Mux: ") | color(themeDim),
                text(snap.dualFanMode ? "ENABLED (0x31)" : "DISABLED") | bold | (snap.dualFanMode ? color(themeCyan) : color(themeDim)),
                filler()
            })
        }) | borderRounded | color(themeBorder);

        // 2. Fans & Tachometers Panel
        float f1Pct = std::clamp((float)snap.fan1Rpm / 7500.0f, 0.0f, 1.0f);
        float f2Pct = std::clamp((float)snap.fan2Rpm / 7500.0f, 0.0f, 1.0f);

        Element fan1Row = hbox({
            text("  Fan 1 (CPU) ") | bold | color(Color::White),
            gauge(f1Pct) | color(GetFanColor(f1Pct)) | size(WIDTH, EQUAL, 24),
            text(std::format(" {:4d} RPM  ", snap.fan1Rpm)) | bold | color(Color::White),
            text("Trend: ") | color(themeDim),
            text(MakeSparkline(snap.fan1History, 7500, 16)) | color(themeCyan)
        });

        Element fan2Row = hbox({
            text("  Fan 2 (GPU) ") | bold | color(Color::White),
            gauge(f2Pct) | color(GetFanColor(f2Pct)) | size(WIDTH, EQUAL, 24),
            text(std::format(" {:4d} RPM  ", snap.fan2Rpm)) | bold | color(Color::White),
            text("Trend: ") | color(themeDim),
            text(MakeSparkline(snap.fan2History, 7500, 16)) | color(themeCyan)
        });

        Elements fanRows;
        fanRows.push_back(hbox({
            text(" FANS & TACHOMETERS") | bold | color(themeCyan),
            filler()
        }));
        fanRows.push_back(separator() | color(themeBorder));
        fanRows.push_back(fan1Row);
        if (snap.dualFanMode || snap.fan2Rpm > 0) {
            fanRows.push_back(fan2Row);
        }

        Element fansPanel = vbox(std::move(fanRows)) | borderRounded | color(themeBorder);

        // 3. Thermal Sensors Panel
        Elements sensorGridRows;
        sensorGridRows.push_back(hbox({
            text(" THERMAL SENSORS ") | bold | color(themeCyan),
            text("│  Peak Hotspot: ") | color(themeDim),
            text(std::format("{}°C [{}]", snap.maxTemp, snap.maxSensorName)) | bold | color(GetTempColor(snap.maxTemp)),
            filler()
        }));
        sensorGridRows.push_back(separator() | color(themeBorder));

        for (size_t i = 0; i < snap.activeSensors.size(); i += 2) {
            Elements rowItems;
            rowItems.push_back(text("  "));

            // Col 1
            const auto& s1 = snap.activeSensors[i];
            float p1 = std::clamp((float)s1.rawTemp / 100.0f, 0.0f, 1.0f);
            rowItems.push_back(hbox({
                text(std::format("{:<4s} ", s1.name)) | bold | color(Color::White),
                gauge(p1) | color(GetTempColor(s1.rawTemp)) | size(WIDTH, EQUAL, 14),
                text(std::format(" {:2d}°C", s1.rawTemp)) | bold | color(GetTempColor(s1.rawTemp))
            }));

            // Col 2 (if exists)
            if (i + 1 < snap.activeSensors.size()) {
                const auto& s2 = snap.activeSensors[i + 1];
                float p2 = std::clamp((float)s2.rawTemp / 100.0f, 0.0f, 1.0f);
                rowItems.push_back(text("  │  ") | color(themeDim));
                rowItems.push_back(hbox({
                    text(std::format("{:<4s} ", s2.name)) | bold | color(Color::White),
                    gauge(p2) | color(GetTempColor(s2.rawTemp)) | size(WIDTH, EQUAL, 14),
                    text(std::format(" {:2d}°C", s2.rawTemp)) | bold | color(GetTempColor(s2.rawTemp))
                }));
            }
            rowItems.push_back(filler());
            sensorGridRows.push_back(hbox(std::move(rowItems)));
        }

        Element thermalsPanel = vbox(std::move(sensorGridRows)) | borderRounded | color(themeBorder);

        // 4. Interactive Controls & Status Bar Panel
        std::string notice = (std::chrono::steady_clock::now() < snap.statusNoticeUntil)
            ? snap.statusNotice : "Monitoring active. System sensors responding normally.";

        Element controlsPanel = vbox({
            hbox({
                text(" INTERACTIVE CONTROLS (Mouse Click or Hotkey)") | bold | color(themeCyan),
                filler()
            }),
            separator() | color(themeBorder),
            hbox({
                filler(),
                buttonsContainer->Render(),
                filler()
            }),
            separator() | color(themeBorder),
            hbox({
                text("  Status: ") | color(themeDim),
                text(notice) | bold | color(Color::RGB(215, 230, 255)),
                filler()
            }),
            hbox({
                text("  Shortcuts: [0-7] Set Level  │  [A] EC Auto  │  [D] Dual-Fan  │  [R] Refresh  │  [Q/Esc] Quit & Restore") | color(themeDim),
                filler()
            })
        }) | borderRounded | color(themeBorder);

        // Main responsive container
        return vbox({
            header,
            fansPanel,
            thermalsPanel | flex,
            controlsPanel
        });
    });

    // Keyboard Hotkey Interceptor via CatchEvent
    auto eventHandler = CatchEvent(renderer, [&](Event event) {
        if (event == Event::Character('q') || event == Event::Character('Q') || event == Event::Escape) {
            g_running.store(false);
            screen.ExitLoopClosure()();
            return true;
        }
        if (event == Event::Character('a') || event == Event::Character('A')) {
            onAuto();
            return true;
        }
        if (event == Event::Character('d') || event == Event::Character('D')) {
            onToggleDualFan();
            return true;
        }
        if (event == Event::Character('r') || event == Event::Character('R')) {
            onRefresh();
            return true;
        }
        if (event.is_character()) {
            char ch = event.character()[0];
            if (ch >= '0' && ch <= '7') {
                onSetLevel(ch - '0');
                return true;
            }
        }
        return false;
    });

    // Enter FTXUI interactive loop
    screen.Loop(eventHandler);

    // Shutdown and Safety Restoration
    g_running.store(false);
    if (pollThread.joinable()) {
        pollThread.join();
    }

    SafeRestoreEC();
    std::cout << "\nTPFanCtrl2 Top exited cleanly. Fan control safely returned to EC firmware (0x80).\n";

    return 0;
}
