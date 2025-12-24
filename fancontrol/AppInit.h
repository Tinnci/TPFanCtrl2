#pragma once

// AppInit.h - Application initialization and cleanup functions
// Extracted from imgui_main.cpp main() function

#include <windows.h>
#include <memory>
#include <string>
#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/msvc_sink.h"

namespace AppInit {

// Enable Per-Monitor DPI Awareness for Windows 10+
inline void EnableDPIAwareness() {
    typedef BOOL(WINAPI* PFN_SetProcessDpiAwarenessContext)(HANDLE);
    HMODULE hUser32 = GetModuleHandleA("user32.dll");
    if (hUser32) {
        PFN_SetProcessDpiAwarenessContext pSetDpi = 
            (PFN_SetProcessDpiAwarenessContext)GetProcAddress(hUser32, "SetProcessDpiAwarenessContext");
        if (pSetDpi) {
            // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 is ((HANDLE)-4)
            pSetDpi((HANDLE)-4);
        }
    }
}

// Initialize spdlog with console, file, and MSVC debug output
inline void InitLogging(const char* logFileName = "TPFanCtrl2_debug.log") {
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFileName, true);
    auto msvc_sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
    
    auto logger = std::make_shared<spdlog::logger>("multi_sink", 
        spdlog::sinks_init_list{ console_sink, file_sink, msvc_sink });
    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::debug);
    spdlog::set_pattern("[%H:%M:%S] [%^%l%$] %v");
    
    spdlog::info("--- TPFanCtrl2 Session Started ---");
}

// Check if running as administrator
inline bool IsRunningAsAdmin() {
    BOOL isAdmin = FALSE;
    PSID adminGroup = NULL;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    
    if (AllocateAndInitializeSid(&ntAuthority, 2, 
        SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 
        0, 0, 0, 0, 0, 0, &adminGroup)) {
        CheckTokenMembership(NULL, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    
    spdlog::info("Running as Administrator: {}", isAdmin ? "YES" : "NO");
    return isAdmin != FALSE;
}

// Get DPI scale factor for the given window
inline float GetDpiScale(HWND hwnd) {
    typedef UINT(WINAPI* PFN_GetDpiForWindow)(HWND);
    HMODULE hUser32 = GetModuleHandleA("user32.dll");
    float dpiScale = 1.0f;
    
    if (hUser32) {
        PFN_GetDpiForWindow pGetDpi = 
            (PFN_GetDpiForWindow)GetProcAddress(hUser32, "GetDpiForWindow");
        if (pGetDpi) {
            UINT dpi = pGetDpi(hwnd);
            dpiScale = static_cast<float>(dpi) / 96.0f;
            spdlog::info("DPI: {}, Scale: {:.2f}", dpi, dpiScale);
        }
    }
    return dpiScale;
}

// Apply Windows 11 Mica/Acrylic effect
inline void ApplyWindows11Effect(HWND hwnd) {
    // Use DwmSetWindowAttribute for modern WinUI effects
    typedef HRESULT(WINAPI* PFN_DwmSetWindowAttribute)(HWND, DWORD, LPCVOID, DWORD);
    HMODULE hDwmapi = LoadLibraryA("dwmapi.dll");
    
    if (hDwmapi) {
        PFN_DwmSetWindowAttribute pDwmSet = 
            (PFN_DwmSetWindowAttribute)GetProcAddress(hDwmapi, "DwmSetWindowAttribute");
        if (pDwmSet) {
            // DWMWA_USE_IMMERSIVE_DARK_MODE = 20
            BOOL darkMode = TRUE;
            pDwmSet(hwnd, 20, &darkMode, sizeof(darkMode));
            
            // DWMWA_MICA_EFFECT = 1029 (Windows 11 22H2+)
            int micaValue = 2;
            pDwmSet(hwnd, 1029, &micaValue, sizeof(micaValue));
        }
        FreeLibrary(hDwmapi);
    }
}

// Create main application window
inline HWND CreateMainWindow(WNDPROC wndProc, HINSTANCE hInstance, 
                             const wchar_t* className, const wchar_t* title,
                             int width = 1100, int height = 850) {
    WNDCLASSEXW wc = { 
        sizeof(wc), CS_CLASSDC, wndProc, 0L, 0L, 
        hInstance, nullptr, nullptr, nullptr, nullptr, 
        className, nullptr 
    };
    ::RegisterClassExW(&wc);
    
    HWND hwnd = ::CreateWindowW(
        wc.lpszClassName, title, WS_OVERLAPPEDWINDOW, 
        100, 100, width, height, nullptr, nullptr, wc.hInstance, nullptr
    );
    
    return hwnd;
}

// Initialize sensor names with default ThinkPad mappings
template<typename SensorManagerPtr>
inline void InitDefaultSensorNames(SensorManagerPtr& sensorManager) {
    const char* defaultNames[] = {
        "CPU", "APS", "PCM", "GPU", "BAT1", "X7D", 
        "BAT2", "X7F", "BUS", "PCI", "PWR", "XC3"
    };
    
    for (int i = 0; i < 12; i++) {
        sensorManager->SetSensorName(i, defaultNames[i]);
    }
}

} // namespace AppInit
