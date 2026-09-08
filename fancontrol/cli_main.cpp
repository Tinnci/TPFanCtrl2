#include "_prec.h"

#include "ECManager.h"
#include "FanController.h"
#include "SensorManager.h"
#include "PawnIOProvider.h"
#include "Version.h"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#include <nlohmann/json.hpp>

namespace {

std::atomic_bool g_stopRequested{false};

BOOL WINAPI ConsoleHandler(DWORD signal) {
    switch (signal) {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
        g_stopRequested.store(true);
        return TRUE;
    default:
        return FALSE;
    }
}

void PrintUsage() {
    std::cout << "TPFanCtrl2 CLI " << AppVersion::GetFullVersionString() << "\n\n"
              << R"(Usage:
  TPFanCtrl2-cli.exe status [--dualfan] [--json] [--backend <auto|pawnio>]
  TPFanCtrl2-cli.exe fan --level <0-7> [--dualfan] [--duration <seconds>]
  TPFanCtrl2-cli.exe fan --level1 <0-7> --level2 <0-7> [--duration <seconds>]
  TPFanCtrl2-cli.exe mode auto [--dualfan]
  TPFanCtrl2-cli.exe --version

Commands:
  status                 Read hardware temperatures, fan RPMs, and current EC level.
  fan --level <0-7>     Apply a manual fan level (use --dualfan for both fans).
  fan --level1 <0-7> --level2 <0-7>
                         Independently set Fan 1 and Fan 2 speeds (dual-fan laptops).
  mode auto              Return fan control to the EC firmware automatic curve.

Options:
  --dualfan              Enable dual-fan control and speed monitoring (e.g. ThinkPad Z13/P1/X1E).
  --duration <seconds>   Restore EC automatic control after the specified duration.
                         Without it, press Ctrl+C to restore EC automatic control.
  --backend <name>       Choose I/O backend: auto (default) or pawnio.
  --json                 Print output as structured JSON.
  --help                 Show this help.

The CLI requires an elevated PowerShell/Command Prompt and the signed
PawnIO driver installed ('winget install namazso.PawnIO').
)";
}

bool ParseInt(std::string_view text, int& value) {
    if (text.empty()) return false;
    const char* first = text.data();
    const char* last = first + text.size();
    auto result = std::from_chars(first, last, value);
    return result.ec == std::errc{} && result.ptr == last;
}

bool HasArg(int argc, char** argv, std::string_view name) {
    for (int i = 1; i < argc; ++i) {
        if (argv[i] && std::string_view(argv[i]) == name) return true;
    }
    return false;
}

bool GetIntArg(int argc, char** argv, std::string_view name, int& value) {
    for (int i = 1; i + 1 < argc; ++i) {
        if (argv[i] && std::string_view(argv[i]) == name) {
            return ParseInt(argv[i + 1], value);
        }
    }
    return false;
}

bool GetStringArg(int argc, char** argv, std::string_view name, std::string_view& value) {
    for (int i = 1; i + 1 < argc; ++i) {
        if (argv[i] && std::string_view(argv[i]) == name) {
            value = argv[i + 1];
            return true;
        }
    }
    return false;
}

struct HardwareSession {
    std::shared_ptr<IIOProvider> io;
    std::shared_ptr<ECManager> ec;
    std::unique_ptr<FanController> fan;
    std::unique_ptr<SensorManager> sensor;
    std::string backendName;

