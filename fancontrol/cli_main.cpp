#include "_prec.h"

#include "ECManager.h"
#include "FanController.h"
#include "PawnIOProvider.h"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <thread>

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
    std::cout << R"(TPFanCtrl2 CLI

Usage:
  TPFanCtrl2-cli.exe status [--json] [--backend <auto|pawnio>]
  TPFanCtrl2-cli.exe fan --level <0-7> [--duration <seconds>] [--backend <auto|pawnio>]
  TPFanCtrl2-cli.exe mode auto [--backend <auto|pawnio>]

Commands:
  status                 Read current EC fan level and fan RPM.
  fan --level <0-7>     Apply a normal manual fan level.
  mode auto              Return fan control to the EC firmware curve.

Options:
  --duration <seconds>   Restore EC automatic control after the duration.
                         Without it, press Ctrl+C to restore EC automatic control.
  --backend <name>       Choose I/O backend: auto (default) or pawnio.
  --json                 Print status as JSON.
  --help                 Show this help.

The CLI requires an elevated PowerShell/Command Prompt and the signed
PawnIO driver installed. It never sends the legacy extreme value 0x40.
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
        return true;
    }

    ~HardwareSession() {
        fan.reset();
        ec.reset();
        io.reset();
    }
};

int PrintStatus(FanController& fan, bool json) {
    fan.RefreshCurrentLevel();
    int fan1 = 0;
    int fan2 = 0;
    const bool speedOk = fan.GetFanSpeeds(fan1, fan2);
    const int level = fan.GetCurrentLevel();

    if (json) {
        std::cout << "{\"level\":" << level
                  << ",\"fan1_rpm\":" << fan1
                  << ",\"fan2_rpm\":" << fan2
                  << ",\"speed_read_ok\":" << (speedOk ? "true" : "false")
                  << "}\n";
    } else {
        std::cout << "EC fan level: " << level << " (0x" << std::hex << level << std::dec << ")\n"
                  << "Fan 1: " << fan1 << " RPM\n"
                  << "Fan 2: " << fan2 << " RPM\n"
                  << "Speed read: " << (speedOk ? "ok" : "failed") << '\n';
    }
    return speedOk ? 0 : 1;
}

int RestoreECAuto(FanController& fan) {
    if (!fan.SetFanLevel(0x80, false)) {
        std::cerr << "Failed to restore EC automatic fan control.\n";
        return 1;
    }
    std::cout << "Fan control returned to EC automatic mode (0x80).\n";
    return 0;
}

int RunManual(FanController& fan, int level, int durationSeconds) {
    if (level < 0 || level > 7) {
        std::cerr << "Manual level must be between 0 and 7.\n";
        return 2;
    }
    if (durationSeconds < 0) {
        std::cerr << "Duration cannot be negative.\n";
        return 2;
    }

    if (!fan.SetFanLevel(level, false)) {
        std::cerr << "Failed to set fan level " << level << ".\n";
        return 1;
    }

    std::cout << "Fan level set to " << level << ".\n";
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
    return RestoreECAuto(fan);
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2 || HasArg(argc, argv, "--help") || HasArg(argc, argv, "-h")) {
        PrintUsage();
        return argc < 2 ? 2 : 0;
    }

    std::string_view backend;
    GetStringArg(argc, argv, "--backend", backend);

    HardwareSession hardware;
    if (!hardware.Open(backend)) return 1;

    const std::string_view command(argv[1]);
    if (command == "status") {
        return PrintStatus(*hardware.fan, HasArg(argc, argv, "--json"));
    }
    if (command == "mode" && argc >= 3 && std::string_view(argv[2]) == "auto") {
        return RestoreECAuto(*hardware.fan);
    }
    if (command == "fan") {
        int level = -1;
        int duration = 0;
        if (!GetIntArg(argc, argv, "--level", level)) {
            std::cerr << "Missing or invalid --level.\n";
            return 2;
        }
        if (HasArg(argc, argv, "--duration") &&
            !GetIntArg(argc, argv, "--duration", duration)) {
            std::cerr << "Missing or invalid --duration.\n";
            return 2;
        }
        return RunManual(*hardware.fan, level, duration);
    }

    std::cerr << "Unknown command. Use --help for usage.\n";
    return 2;
}
