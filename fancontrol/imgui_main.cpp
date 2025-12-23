#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00 // Windows 10
#endif
#include <windows.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>
#include <dwmapi.h>
#include <vector>
#include <string>
#include <deque>
#include <mutex>
#include <thread>
#include <chrono>
#include <cstdio>
#include <cstdarg>
#include <ctime>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_vulkan.h"
#include "imgui_freetype.h"

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"
#include <shellapi.h>

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif

// Project includes
#include "ConfigManager.h"
#include "SensorManager.h"
#include "FanController.h"
#include "ECManager.h"
#include "TVicPortProvider.h"
#include "TVicPort.h"
#include "DynamicIcon.h"
#include "I18nManager.h"

// --- Tray Constants ---
#define WM_TRAYICON (WM_USER + 100)
#define ID_TRAY_ICON 1001
#define ID_TRAY_RESTORE 1002
#define ID_TRAY_EXIT 1003

// --- Animation Helper ---
struct SmoothValue {
    float Current = 0.0f;
    float Target = 0.0f;
    void Update(float dt, float speed = 5.0f) {
        Current += (Target - Current) * (1.0f - expf(-speed * dt));
    }
};

// Icons (Segoe MDL2 Assets / Segoe Fluent Icons - Built-in on Windows 10/11)
// Using u8 prefix to ensure UTF-8 encoding
#define ICON_CPU   (const char*)u8"\uE9D2" // Speed High (仪表盘，代指处理器频率)
#define ICON_GPU   (const char*)u8"\uE967" // Video (视频，代指显卡)
#define ICON_FAN   (const char*)u8"\uE713" // Settings (齿轮，作为风扇/转速的通用代指)
#define ICON_CHIP  (const char*)u8"\uE950" // Memory (内存条/芯片)
#define ICON_CHART (const char*)u8"\uE9D9" // Activity Feed (动态折线图)
#define ICON_LOG   (const char*)u8"\uE81C" // List (列表/日志)

struct UIState {
    std::vector<SensorData> Sensors;
    std::map<std::string, SmoothValue> SmoothTemps;
    std::map<std::string, std::deque<float>> TempHistory;
    int Fan1Speed = 0;
    int Fan2Speed = 0;
    time_t LastUpdate = 0;
    
    // Control Settings
    ControlAlgorithm Algorithm = ControlAlgorithm::Step;
    PIDSettings PID;
    int ManualLevel = 0;
    int Mode = 2; // 0: BIOS, 1: Manual, 2: Smart
    int SelectedSettingsTab = 0; // 0: General, 1: Hardware, 2: PID, 3: Sensors
    
    std::mutex Mutex;
} g_UIState;

// --- Vulkan Globals ---
static VkAllocationCallbacks*   g_Allocator = nullptr;
static VkInstance               g_Instance = VK_NULL_HANDLE;
static VkPhysicalDevice         g_PhysicalDevice = VK_NULL_HANDLE;
static VkDevice                 g_Device = VK_NULL_HANDLE;
static uint32_t                 g_QueueFamily = (uint32_t)-1;
static VkQueue                  g_Queue = VK_NULL_HANDLE;
static VkDescriptorPool         g_DescriptorPool = VK_NULL_HANDLE;
static ImGui_ImplVulkanH_Window g_MainWindowData;
static uint32_t                 g_MinImageCount = 2;
static VmaAllocator             g_VmaAllocator = VK_NULL_HANDLE;
static bool                     g_SwapChainRebuild = false;
static int                      g_SwapChainResizeWidth = 0;
static int                      g_SwapChainResizeHeight = 0;

static std::shared_ptr<ConfigManager> g_Config;

void UpdateTrayIcon(HWND hWnd, int temp, int fan) {
    if (!hWnd) return;

    // Calculate DPI scale for tray icon
    float dpiScale = 1.0f;
    HMODULE hUser32 = GetModuleHandleA("user32.dll");
    if (hUser32) {
        typedef UINT(WINAPI* PFN_GetDpiForWindow)(HWND);
        PFN_GetDpiForWindow pGetDpi = (PFN_GetDpiForWindow)GetProcAddress(hUser32, "GetDpiForWindow");
        if (pGetDpi) {
            dpiScale = (float)pGetDpi(hWnd) / 96.0f;
        }
    }

    char line1[16], line2[16];
    sprintf_s(line1, "%d", temp);
    
    // Show fan speed in hundreds (e.g. 35 for 3500 RPM)
    sprintf_s(line2, "%d", fan / 100);

    int color = 0; // white
    if (temp >= 80) color = 14;      // red
    else if (temp >= 70) color = 13; // orange
    else if (temp >= 60) color = 12; // yellow
    else if (temp >= 50) color = 23; // green
    else color = 11;                 // blue

    CDynamicIcon dynIcon(line1, line2, color, 0, dpiScale);
    HICON hIcon = dynIcon.GetHIcon();
    if (!hIcon) return;

    NOTIFYICONDATAW nid = { sizeof(nid) };
    nid.hWnd = hWnd;
    nid.uID = ID_TRAY_ICON;
    nid.uFlags = NIF_ICON | NIF_TIP;
    nid.hIcon = hIcon;
    
    wchar_t tip[128];
    wchar_t wTemp[32], wFan[32];
    MultiByteToWideChar(CP_UTF8, 0, _TR("LBL_TEMP"), -1, wTemp, 32);
    MultiByteToWideChar(CP_UTF8, 0, _TR("LBL_FAN"), -1, wFan, 32);

    swprintf_s(tip, L"TPFanCtrl2\n%s: %d\u00B0\x43\n%s: %d RPM", wTemp, temp, wFan, fan);
    wcscpy_s(nid.szTip, tip);

    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

// --- Custom Lightweight Plot ---
void DrawSimplePlot(const char* label, const std::map<std::string, std::deque<float>>& history, float height, float dpiScale) {
    ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();      // Upper-left
    ImVec2 canvas_sz = ImGui::GetContentRegionAvail();   // Size of available space
    if (canvas_sz.x < 50.0f) canvas_sz.x = 50.0f;
    if (height > 0) canvas_sz.y = height;
    ImVec2 canvas_p1 = ImVec2(canvas_p0.x + canvas_sz.x, canvas_p0.y + canvas_sz.y);

    // Draw background
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(canvas_p0, canvas_p1, IM_COL32(30, 30, 30, 255));
    draw_list->AddRect(canvas_p0, canvas_p1, IM_COL32(100, 100, 100, 255));

    // Grid Lines (30, 50, 70, 90 degrees)
    float temps[] = { 30.0f, 50.0f, 70.0f, 90.0f };
    for (float t : temps) {
        float y = canvas_p1.y - (t / 100.0f) * canvas_sz.y;
        draw_list->AddLine(ImVec2(canvas_p0.x, y), ImVec2(canvas_p1.x, y), IM_COL32(60, 60, 60, 255));
        char buf[16]; snprintf(buf, 16, "%d", (int)t);
        draw_list->AddText(ImVec2(canvas_p0.x + 5 * dpiScale, y - 15 * dpiScale), IM_COL32(150, 150, 150, 255), buf);
    }

    // Plot Lines
    int colorIdx = 0;
    ImU32 colors[] = {
        IM_COL32(0, 255, 255, 255),   // Cyan
        IM_COL32(255, 165, 0, 255),   // Orange
        IM_COL32(124, 252, 0, 255),   // LawnGreen
        IM_COL32(255, 105, 180, 255), // HotPink
        IM_COL32(173, 216, 230, 255)  // LightBlue
    };

    float legendX = canvas_p0.x + 40 * dpiScale;
    for (auto const& [name, data] : history) {
        // Skip ignored sensors in plot
        if (g_Config && g_Config->IgnoreSensors.find(name) != std::string::npos) continue;

        if (data.size() < 2) continue;

        ImU32 color = colors[colorIdx % 5];
        float stepX = canvas_sz.x / 300.0f; 
        
        for (size_t i = 0; i < data.size() - 1; i++) {
            ImVec2 p1 = ImVec2(canvas_p1.x - (data.size() - i) * stepX, 
                               canvas_p1.y - (data[i] / 100.0f) * canvas_sz.y);
            ImVec2 p2 = ImVec2(canvas_p1.x - (data.size() - (i + 1)) * stepX, 
                               canvas_p1.y - (data[i+1] / 100.0f) * canvas_sz.y);
            
            if (p1.x < canvas_p0.x) continue;
            draw_list->AddLine(p1, p2, color, 2.0f * dpiScale);
        }
        
        // Draw Legend in the plot area
        draw_list->AddText(ImVec2(legendX, canvas_p0.y + 5 * dpiScale), color, name.c_str());
        legendX += ImGui::CalcTextSize(name.c_str()).x + 15 * dpiScale;
        colorIdx++;
    }

    ImGui::Dummy(canvas_sz); // Advance cursor
}

// --- Logging System ---
static std::mutex g_LogMutex;
enum LogLevel { LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR };
static LogLevel g_MinLogLevel = LOG_INFO;

void Log(LogLevel level, const char* fmt, ...) {
    if (level < g_MinLogLevel) return;
    std::lock_guard<std::mutex> lock(g_LogMutex);
    const char* prefix = "[INFO] ";
    if (level == LOG_DEBUG) prefix = "[DEBUG] ";
    if (level == LOG_WARN)  prefix = "[WARN]  ";
    if (level == LOG_ERROR) prefix = "[ERROR] ";
    
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, 1024, fmt, args);
    va_end(args);

    char timeBuf[32];
    time_t now = time(nullptr);
    struct tm tm_info;
    localtime_s(&tm_info, &now);
    strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", &tm_info);

    printf("[%s] %s%s\n", timeBuf, prefix, buf);

    FILE* f = fopen("TPFanCtrl2_debug.log", "a");
    if (f) {
        fprintf(f, "[%s] %s%s\n", timeBuf, prefix, buf);
        fclose(f);
    }
}

