#pragma once

#include <windows.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/msvc_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <dwmapi.h>
#include <cstdio>
#include <initializer_list>
#include <memory>
#include <string_view>
#include <vector>

namespace AppInit {

/// Enable DPI awareness for high-DPI displays
inline void EnableDPIAwareness() {
    HMODULE hUser32 = LoadLibraryA("user32.dll");
    if (hUser32) {
        typedef BOOL(WINAPI* PFN_SetProcessDpiAwarenessContext)(DPI_AWARENESS_CONTEXT);
        auto pSetProcessDpiAwarenessContext = (PFN_SetProcessDpiAwarenessContext)GetProcAddress(hUser32, "SetProcessDpiAwarenessContext");
        
        if (pSetProcessDpiAwarenessContext) {
            pSetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
            spdlog::info("DPI awareness enabled (Per-Monitor V2)");
        } else {
            spdlog::warn("SetProcessDpiAwarenessContext not available, using fallback");
            // Fallback to older API if available
            typedef BOOL(WINAPI* PFN_SetProcessDPIAware)(VOID);
            auto pSetProcessDPIAware = (PFN_SetProcessDPIAware)GetProcAddress(hUser32, "SetProcessDPIAware");
            if (pSetProcessDPIAware) {
                pSetProcessDPIAware();
            }
        }
        FreeLibrary(hUser32);
    }
}

inline bool HasArgument(int argc, char** argv, std::initializer_list<std::string_view> names) {
    for (int i = 1; i < argc; ++i) {
        if (!argv[i]) {
            continue;
        }

        std::string_view arg(argv[i]);
        for (std::string_view name : names) {
            if (arg == name) {
                return true;
            }
        }
    }

    return false;
}

inline bool HasConsoleArgument(int argc, char** argv) {
    return HasArgument(argc, argv, {"--console", "--debug-console", "-c"});
}

inline void EnsureDebugConsole() {
    if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
        AllocConsole();
    }

    FILE* stream = nullptr;
    freopen_s(&stream, "CONOUT$", "w", stdout);
    freopen_s(&stream, "CONOUT$", "w", stderr);
}

/// Initialize logging system. Default GUI launches stay silent; use --console for diagnostics.
inline void InitLogging(bool enableConsole = false) {
    try {
        auto msvc_sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("TPFanCtrl2.log", true);
        
        std::vector<spdlog::sink_ptr> sink_list = { msvc_sink, file_sink };
        if (enableConsole) {
            EnsureDebugConsole();
            sink_list.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
        }

        auto logger = std::make_shared<spdlog::logger>("multi_sink", sink_list.begin(), sink_list.end());
        logger->set_level(spdlog::level::debug);
        logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
        
        spdlog::set_default_logger(logger);
        spdlog::info("TPFanCtrl2 starting...");
        spdlog::info("Logging initialized ({} + file + MSVC)", enableConsole ? "console" : "silent GUI");
    } catch (const std::exception& ex) {
        ::MessageBoxA(nullptr, ex.what(), "Logging Init Failed", MB_ICONERROR);
    }
}

/// Check and log whether the process is running as Administrator
inline bool IsRunningAsAdmin() {
    BOOL isAdmin = FALSE;
    PSID adminGroup = nullptr;
    
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, 
                                  DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
        CheckTokenMembership(nullptr, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    
    if (isAdmin) {
        spdlog::info("Running with Administrator privileges");
    } else {
        spdlog::warn("NOT running as Administrator - EC access may fail!");
        spdlog::warn("Right-click TPFanCtrl2.exe and select 'Run as Administrator'");
    }
    
    return isAdmin;
}

/// Get DPI scale factor for a window
inline float GetDpiScale(HWND hwnd) {
    float dpiScale = 1.0f;
    HMODULE hUser32 = GetModuleHandleA("user32.dll");
    if (hUser32) {
        typedef UINT(WINAPI* PFN_GetDpiForWindow)(HWND);
        auto pGetDpiForWindow = (PFN_GetDpiForWindow)GetProcAddress(hUser32, "GetDpiForWindow");
        if (pGetDpiForWindow) {
            UINT dpi = pGetDpiForWindow(hwnd);
            dpiScale = (float)dpi / 96.0f;
            spdlog::info("Window DPI: {} (scale: {:.2f}x)", dpi, dpiScale);
        } else {
            spdlog::debug("GetDpiForWindow not available, using default scale");
        }
    }
    return dpiScale;
}

/// Apply Windows 11 visual effects (rounded corners, mica backdrop, dark mode)
inline void ApplyWindows11Effect(HWND hwnd) {
    HMODULE hDwmapi = LoadLibraryA("dwmapi.dll");
    if (!hDwmapi) {
        spdlog::debug("dwmapi.dll not available, skipping Windows 11 effects");
        return;
    }
    
    // Enable dark mode title bar (Windows 10 1809+)
    BOOL useDarkMode = TRUE;
    DwmSetWindowAttribute(hwnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &useDarkMode, sizeof(useDarkMode));
    
    // Enable rounded corners (Windows 11+)
    int cornerPreference = 2; // DWMWCP_ROUND
    DwmSetWindowAttribute(hwnd, 33 /* DWMWA_WINDOW_CORNER_PREFERENCE */, &cornerPreference, sizeof(cornerPreference));
    
    // Enable Mica backdrop (Windows 11+)
    int backdropType = 2; // DWMSBT_MAINWINDOW
    HRESULT hr = DwmSetWindowAttribute(hwnd, 38 /* DWMWA_SYSTEMBACKDROP_TYPE */, &backdropType, sizeof(backdropType));
    
    if (SUCCEEDED(hr)) {
        spdlog::info("Windows 11 visual effects applied (dark mode, rounded corners, mica)");
    } else {
        spdlog::debug("Mica backdrop not available (Windows 10 or older)");
    }
    
    FreeLibrary(hDwmapi);
}

} // namespace AppInit