    bool Open(std::string_view preferredBackend = "") {
        (void)preferredBackend; // Reserved for future backends
        auto pawn = std::make_shared<PawnIOProvider>([](const char* msg) {
            std::cerr << "[PawnIO] " << msg << '\n';
        });
        if (pawn->Initialize()) {
            io = pawn;
            backendName = "PawnIO";
        } else {
            std::cerr << "Failed to initialize PawnIO hardware I/O backend.\n"
                      << "Please run as Administrator and ensure PawnIO driver is installed ('winget install namazso.PawnIO').\n";
            return false;
        }

        std::cerr << "[Backend] Active hardware I/O backend: " << backendName << '\n';
        ec = std::make_shared<ECManager>(io, [](const char* message) {
            std::cerr << "[EC] " << message << '\n';
        });
        fan = std::make_unique<FanController>(ec);
        sensor = std::make_unique<SensorManager>(ec);

        // Populate standard ThinkPad sensor names
        static const char* defaultNames[] = {
            "CPU", "APS", "PCM", "GPU", "BAT1", "X7D", 
            "BAT2", "X7F", "BUS", "PCI", "PWR", "XC3"
        };
        for (int i = 0; i < 12; ++i) {
            sensor->SetSensorName(i, defaultNames[i]);
        }
        return true;
    }

    ~HardwareSession() {
        sensor.reset();
        fan.reset();
        ec.reset();
        io.reset();
    }
};

int PrintStatus(FanController& fan, SensorManager* sensor, bool json, bool dualFan) {
    if (dualFan) {
        fan.SetDualFanMode(true);
    }
    fan.RefreshCurrentLevel();
    int fan1 = 0;
    int fan2 = 0;
    const bool speedOk = fan.GetFanSpeeds(fan1, fan2);
    const int level = fan.GetCurrentLevel();

    int maxTemp = 0;
    int maxIndex = -1;
    std::vector<SensorData> activeSensors;
    if (sensor) {
        sensor->UpdateSensors(false, false, false);
        maxTemp = sensor->GetMaxTemp(maxIndex, "");
        for (const auto& s : sensor->GetSensors()) {
            if (s.isAvailable && s.rawTemp > 0 && s.rawTemp < 128) {
                activeSensors.push_back(s);
            }
        }
    }
    std::string maxSensorName = (maxIndex >= 0 && maxIndex < (int)sensor->GetSensors().size())
        ? sensor->GetSensor(maxIndex).name : "Unknown";

    if (json) {
        nlohmann::json j;
        j["level"] = level;
        j["fan1_rpm"] = fan1;
        j["fan2_rpm"] = fan2;
        j["dual_fan"] = dualFan || (fan2 > 0);
        j["speed_read_ok"] = speedOk;
        j["max_temp"] = maxTemp;
        j["max_sensor"] = maxSensorName;
        j["sensors"] = nlohmann::json::array();
        for (const auto& s : activeSensors) {
            j["sensors"].push_back({
                {"name", s.name},
                {"temp", s.rawTemp}
            });
        }
        std::cout << j.dump(2) << '\n';
    } else {
        std::cout << "EC Fan Status:\n"
                  << "  Current level: " << level << " (0x" << std::hex << level << std::dec << ")\n"
                  << "  Fan 1 speed:   " << fan1 << " RPM\n";
        if (dualFan || fan2 > 0) {
            std::cout << "  Fan 2 speed:   " << fan2 << " RPM" << (dualFan ? " (Dual-fan)" : "") << "\n";
        }
        std::cout << "  Speed read:    " << (speedOk ? "ok" : "failed") << "\n\n";

        std::cout << "Temperatures (Max: " << maxTemp << "\xC2\xB0\x43 [" << maxSensorName << "]):\n";
        int count = 0;
        for (const auto& s : activeSensors) {
            std::cout << "  " << s.name << ": " << s.rawTemp << "\xC2\xB0\x43";
            if (++count % 4 == 0) {
                std::cout << '\n';
            } else {
                std::cout << "    ";
            }
        }
        if (count % 4 != 0) std::cout << '\n';
    }
    return speedOk ? 0 : 1;
}

int RestoreECAuto(FanController& fan, bool dualFan) {
    fan.SetDualFanMode(dualFan);
    if (!fan.SetFanLevel(0x80, dualFan)) {
        std::cerr << "Failed to restore EC automatic fan control.\n";
        return 1;
    }
    std::cout << "Fan control returned to EC automatic mode (0x80)" << (dualFan ? " [Dual-fan]" : "") << ".\n";
    return 0;
}