// --- Log System ---
struct AppLog {
    std::deque<std::string> Items;
    std::mutex Mutex;
    void AddLog(const char* fmt, ...) {
        char buf[1024];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, 1024, fmt, args);
        va_end(args);

        // Persist to file and console as well
        Log(LOG_INFO, "%s", buf);

        std::lock_guard<std::mutex> lock(Mutex);
        Items.push_back(buf);
        if (Items.size() > 100) Items.pop_front();
    }
} g_AppLog;

// --- Vulkan Helpers ---
void SetupVulkan(const char** extensions, uint32_t extensions_count) {
    VkResult err;
    VkInstanceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    
    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.apiVersion = VK_API_VERSION_1_2;
    create_info.pApplicationInfo = &app_info;
    
    create_info.enabledExtensionCount = extensions_count;
    create_info.ppEnabledExtensionNames = extensions;
    err = vkCreateInstance(&create_info, g_Allocator, &g_Instance);
    if (err != VK_SUCCESS) { Log(LOG_ERROR, "vkCreateInstance failed: %d", err); exit(1); }

    uint32_t gpu_count;
    vkEnumeratePhysicalDevices(g_Instance, &gpu_count, nullptr);
    if (gpu_count == 0) { Log(LOG_ERROR, "No GPU found"); exit(1); }
    
    std::vector<VkPhysicalDevice> gpus(gpu_count);
    vkEnumeratePhysicalDevices(g_Instance, &gpu_count, gpus.data());
    g_PhysicalDevice = gpus[0];

    uint32_t count;
    vkGetPhysicalDeviceQueueFamilyProperties(g_PhysicalDevice, &count, nullptr);
    std::vector<VkQueueFamilyProperties> queues(count);
    vkGetPhysicalDeviceQueueFamilyProperties(g_PhysicalDevice, &count, queues.data());
    for (uint32_t i = 0; i < count; i++)
        if (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) { g_QueueFamily = i; break; }

    float queue_priority[] = { 1.0f };
    VkDeviceQueueCreateInfo queue_info = {};
    queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info.queueFamilyIndex = g_QueueFamily;
    queue_info.queueCount = 1;
    queue_info.pQueuePriorities = queue_priority;
    
    const char* device_extensions[] = { "VK_KHR_swapchain" };
    VkDeviceCreateInfo device_info = {};
    device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_info.queueCreateInfoCount = 1;
    device_info.pQueueCreateInfos = &queue_info;
    device_info.enabledExtensionCount = 1;
    device_info.ppEnabledExtensionNames = device_extensions;
    err = vkCreateDevice(g_PhysicalDevice, &device_info, g_Allocator, &g_Device);
    if (err != VK_SUCCESS) { Log(LOG_ERROR, "vkCreateDevice failed: %d", err); exit(1); }
    vkGetDeviceQueue(g_Device, g_QueueFamily, 0, &g_Queue);

    // Initialize VMA
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_2;
    allocatorInfo.physicalDevice = g_PhysicalDevice;
    allocatorInfo.device = g_Device;
    allocatorInfo.instance = g_Instance;
    err = vmaCreateAllocator(&allocatorInfo, &g_VmaAllocator);
    if (err != VK_SUCCESS) { Log(LOG_ERROR, "vmaCreateAllocator failed: %d", err); exit(1); }

    VkDescriptorPoolSize pool_sizes[] = { { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100 } };
    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 100;
    pool_info.poolSizeCount = 1;
    pool_info.pPoolSizes = pool_sizes;
    vkCreateDescriptorPool(g_Device, &pool_info, g_Allocator, &g_DescriptorPool);
}

void SetupVulkanWindow(ImGui_ImplVulkanH_Window* wd, VkSurfaceKHR surface, int width, int height) {
    wd->Surface = surface;
    const VkFormat requestSurfaceImageFormat[] = { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM };
    const VkColorSpaceKHR requestSurfaceColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    wd->SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(g_PhysicalDevice, wd->Surface, requestSurfaceImageFormat, 2, requestSurfaceColorSpace);
    VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_FIFO_KHR };
    wd->PresentMode = ImGui_ImplVulkanH_SelectPresentMode(g_PhysicalDevice, wd->Surface, present_modes, 1);
    ImGui_ImplVulkanH_CreateOrResizeWindow(g_Instance, g_PhysicalDevice, g_Device, wd, g_QueueFamily, g_Allocator, width, height, g_MinImageCount, 0);
}

// --- Hardware Worker Thread ---
void HardwareWorker(std::shared_ptr<ConfigManager> config, 
                    std::shared_ptr<SensorManager> sensors, 
                    std::shared_ptr<FanController> fans,
                    bool* running,
                    HWND hWnd) {
    int fanCounter = 0;
    int sensorCounter = 0;
    int trayCounter = 0;
    int maxTemp = 0;

    while (*running) {
        // 1. Fan Speed Polling (Every 1s = 10 * 100ms)
        if (fanCounter <= 0) {
            int f1 = 0, f2 = 0;
            fans->GetFanSpeeds(f1, f2);
            {
                std::lock_guard<std::mutex> lock(g_UIState.Mutex);
                g_UIState.Fan1Speed = f1;
                g_UIState.Fan2Speed = f2;
            }
            fanCounter = 10;
        }

        // 2. Sensor Polling (Every 'Cycle' seconds)
        if (sensorCounter <= 0) {
            if (!sensors->UpdateSensors(config->ShowBiasedTemps, config->NoExtSensor, config->UseTWR)) {
                Log(LOG_WARN, "Failed to update sensors from EC.");
            }
            
            int maxIdx = 0;
            maxTemp = sensors->GetMaxTemp(maxIdx, config->IgnoreSensors);
            
            // Sync to UI State
            {
                std::lock_guard<std::mutex> lock(g_UIState.Mutex);
                g_UIState.Sensors = sensors->GetSensors();
                g_UIState.LastUpdate = time(nullptr);
                for (const auto& s : g_UIState.Sensors) {
                    // Update smooth animation target
                    if (s.isAvailable && s.rawTemp > 0 && s.rawTemp < 128) {
                        g_UIState.SmoothTemps[s.name].Target = (float)s.rawTemp;
                    }

                    // Update history - ALWAYS push a value to keep timelines synchronized
                    // If sensor is not available, we don't push anything to avoid cluttering the map
                    if (!s.isAvailable) continue;

                    auto& history = g_UIState.TempHistory[s.name];
                    
                    float valToPush = (float)s.rawTemp;
                    // If value is clearly invalid (0 or 128), use the last known value to avoid spikes
                    if ((s.rawTemp <= 0 || s.rawTemp >= 128) && !history.empty()) {
                        valToPush = history.back();
                    }

                    history.push_back(valToPush);
                    if (history.size() > 300) history.pop_front();
                }
            }

            sensorCounter = (config->Cycle > 0 ? config->Cycle : 1) * 10;
        }

        // 3. Control Logic (Every 100ms for maximum responsiveness)
        {
            std::lock_guard<std::mutex> lock(g_UIState.Mutex);
            if (g_UIState.Mode == 1) { // Manual
                fans->SetFanLevel(g_UIState.ManualLevel);
            } else if (g_UIState.Mode == 2) { // Smart
                if (g_UIState.Algorithm == ControlAlgorithm::Step) {
                    // Step control still uses the 1s-based logic internally or we just call it
                    fans->UpdateSmartControl(maxTemp, config->SmartLevels1);
                } else {
                    // PID Control (using 0.1s dt for high-frequency updates)
                    fans->UpdatePIDControl((float)maxTemp, g_UIState.PID, 0.1f);
                }
            }
        }

        // 4. Tray Update (Every 1s)
        if (trayCounter <= 0) {
            UpdateTrayIcon(hWnd, maxTemp, g_UIState.Fan1Speed);
            trayCounter = 10;
        }
        trayCounter--;

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        fanCounter--;
        sensorCounter--;
    }
}

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// --- Tray Management ---
void AddTrayIcon(HWND hWnd) {
    NOTIFYICONDATAW nid = { sizeof(nid) };
    nid.hWnd = hWnd;
    nid.uID = ID_TRAY_ICON;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcscpy(nid.szTip, L"TPFanCtrl2 - Modernized");
    Shell_NotifyIconW(NIM_ADD, &nid);
}

void RemoveTrayIcon(HWND hWnd) {
    NOTIFYICONDATAW nid = { sizeof(nid) };
    nid.hWnd = hWnd;
    nid.uID = ID_TRAY_ICON;
    Shell_NotifyIconW(NIM_DELETE, &nid);
}