int RunManual(FanController& fan, int level1, int level2, bool dualFan, int durationSeconds) {
    if (level1 < 0 || level1 > 7) {
        std::cerr << "Manual level must be between 0 and 7.\n";
        return 2;
    }
    if (level2 >= 0 && (level2 < 0 || level2 > 7)) {
        std::cerr << "Fan 2 manual level must be between 0 and 7.\n";
        return 2;
    }
    if (durationSeconds < 0) {
        std::cerr << "Duration cannot be negative.\n";
        return 2;
    }

    fan.SetDualFanMode(dualFan || level2 >= 0);
    bool setOk = false;
    if (level2 >= 0) {
        setOk = fan.SetFanLevels(level1, level2);
        if (setOk) {
            std::cout << "Fan 1 level set to " << level1 << ", Fan 2 level set to " << level2 << ".\n";
        }
    } else {
        setOk = fan.SetFanLevel(level1, dualFan);
        if (setOk) {
            std::cout << "Fan level set to " << level1 << (dualFan ? " (Both Fan 1 & Fan 2)" : "") << ".\n";
        }
    }

    if (!setOk) {
        std::cerr << "Failed to set fan level.\n";
        return 1;
    }

    std::cout << (durationSeconds > 0
        ? "Press Ctrl+C or wait for the duration to restore EC automatic control.\n"
        : "Press Ctrl+C to restore EC automatic control.\n");

    SetConsoleCtrlHandler(ConsoleHandler, TRUE);
    const auto deadline = std::chrono::steady_clock::now() +
        std::chrono::seconds(durationSeconds > 0 ? durationSeconds : INT_MAX);
    while (!g_stopRequested.load() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    SetConsoleCtrlHandler(ConsoleHandler, FALSE);
    return RestoreECAuto(fan, dualFan || level2 >= 0);
}

} // namespace

int main(int argc, char** argv) {
    if (HasArg(argc, argv, "--version") || HasArg(argc, argv, "-v")) {
        std::cout << "TPFanCtrl2 CLI " << AppVersion::GetFullVersionString() << "\n";
        return 0;
    }

    if (argc < 2 || HasArg(argc, argv, "--help") || HasArg(argc, argv, "-h")) {
        PrintUsage();
        return argc < 2 ? 2 : 0;
    }

    if (!HasArg(argc, argv, "--json")) {
        std::cerr << "TPFanCtrl2 CLI " << AppVersion::GetFullVersionString() << "\n";
    }

    std::string_view backend;
    GetStringArg(argc, argv, "--backend", backend);

    HardwareSession hardware;
    if (!hardware.Open(backend)) return 1;

    const bool dualFan = HasArg(argc, argv, "--dualfan");

    const std::string_view command(argv[1]);
    if (command == "status") {
        return PrintStatus(*hardware.fan, hardware.sensor.get(), HasArg(argc, argv, "--json"), dualFan);
    }
    if (command == "mode" && argc >= 3 && std::string_view(argv[2]) == "auto") {
        return RestoreECAuto(*hardware.fan, dualFan);
    }
    if (command == "fan") {
        int level = -1;
        int level1 = -1;
        int level2 = -1;
        int duration = 0;

        GetIntArg(argc, argv, "--level", level);
        GetIntArg(argc, argv, "--level1", level1);
        GetIntArg(argc, argv, "--level2", level2);

        if (level < 0 && level1 < 0) {
            std::cerr << "Missing or invalid --level (or --level1/--level2).\n";
            return 2;
        }

        if (level >= 0 && level1 < 0) {
            level1 = level;
            if (dualFan && level2 < 0) {
                // If dualfan is active without explicit level2, apply same level to both
                level2 = -1; // handled by SetFanLevel(level, true)
            }
        }

        if (HasArg(argc, argv, "--duration") &&
            !GetIntArg(argc, argv, "--duration", duration)) {
            std::cerr << "Missing or invalid --duration.\n";
            return 2;
        }
        return RunManual(*hardware.fan, level1, level2, dualFan, duration);
    }

    std::cerr << "Unknown command. Use --help for usage.\n";
    return 2;
}