int main(int argc, char** argv) {
    // Enable DPI Awareness (Dynamic loading for compatibility)
    typedef BOOL(WINAPI* PFN_SetProcessDpiAwarenessContext)(HANDLE);
    HMODULE hUser32 = GetModuleHandleA("user32.dll");
    if (hUser32) {
        PFN_SetProcessDpiAwarenessContext pSetDpi = (PFN_SetProcessDpiAwarenessContext)GetProcAddress(hUser32, "SetProcessDpiAwarenessContext");
        if (pSetDpi) {
            // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 is ((HANDLE)-4)
            pSetDpi((HANDLE)-4);
        }
    }

    // Clear old log file for a fresh session
    remove("TPFanCtrl2_debug.log");
    Log(LOG_INFO, "--- TPFanCtrl2 Session Started ---");

    // Check Privileges
    BOOL isAdmin = FALSE;
    PSID adminGroup = NULL;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
        CheckTokenMembership(NULL, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    Log(LOG_INFO, "Running as Administrator: %s", isAdmin ? "YES" : "NO");

    HINSTANCE hInstance = GetModuleHandle(NULL);
    // Logic Init
    g_Config = std::make_shared<ConfigManager>();
    g_Config->LoadConfig("TPFanCtrl2.ini");
    I18nManager::Get().SetLanguage(g_Config->Language);

    // Sync UI State with Config
    {
        std::lock_guard<std::mutex> lock(g_UIState.Mutex);
        g_UIState.Mode = g_Config->ActiveMode;
        g_UIState.ManualLevel = g_Config->ManFanSpeed;
        g_UIState.Algorithm = (ControlAlgorithm)g_Config->ControlAlgorithm;
        g_UIState.PID.targetTemp = g_Config->PID_Target;
        g_UIState.PID.Kp = g_Config->PID_Kp;
        g_UIState.PID.Ki = g_Config->PID_Ki;
        g_UIState.PID.Kd = g_Config->PID_Kd;
    }

    // Initialize Hardware Driver (TVicPort)
    Log(LOG_INFO, "Initializing TVicPort driver...");
    bool driverOk = false;
    for (int i = 0; i < 5; i++) {
        if (OpenTVicPort()) {
            driverOk = true;
            break;
        }
        Log(LOG_WARN, "Failed to open TVicPort, retrying... (%d/5)", i + 1);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    if (driverOk) {
        Log(LOG_INFO, "TVicPort driver opened successfully.");
        SetHardAccess(TRUE);
        if (TestHardAccess()) {
            Log(LOG_INFO, "Hardware access (Ring 0) granted.");
        } else {
            Log(LOG_ERROR, "Hardware access denied even with driver opened.");
        }
    } else {
        Log(LOG_ERROR, "CRITICAL: Could not initialize TVicPort driver. Hardware control will not work.");
        MessageBoxW(NULL, L"Could not initialize TVicPort driver.\nEnsure TVicPort.dll is present and you are running as Administrator.", L"Driver Error", MB_OK | MB_ICONERROR);
    }

    auto ecManager = std::make_shared<ECManager>(std::make_shared<TVicPortProvider>(), [](const char* msg) { g_AppLog.AddLog("[EC] %s", msg); });
    auto sensorManager = std::make_shared<SensorManager>(ecManager);
    
    // Initialize Sensor Names (matching original TPFanControl)
    sensorManager->SetSensorName(0, "CPU");
    sensorManager->SetSensorName(1, "APS");
    sensorManager->SetSensorName(2, "PCM");
    sensorManager->SetSensorName(3, "GPU");
    sensorManager->SetSensorName(4, "BAT1");
    sensorManager->SetSensorName(5, "X7D");
    sensorManager->SetSensorName(6, "BAT2");
    sensorManager->SetSensorName(7, "X7F");
    sensorManager->SetSensorName(8, "BUS");
    sensorManager->SetSensorName(9, "PCI");
    sensorManager->SetSensorName(10, "PWR");
    sensorManager->SetSensorName(11, "XC3");

    auto fanController = std::make_shared<FanController>(ecManager);

    fanController->SetOnChangeCallback([](int level) {
        g_AppLog.AddLog("[Fan] Level changed to %d", level);
    });

    // Window Init
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"TPFanCtrl2Vulkan", nullptr };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"TPFanCtrl2 - Vulkan Modernized", WS_OVERLAPPEDWINDOW, 100, 100, 1100, 850, nullptr, nullptr, wc.hInstance, nullptr);

    // Start Hardware Thread
    bool hwRunning = true;
    std::thread hwThread(HardwareWorker, g_Config, sensorManager, fanController, &hwRunning, hwnd);

    // Get DPI Scale
    float dpiScale = 1.0f;
    typedef UINT(WINAPI* PFN_GetDpiForWindow)(HWND);
    if (hUser32) {
        PFN_GetDpiForWindow pGetDpi = (PFN_GetDpiForWindow)GetProcAddress(hUser32, "GetDpiForWindow");
        if (pGetDpi) {
            dpiScale = (float)pGetDpi(hwnd) / 96.0f;
        }
    }
    Log(LOG_INFO, "DPI Scale detected: %.2f", dpiScale);

    BOOL dark = TRUE;
    DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark));
    int backdrop = 2;
    DwmSetWindowAttribute(hwnd, 38, &backdrop, sizeof(backdrop));

    const char* extensions[] = { "VK_KHR_surface", "VK_KHR_win32_surface" };
    SetupVulkan(extensions, 2);

    VkWin32SurfaceCreateInfoKHR surface_info = {};
    surface_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surface_info.hinstance = hInstance;
    surface_info.hwnd = hwnd;
    VkSurfaceKHR surface;
    vkCreateWin32SurfaceKHR(g_Instance, &surface_info, g_Allocator, &surface);

    RECT rect; GetClientRect(hwnd, &rect);
    SetupVulkanWindow(&g_MainWindowData, surface, rect.right - rect.left, rect.bottom - rect.top);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    
    Log(LOG_INFO, "Loading fonts...");
    // 1. Optimize Font Rendering with FreeType
    ImFontConfig font_cfg;
    font_cfg.FontLoaderFlags |= ImGuiFreeTypeBuilderFlags_LightHinting;
    
    // Try to load a font that supports Chinese to allow dynamic language switching
    ImFont* mainFont = nullptr;
    const char* msyhPath = "C:\\Windows\\Fonts\\msyh.ttc";
    if (::GetFileAttributesA(msyhPath) != INVALID_FILE_ATTRIBUTES) {
        mainFont = io.Fonts->AddFontFromFileTTF(msyhPath, 18.0f * dpiScale, &font_cfg, io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
    }
    
    if (!mainFont) {
        mainFont = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 18.0f * dpiScale, &font_cfg, io.Fonts->GetGlyphRangesDefault());
    }

    if (!mainFont) {
        Log(LOG_WARN, "Failed to load system fonts, using default font.");
    }
    
    // 2. Load Icons (Segoe Fluent Icons on Win11, Segoe MDL2 Assets on Win10)
    static const ImWchar icon_ranges[] = { 0xE700, 0xF800, 0 };
    ImFontConfig icon_cfg;
    icon_cfg.MergeMode = true;
    icon_cfg.PixelSnapH = true;
    icon_cfg.GlyphMinAdvanceX = 16.0f * dpiScale; // Ensure icons have some width
    
    // Try multiple possible paths for the icon font
    const char* iconFontPaths[] = {
        "C:\\Windows\\Fonts\\SegoeIcons.ttf", // Windows 11 Fluent Icons
        "C:\\Windows\\Fonts\\segmdl2.ttf",    // Windows 10 MDL2 Assets
        "C:\\Windows\\Fonts\\seguisym.ttf"    // Fallback for older Windows
    };
    
    bool iconLoaded = false;
    for (const char* path : iconFontPaths) {
        if (io.Fonts->AddFontFromFileTTF(path, 16.0f * dpiScale, &icon_cfg, icon_ranges)) {
            Log(LOG_INFO, "Icon font loaded successfully from: %s", path);
            iconLoaded = true;
            break;
        }
    }
    
    if (!iconLoaded) {
        Log(LOG_ERROR, "CRITICAL: Could not load any icon font. Icons will appear as '?'");
    }
    
    ImGui::StyleColorsDark();
    auto& style = ImGui::GetStyle();
    style.ScaleAllSizes(dpiScale); // Scale UI elements (padding, spacing, etc.)
    style.WindowRounding = 10.0f * dpiScale;
    style.FrameRounding = 6.0f * dpiScale;
    style.PopupRounding = 6.0f * dpiScale;
    style.ScrollbarRounding = 12.0f * dpiScale;
    style.GrabRounding = 6.0f * dpiScale;
    style.ItemSpacing = ImVec2(12 * dpiScale, 10 * dpiScale);
    style.WindowPadding = ImVec2(15 * dpiScale, 15 * dpiScale);
    
    // ThinkPad Modern Theme Colors
    ImVec4 tpRed = ImVec4(0.89f, 0.12f, 0.16f, 1.0f);
    ImVec4 tpRedHover = ImVec4(1.0f, 0.2f, 0.25f, 1.0f);
    
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.07f, 0.07f, 0.07f, 0.94f);
    style.Colors[ImGuiCol_ChildBg] = ImVec4(0.12f, 0.12f, 0.12f, 0.5f);
    style.Colors[ImGuiCol_Border] = ImVec4(0.2f, 0.2f, 0.2f, 0.5f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
    style.Colors[ImGuiCol_CheckMark] = tpRed;
    style.Colors[ImGuiCol_SliderGrab] = tpRed;
    style.Colors[ImGuiCol_SliderGrabActive] = tpRedHover;
    style.Colors[ImGuiCol_Button] = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
    style.Colors[ImGuiCol_ButtonHovered] = tpRed;
    style.Colors[ImGuiCol_ButtonActive] = tpRedHover;
    style.Colors[ImGuiCol_Header] = ImVec4(tpRed.x, tpRed.y, tpRed.z, 0.3f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(tpRed.x, tpRed.y, tpRed.z, 0.5f);
    style.Colors[ImGuiCol_HeaderActive] = tpRed;
    style.Colors[ImGuiCol_Tab] = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
    style.Colors[ImGuiCol_TabHovered] = tpRed;
    style.Colors[ImGuiCol_TabActive] = tpRed;
    style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);
    style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
    style.Colors[ImGuiCol_PlotHistogram] = tpRed;
    style.Colors[ImGuiCol_Separator] = ImVec4(0.25f, 0.25f, 0.25f, 1.0f);

    Log(LOG_INFO, "Initializing Win32 backend...");
    ImGui_ImplWin32_Init(hwnd);
    
    AddTrayIcon(hwnd);

    Log(LOG_INFO, "Initializing Vulkan backend...");
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.ApiVersion = VK_API_VERSION_1_2;
    init_info.Instance = g_Instance;
    init_info.PhysicalDevice = g_PhysicalDevice;
    init_info.Device = g_Device;
    init_info.QueueFamily = g_QueueFamily;
    init_info.Queue = g_Queue;
    init_info.DescriptorPool = g_DescriptorPool;
    init_info.MinImageCount = g_MinImageCount;
    init_info.ImageCount = g_MainWindowData.ImageCount;
    init_info.PipelineInfoMain.RenderPass = g_MainWindowData.RenderPass;
    init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    if (!ImGui_ImplVulkan_Init(&init_info)) {
        Log(LOG_ERROR, "ImGui_ImplVulkan_Init failed");
        exit(1);
    }

    if (g_Config->StartMinimized) {
        Log(LOG_INFO, "Starting minimized to tray.");
        ::ShowWindow(hwnd, SW_HIDE);
    } else {
        Log(LOG_INFO, "Showing window...");
        ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    }

    bool done = false;
    Log(LOG_INFO, "Entering main loop...");
    int frameCount = 0;
    while (!done) {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT) {
                Log(LOG_INFO, "WM_QUIT received.");
                done = true;
            }
        }
        if (done) break;

        frameCount++;

        if (g_SwapChainRebuild) {
            int width, height;
            RECT rect;
            GetClientRect(hwnd, &rect);
            width = rect.right - rect.left;
            height = rect.bottom - rect.top;
            if (width > 0 && height > 0) {
                vkDeviceWaitIdle(g_Device);
                ImGui_ImplVulkan_SetMinImageCount(g_MinImageCount);
                ImGui_ImplVulkanH_CreateOrResizeWindow(g_Instance, g_PhysicalDevice, g_Device, &g_MainWindowData, g_QueueFamily, g_Allocator, width, height, g_MinImageCount, 0);
                g_MainWindowData.FrameIndex = 0;
                g_SwapChainRebuild = false;
            }
        }

        // Skip rendering if minimized
        if (g_MainWindowData.Width == 0 || g_MainWindowData.Height == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        ImGui_ImplVulkanH_Window* wd = &g_MainWindowData;
        VkSemaphore image_acquired_semaphore  = wd->FrameSemaphores[wd->SemaphoreIndex].ImageAcquiredSemaphore;
        VkSemaphore render_complete_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
        VkResult err = vkAcquireNextImageKHR(g_Device, wd->Swapchain, UINT64_MAX, image_acquired_semaphore, VK_NULL_HANDLE, &wd->FrameIndex);
        if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR) {
            g_SwapChainRebuild = true;
            continue;
        }

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        float dt = ImGui::GetIO().DeltaTime;
        {
            std::lock_guard<std::mutex> lock(g_UIState.Mutex);
            for (auto& pair : g_UIState.SmoothTemps) {
                pair.second.Update(dt);
            }
        }

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin("Main", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus);

        // --- Main Content (Tabs) ---
        if (ImGui::BeginTabBar("MainTabs", ImGuiTabBarFlags_None)) {
            if (ImGui::BeginTabItem(_TR("TAB_DASHBOARD"))) {
                ImGui::Spacing();

                // --- 1. Metric Cards ---
                ImGui::Columns(2, "MetricCards", false);
                
                auto drawMetricCard = [&](const char* icon, const char* label, const char* value, const char* subValue, ImVec4 color) {
                    ImGui::BeginChild(label, ImVec2(0, 90 * dpiScale), true);
                    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1), "%s %s", icon, label);
                    ImGui::Spacing();
                    
                    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]); // Use a slightly larger font if available, or just bold
                    ImGui::TextColored(color, "%s", value);
                    ImGui::PopFont();

                    if (subValue && subValue[0] != '\0') {
                        ImGui::SameLine();
                        ImGui::TextColored(ImVec4(0.4f, 0.4f, 0.4f, 1), "(%s)", subValue);
                    }
                    ImGui::EndChild();
                };

                int f1, f2, maxTemp = 0;
                std::string maxName = "N/A";
                {
                    std::lock_guard<std::mutex> lock(g_UIState.Mutex);
                    f1 = g_UIState.Fan1Speed;
                    f2 = g_UIState.Fan2Speed;
                    for (const auto& s : g_UIState.Sensors) {
                        if (s.rawTemp > maxTemp && s.rawTemp < 128) {
                            maxTemp = s.rawTemp;
                            maxName = s.name;
                        }
                    }
                }

                ImVec4 tempColor = ImVec4(0, 0.8f, 1, 1);
                if (maxTemp > 65) tempColor = ImVec4(1, 0.6f, 0, 1);
                if (maxTemp > 85) tempColor = ImVec4(1, 0.2f, 0.2f, 1);

                char tempVal[32], fanVal[64], fanSub[64];
                sprintf_s(tempVal, "%d\xC2\xB0\x43", maxTemp);
                sprintf_s(fanVal, "%d %s", f1, _TR("LBL_RPM"));
                if (f2 > 0) sprintf_s(fanSub, "%d %s", f2, _TR("LBL_RPM"));
                else fanSub[0] = '\0';

                drawMetricCard(ICON_CPU, _TR("LBL_MAX_TEMP"), tempVal, maxName.c_str(), tempColor);
                ImGui::NextColumn();
                drawMetricCard(ICON_FAN, _TR("LBL_FAN_SPEEDS"), fanVal, (f2 > 0 ? fanSub : nullptr), ImVec4(1, 1, 1, 1));
                ImGui::Columns(1);

                ImGui::Spacing();

                // --- 2. Main Layout ---
                if (ImGui::BeginTable("DashboardMain", 2, ImGuiTableFlags_Resizable)) {
                    ImGui::TableNextColumn();
                    
                    // --- Left: Sensors Grid & History ---
                    ImGui::BeginChild("SensorsArea", ImVec2(0, 0), false);
                    
                    ImGui::TextColored(ImVec4(0.89f, 0.12f, 0.16f, 1.0f), "%s %s", ICON_CHIP, _TR("SECTION_STATUS"));
                    ImGui::Separator();
                    ImGui::Spacing();

                    // Sensors in a grid
                    if (ImGui::BeginChild("SensorsScroll", ImVec2(0, 280 * dpiScale), true)) {
                        ImGui::Columns(2, "SensorGrid", false);
                        {
                            std::lock_guard<std::mutex> lock(g_UIState.Mutex);
                            for (const auto& s : g_UIState.Sensors) {
                                if (!s.isAvailable) continue;
                                if (g_Config->IgnoreSensors.find(s.name) != std::string::npos) continue;

                                float currentTemp = g_UIState.SmoothTemps[s.name].Current;
                                float progress = currentTemp / 100.0f;
                                ImVec4 color = (currentTemp < 50 ? ImVec4(0, 0.7f, 0.9f, 1) : (currentTemp < 75 ? ImVec4(0.9f, 0.6f, 0, 1) : ImVec4(0.9f, 0.1f, 0.1f, 1)));

                                ImGui::BeginChild(s.name.c_str(), ImVec2(0, 65 * dpiScale), true);
                                ImGui::Text("%s %s", (s.name.find("GPU") != std::string::npos ? ICON_GPU : ICON_CPU), s.name.c_str());
                                ImGui::SameLine(ImGui::GetContentRegionAvail().x - 45 * dpiScale);
                                ImGui::Text("%.0f\xC2\xB0\x43", currentTemp);
                                
                                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, color);
                                ImGui::ProgressBar(progress, ImVec2(-1, 5 * dpiScale), "");
                                ImGui::PopStyleColor();
                                ImGui::EndChild();

                                ImGui::NextColumn();
                            }
                        }
                        ImGui::Columns(1);
                    }
                    ImGui::EndChild();

                    ImGui::Spacing();
                    ImGui::TextColored(ImVec4(0.89f, 0.12f, 0.16f, 1.0f), "%s %s", ICON_CHART, _TR("SECTION_HISTORY"));
                    ImGui::Separator();
                    ImGui::Spacing();
                    {
                        std::lock_guard<std::mutex> lock(g_UIState.Mutex);
                        DrawSimplePlot("TempPlot", g_UIState.TempHistory, 0, dpiScale);
                    }
                    ImGui::EndChild();

                    ImGui::TableNextColumn();

                    // --- Right: Control & Logs ---
                    ImGui::BeginChild("ControlPanel", ImVec2(0, 240 * dpiScale), true);
                    ImGui::TextColored(ImVec4(0.89f, 0.12f, 0.16f, 1.0f), "%s %s", ICON_FAN, _TR("SECTION_CONTROL"));
                    ImGui::Separator();
                    ImGui::Spacing();
                    
                    auto drawSegmented = [&](const char* label, int id, int* current) {
                        bool active = (*current == id);
                        if (active) {
                            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.89f, 0.12f, 0.16f, 0.8f));
                            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.89f, 0.12f, 0.16f, 0.9f));
                            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.89f, 0.12f, 0.16f, 1.0f));
                        }
                        if (ImGui::Button(label, ImVec2(ImGui::GetContentRegionAvail().x / 3.2f, 35 * dpiScale))) {
                            *current = id;
                        }
                        if (active) ImGui::PopStyleColor(3);
                        ImGui::SameLine();
                    };

                    {
                        std::lock_guard<std::mutex> lock(g_UIState.Mutex);
                        drawSegmented(_TR("MODE_BIOS"), 0, &g_UIState.Mode);
                        drawSegmented(_TR("MODE_MANUAL"), 1, &g_UIState.Mode);
                        drawSegmented(_TR("MODE_SMART"), 2, &g_UIState.Mode);
                        ImGui::NewLine();

                        ImGui::Spacing();
                        if (g_UIState.Mode == 1) {
                            ImGui::Text(_TR("LBL_MANUAL_LEVEL"));
                            ImGui::SliderInt("##Level", &g_UIState.ManualLevel, 0, 7);
                        } else if (g_UIState.Mode == 2) {
                            ImGui::Text(_TR("LBL_ALGORITHM"));
                            const char* algoNames[] = { _TR("ALGO_STEP"), _TR("ALGO_PID") };
                            int algoIdx = (int)g_UIState.Algorithm;
                            ImGui::PushItemWidth(-1);
                            if (ImGui::Combo("##Algo", &algoIdx, algoNames, 2)) {
                                g_UIState.Algorithm = (ControlAlgorithm)algoIdx;
                            }
                            ImGui::PopItemWidth();
                        }
                    }
                    
                    ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 55 * dpiScale);
                    if (ImGui::Button(_TR("BTN_MINIMIZE"), ImVec2(-1, 40 * dpiScale))) {
                        ::ShowWindow(hwnd, SW_HIDE);
                    }
                    ImGui::EndChild();

                    ImGui::BeginChild("LogsPanel", ImVec2(0, 0), true);
                    ImGui::TextColored(ImVec4(0.89f, 0.12f, 0.16f, 1.0f), "%s %s", ICON_LOG, _TR("SECTION_LOGS"));
                    ImGui::Separator();
                    {
                        std::lock_guard<std::mutex> lock(g_AppLog.Mutex);
                        ImGui::BeginChild("LogScroll");
                        for (const auto& line : g_AppLog.Items) {
                            if (line.find("[ERROR]") != std::string::npos) ImGui::TextColored(ImVec4(1, 0.2f, 0.2f, 1), line.c_str());
                            else if (line.find("[WARN]") != std::string::npos) ImGui::TextColored(ImVec4(1, 0.8f, 0, 1), line.c_str());
                            else ImGui::TextUnformatted(line.c_str());
                        }
                        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f);
                        ImGui::EndChild();
                    }
                    ImGui::EndChild();

                    ImGui::EndTable();
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem(_TR("TAB_SETTINGS"))) {
                // --- Sidebar ---
                ImGui::BeginChild("SettingsSidebar", ImVec2(180 * dpiScale, 0), true);
                
                auto drawSidebarItem = [&](int id, const char* icon, const char* labelKey) {
                    bool selected = (g_UIState.SelectedSettingsTab == id);
                    if (selected) ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.89f, 0.12f, 0.16f, 0.2f));
                    if (selected) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.89f, 0.12f, 0.16f, 1.0f));
                    
                    char buf[128];
                    sprintf_s(buf, "%s  %s", icon, _TR(labelKey));
                    if (ImGui::Selectable(buf, selected, 0, ImVec2(0, 40 * dpiScale))) {
                        g_UIState.SelectedSettingsTab = id;
                    }
                    
                    if (selected) ImGui::PopStyleColor(2);
                };

                ImGui::Spacing();
                drawSidebarItem(0, ICON_CHIP, "SIDEBAR_GENERAL");
                drawSidebarItem(1, ICON_CPU, "SIDEBAR_PID");
                drawSidebarItem(2, ICON_GPU, "SIDEBAR_SENSORS");

                // Bottom Save Button
                float availH = ImGui::GetContentRegionAvail().y;
                if (availH > 60 * dpiScale) {
                    ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 70 * dpiScale);
                    ImGui::Separator();
                    if (ImGui::Button(_TR("BTN_SAVE_ALL"), ImVec2(-1, 50 * dpiScale))) {
                        {
                            std::lock_guard<std::mutex> lock(g_UIState.Mutex);
                            g_Config->ActiveMode = g_UIState.Mode;
                            g_Config->ManFanSpeed = g_UIState.ManualLevel;
                            g_Config->ControlAlgorithm = (int)g_UIState.Algorithm;
                            g_Config->PID_Target = g_UIState.PID.targetTemp;
                            g_Config->PID_Kp = g_UIState.PID.Kp;
                            g_Config->PID_Ki = g_UIState.PID.Ki;
                            g_Config->PID_Kd = g_UIState.PID.Kd;
                        }
                        if (g_Config->SaveConfig("TPFanCtrl2.ini")) {
                            g_AppLog.AddLog("[Config] %s", _TR("LOG_SAVE_SUCCESS"));
                        } else {
                            g_AppLog.AddLog("[Config] %s", _TR("LOG_SAVE_ERROR"));
                        }
                    }
                }
                ImGui::EndChild();

                ImGui::SameLine();

                // --- Content Area ---
                ImGui::BeginChild("SettingsContent", ImVec2(0, 0), false);
                ImGui::Indent(10 * dpiScale);
                ImGui::Spacing();

                if (g_UIState.SelectedSettingsTab == 0) {
                    // --- General Settings ---
                    ImGui::TextColored(ImVec4(0.89f, 0.12f, 0.16f, 1.0f), "%s", _TR("SIDEBAR_GENERAL"));
                    ImGui::Separator();
                    ImGui::Spacing();

                    ImGui::BeginChild("BehaviorGroup", ImVec2(0, 220 * dpiScale), true);
                    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s", _TR("SETTING_BEHAVIOR"));
                    ImGui::Spacing();
                    {
                        bool startMin = g_Config->StartMinimized != 0;
                        if (ImGui::Checkbox(_TR("OPT_START_MINIMIZED"), &startMin)) g_Config->StartMinimized = startMin;
                        
                        bool minToTray = g_Config->MinimizeToSysTray != 0;
                        if (ImGui::Checkbox(_TR("OPT_MINIMIZE_TRAY"), &minToTray)) g_Config->MinimizeToSysTray = minToTray;
                        
                        bool minOnClose = g_Config->MinimizeOnClose != 0;
                        if (ImGui::Checkbox(_TR("OPT_MINIMIZE_CLOSE"), &minOnClose)) g_Config->MinimizeOnClose = minOnClose;

                        ImGui::Spacing();
                        ImGui::Text("Language / \xE8\xAF\xAD\xE8\xA8\x80:");
                        const auto& langs = I18nManager::Get().GetAvailableLanguages();
                        if (ImGui::BeginCombo("##Lang", I18nManager::Get().GetCurrentLanguage() == "zh" ? "\xE7\xAE\x80\xE4\xBD\x93\xE4\xB8\xAD\xE6\x96\x87" : "English")) {
                            for (const auto& lang : langs) {
                                if (ImGui::Selectable(lang.name.c_str(), I18nManager::Get().GetCurrentLanguage() == lang.code)) {
                                    I18nManager::Get().SetLanguage(lang.code);
                                    g_Config->Language = lang.code;
                                }
                            }
                            ImGui::EndCombo();
                        }
                    }
                    ImGui::EndChild();

                    ImGui::BeginChild("PollingGroup", ImVec2(0, 160 * dpiScale), true);
                    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s", _TR("SETTING_POLLING"));
                    ImGui::Spacing();
                    {
                        int cycle = g_Config->Cycle;
                        ImGui::Text(_TR("LBL_CYCLE"));
                        ImGui::PushItemWidth(200 * dpiScale);
                        if (ImGui::InputInt("##Cycle", &cycle)) {
                            if (cycle < 1) cycle = 1;
                            if (cycle > 60) cycle = 60;
                            g_Config->Cycle = cycle;
                        }
                        ImGui::PopItemWidth();
                        ImGui::TextDisabled("%s: %ds", _TR("LBL_REFRESH_SENSOR"), cycle);
                        ImGui::TextDisabled("%s: 1s", _TR("LBL_REFRESH_FAN"));
                        ImGui::TextDisabled("%s: 100ms (10Hz)", _TR("LBL_REFRESH_CTRL"));
                    }
                    ImGui::EndChild();
                }
                else if (g_UIState.SelectedSettingsTab == 1) {
                    // --- PID Tuning ---
                    ImGui::TextColored(ImVec4(0.89f, 0.12f, 0.16f, 1.0f), "%s", _TR("SIDEBAR_PID"));
                    ImGui::Separator();
                    ImGui::Spacing();

                    ImGui::BeginChild("PIDGroup", ImVec2(0, 350 * dpiScale), true);
                    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s", _TR("SETTING_PID"));
                    ImGui::Spacing();
                    {
                        std::lock_guard<std::mutex> lock(g_UIState.Mutex);
                        float itemWidth = 200 * dpiScale;
                        
                        ImGui::Text("%s:", _TR("LBL_TARGET_TEMP")); ImGui::SameLine(150 * dpiScale);
                        ImGui::PushItemWidth(itemWidth);
                        ImGui::InputFloat("##Target", &g_UIState.PID.targetTemp, 1.0f, 5.0f, "%.1f\xC2\xB0\x43");
                        ImGui::PopItemWidth();

                        ImGui::Text("%s:", _TR("LBL_KP")); ImGui::SameLine(150 * dpiScale);
                        ImGui::PushItemWidth(itemWidth);
                        ImGui::InputFloat("##Kp", &g_UIState.PID.Kp, 0.01f, 0.1f, "%.3f");
                        ImGui::PopItemWidth();

                        ImGui::Text("%s:", _TR("LBL_KI")); ImGui::SameLine(150 * dpiScale);
                        ImGui::PushItemWidth(itemWidth);
                        ImGui::InputFloat("##Ki", &g_UIState.PID.Ki, 0.001f, 0.01f, "%.4f");
                        ImGui::PopItemWidth();

                        ImGui::Text("%s:", _TR("LBL_KD")); ImGui::SameLine(150 * dpiScale);
                        ImGui::PushItemWidth(itemWidth);
                        ImGui::InputFloat("##Kd", &g_UIState.PID.Kd, 0.01f, 0.1f, "%.3f");
                        ImGui::PopItemWidth();
                        
                        ImGui::Spacing();
                        ImGui::Separator();
                        if (ImGui::Button(_TR("BTN_RESET_PID"), ImVec2(200 * dpiScale, 35 * dpiScale))) {
                            g_UIState.PID.targetTemp = 60.0f;
                            g_UIState.PID.Kp = 0.5f;
                            g_UIState.PID.Ki = 0.01f;
                            g_UIState.PID.Kd = 0.1f;
                        }
                    }
                    ImGui::EndChild();

                    // --- PID Step Response Preview ---
                    ImGui::Spacing();
                    ImGui::BeginChild("PIDPreview", ImVec2(0, 220 * dpiScale), true);
                    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s", _TR("SETTING_PID_PREVIEW"));
                    ImGui::TextDisabled("%s", _TR("DESC_PID_PREVIEW"));
                    ImGui::Spacing();
                    
                    // Simulate PID step response (temperature jumps from 50 to 70, target is user's setting)
                    static float pidResponse[100];
                    static float lastKp = -1, lastKi = -1, lastKd = -1, lastTarget = -1;
                    
                    // Only recalculate if parameters changed
                    if (lastKp != g_UIState.PID.Kp || lastKi != g_UIState.PID.Ki || 
                        lastKd != g_UIState.PID.Kd || lastTarget != g_UIState.PID.targetTemp) {
                        
                        lastKp = g_UIState.PID.Kp;
                        lastKi = g_UIState.PID.Ki;
                        lastKd = g_UIState.PID.Kd;
                        lastTarget = g_UIState.PID.targetTemp;
                        
                        float simTemp = 70.0f;  // Simulated current temperature (spike)
                        float integral = 0.0f;
                        float prevError = simTemp - lastTarget;
                        
                        for (int i = 0; i < 100; i++) {
                            float error = simTemp - lastTarget;
                            integral += error * 0.1f;
                            integral = std::clamp(integral, -50.0f, 50.0f);
                            float derivative = (error - prevError) / 0.1f;
                            
                            float output = lastKp * error + lastKi * integral + lastKd * derivative;
                            output = std::clamp(output, 0.0f, 100.0f);
                            pidResponse[i] = output;
                            
                            // Simulate temperature drop due to fan (simplified thermal model)
                            float coolingPower = output * 0.15f;
                            simTemp -= coolingPower * 0.1f;
                            simTemp = (std::max)(simTemp, lastTarget - 5.0f);
                            
                            prevError = error;
                        }
                    }
                    
                    // Draw the response curve
                    ImGui::PlotLines("##PIDResponse", pidResponse, 100, 0, nullptr, 0.0f, 100.0f, 
                                     ImVec2(ImGui::GetContentRegionAvail().x, 120 * dpiScale));
                    
                    // Legend
                    ImGui::TextDisabled("0%% = %s, 100%% = %s", _TR("LBL_FAN_OFF"), _TR("LBL_FAN_MAX"));
                    ImGui::EndChild();
                }
                else if (g_UIState.SelectedSettingsTab == 2) {
                    // --- Sensors ---
                    ImGui::TextColored(ImVec4(0.89f, 0.12f, 0.16f, 1.0f), "%s", _TR("SIDEBAR_SENSORS"));
                    ImGui::Separator();
                    ImGui::Spacing();

                    ImGui::BeginChild("SensorGroup", ImVec2(0, 0), true);
                    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s", _TR("SETTING_SENSORS"));
                    ImGui::Spacing();
                    ImGui::TextDisabled("%s", _TR("DESC_SENSORS"));
                    ImGui::Spacing();
                    {
                        std::lock_guard<std::mutex> lock(g_UIState.Mutex);
                        if (ImGui::BeginTable("SensorsIgnore", 3, ImGuiTableFlags_NoSavedSettings)) {
                            for (const auto& s : g_UIState.Sensors) {
                                ImGui::TableNextColumn();
                                
                                bool ignored = g_Config->IgnoreSensors.find(s.name) != std::string::npos;
                                
                                if (!s.isAvailable) ImGui::BeginDisabled();
                                
                                if (ImGui::Checkbox(s.name.c_str(), &ignored)) {
                                    if (ignored) {
                                        if (g_Config->IgnoreSensors.find(s.name) == std::string::npos)
                                            g_Config->IgnoreSensors += s.name + " ";
                                    } else {
                                        size_t pos = g_Config->IgnoreSensors.find(s.name);
                                        if (pos != std::string::npos)
                                            g_Config->IgnoreSensors.erase(pos, s.name.length() + 1);
                                    }
                                }
                                
                                if (!s.isAvailable) ImGui::EndDisabled();
                                
                                if (ImGui::IsItemHovered() && !s.isAvailable) {
                                    ImGui::SetTooltip(_TR("TIP_SENSOR_UNAVAILABLE"), s.addr);
                                }
                            }
                            ImGui::EndTable();
                        }
                    }
                    ImGui::EndChild();
                }

                ImGui::Unindent(10 * dpiScale);
                ImGui::EndChild();

                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::End();

        ImGui::Render();
        ImDrawData* draw_data = ImGui::GetDrawData();
        
        ImGui_ImplVulkanH_Frame* fd = &wd->Frames[wd->FrameIndex];
        vkWaitForFences(g_Device, 1, &fd->Fence, VK_TRUE, UINT64_MAX);
        vkResetFences(g_Device, 1, &fd->Fence);
        vkResetCommandPool(g_Device, fd->CommandPool, 0);

        VkCommandBufferBeginInfo b_info = {};
        b_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        b_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(fd->CommandBuffer, &b_info);

        VkRenderPassBeginInfo rp_info = {};
        rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rp_info.renderPass = wd->RenderPass;
        rp_info.framebuffer = fd->Framebuffer;
        rp_info.renderArea.extent.width = wd->Width;
        rp_info.renderArea.extent.height = wd->Height;
        rp_info.clearValueCount = 1;
        VkClearValue clear_value = { {{0.0f, 0.0f, 0.0f, 0.0f}} };
        rp_info.pClearValues = &clear_value;
        vkCmdBeginRenderPass(fd->CommandBuffer, &rp_info, VK_SUBPASS_CONTENTS_INLINE);

        ImGui_ImplVulkan_RenderDrawData(draw_data, fd->CommandBuffer);

        vkCmdEndRenderPass(fd->CommandBuffer);
        vkEndCommandBuffer(fd->CommandBuffer);

        VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo s_info = {};
        s_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        s_info.waitSemaphoreCount = 1;
        s_info.pWaitSemaphores = &image_acquired_semaphore;
        s_info.pWaitDstStageMask = &wait_stage;
        s_info.commandBufferCount = 1;
        s_info.pCommandBuffers = &fd->CommandBuffer;
        s_info.signalSemaphoreCount = 1;
        s_info.pSignalSemaphores = &render_complete_semaphore;
        vkQueueSubmit(g_Queue, 1, &s_info, fd->Fence);

        VkPresentInfoKHR p_info = {};
        p_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        p_info.waitSemaphoreCount = 1;
        p_info.pWaitSemaphores = &render_complete_semaphore;
        p_info.swapchainCount = 1;
        p_info.pSwapchains = &wd->Swapchain;
        p_info.pImageIndices = &wd->FrameIndex;
        err = vkQueuePresentKHR(g_Queue, &p_info);
        if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR) {
            g_SwapChainRebuild = true;
        }

        wd->SemaphoreIndex = (wd->SemaphoreIndex + 1) % wd->SemaphoreCount;
    }

    Log(LOG_INFO, "Exiting main loop. Starting cleanup...");

    // Cleanup
    hwRunning = false;
    if (hwThread.joinable()) {
        Log(LOG_INFO, "Waiting for hardware thread to stop...");
        hwThread.join();
    }

    Log(LOG_INFO, "Shutting down ImGui backends...");
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    Log(LOG_INFO, "Destroying Vulkan resources...");
    vkDestroySurfaceKHR(g_Instance, surface, g_Allocator);
    ImGui_ImplVulkanH_DestroyWindow(g_Instance, g_Device, &g_MainWindowData, g_Allocator);
    vkDestroyDescriptorPool(g_Device, g_DescriptorPool, g_Allocator);
    vkDestroyDevice(g_Device, g_Allocator);
    vkDestroyInstance(g_Instance, g_Allocator);

    CloseTVicPort();
    Log(LOG_INFO, "TVicPort driver closed.");

    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    Log(LOG_INFO, "Application terminated successfully.");
    return 0;
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;
    
    switch (msg) {
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED) {
            g_SwapChainRebuild = true;
            g_SwapChainResizeWidth = (int)LOWORD(lParam);
            g_SwapChainResizeHeight = (int)HIWORD(lParam);
        }
        return 0;
    case WM_PAINT:
        if (g_SwapChainRebuild) {
            PAINTSTRUCT ps;
            BeginPaint(hWnd, &ps);
            EndPaint(hWnd, &ps);
            return 0;
        }
        break;
    case WM_GETMINMAXINFO:
        {
            MINMAXINFO* mmi = (MINMAXINFO*)lParam;
            // Increased minimum size to prevent layout breaking
            mmi->ptMinTrackSize.x = 800;
            mmi->ptMinTrackSize.y = 600;
        }
        return 0;
    case WM_TRAYICON:
        if (LOWORD(lParam) == WM_LBUTTONDBLCLK) {
            ShowWindow(hWnd, SW_RESTORE);
            SetForegroundWindow(hWnd);
        } else if (LOWORD(lParam) == WM_RBUTTONUP) {
            POINT curPoint;
            GetCursorPos(&curPoint);
            HMENU hMenu = CreatePopupMenu();
            InsertMenuW(hMenu, 0, MF_BYPOSITION | MF_STRING, ID_TRAY_RESTORE, L"Restore");
            InsertMenuW(hMenu, 1, MF_BYPOSITION | MF_STRING, ID_TRAY_EXIT, L"Exit");
            SetForegroundWindow(hWnd);
            TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, curPoint.x, curPoint.y, 0, hWnd, NULL);
            DestroyMenu(hMenu);
        }
        break;
    case WM_COMMAND:
        if (LOWORD(wParam) == ID_TRAY_RESTORE) {
            ShowWindow(hWnd, SW_RESTORE);
            SetForegroundWindow(hWnd);
        } else if (LOWORD(wParam) == ID_TRAY_EXIT) {
            PostQuitMessage(0);
        }
        break;
    case WM_SYSCOMMAND:
        if ((wParam & 0xFFF0) == SC_MINIMIZE) {
            if (g_Config && g_Config->MinimizeToSysTray) {
                ShowWindow(hWnd, SW_HIDE);
                return 0;
            }
        }
        break;
    case WM_CLOSE:
        if (g_Config && g_Config->MinimizeOnClose) {
            ShowWindow(hWnd, SW_HIDE);
            return 0;
        }
        break;
    case WM_DESTROY:
        RemoveTrayIcon(hWnd);
        ::PostQuitMessage(0);
        return 0;
    case WM_DPICHANGED:
        if (lParam != NULL) {
            const RECT* prcNewWindow = (RECT*)lParam;
            SetWindowPos(hWnd, NULL, prcNewWindow->left, prcNewWindow->top,
                prcNewWindow->right - prcNewWindow->left,
                prcNewWindow->bottom - prcNewWindow->top,
                SWP_NOZORDER | SWP_NOACTIVATE);
        }
        break;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}
