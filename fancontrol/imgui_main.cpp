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

// Icons (Segoe MDL2 Assets - Built-in on Windows 10/11)
#define ICON_CPU "\xEE\xA7\x92" // Processor
#define ICON_GPU "\xEE\xA6\xAB" // Video / GPU
#define ICON_FAN "\xEE\xA7\xB6" // Fan / Ventilation
#define ICON_CHIP "\xEE\xA9\xA7" // Chipset / RAM

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
                    bool* running) {
    int tempCycleCounter = 0;
    int maxTemp = 0;
    while (*running) {
        // 1. High-frequency Polling (Fan Speeds - every 1s)
        int f1 = 0, f2 = 0;
        fans->GetFanSpeeds(f1, f2);
        {
            std::lock_guard<std::mutex> lock(g_UIState.Mutex);
            g_UIState.Fan1Speed = f1;
            g_UIState.Fan2Speed = f2;
        }

        // 2. Low-frequency Polling (Sensors & Control Logic - every 'Cycle' seconds)
        if (tempCycleCounter <= 0) {
            if (!sensors->UpdateSensors(config->ShowBiasedTemps, config->NoExtSensor, config->UseTWR)) {
                Log(LOG_WARN, "Failed to update sensors from EC.");
            }
            
            int maxIdx = 0;
            maxTemp = sensors->GetMaxTemp(maxIdx);
            
            static int logCounter = 0;
            if (logCounter++ % 6 == 0) { // Log every ~30s
                auto allSensors = sensors->GetSensors();
                std::string sensorLog = "";
                for (const auto& s : allSensors) {
                    if (s.rawTemp > 0 && s.rawTemp < 128) {
                        sensorLog += s.name + ":" + std::to_string(s.rawTemp) + " ";
                    }
                }
                Log(LOG_INFO, "Hardware Status: MaxTemp=%d (%s), Fan1=%d, Fan2=%d. All: %s", 
                    maxTemp, (maxIdx >= 0 && maxIdx < (int)allSensors.size()) ? allSensors[maxIdx].name.c_str() : "N/A", 
                    f1, f2, sensorLog.c_str());
            }
        }

        // 3. Control Logic
        {
            std::lock_guard<std::mutex> lock(g_UIState.Mutex);
            if (g_UIState.Mode == 1) { // Manual
                fans->SetFanLevel(g_UIState.ManualLevel);
            } else if (g_UIState.Mode == 2) { // Smart
                if (g_UIState.Algorithm == ControlAlgorithm::Step) {
                    fans->UpdateSmartControl(maxTemp, config->SmartLevels1);
                } else {
                    // PID Control (using 1s dt since this loop runs every 1s)
                    fans->UpdatePIDControl((float)maxTemp, g_UIState.PID, 1.0f);
                }
            }
        }

        // 4. Sync to UI State
        {
            std::lock_guard<std::mutex> lock(g_UIState.Mutex);
            g_UIState.Sensors = sensors->GetSensors();
            g_UIState.LastUpdate = time(nullptr);
            for (const auto& s : g_UIState.Sensors) {
                // Only record history for valid sensors (0 < temp < 128)
                if (s.rawTemp > 0 && s.rawTemp < 128) {
                    g_UIState.SmoothTemps[s.name].Target = (float)s.rawTemp;
                    auto& history = g_UIState.TempHistory[s.name];
                    history.push_back((float)s.rawTemp);
                    if (history.size() > 300) history.pop_front();
                }
            }
        }
        
        if (tempCycleCounter <= 0) {
            tempCycleCounter = config->Cycle > 0 ? config->Cycle : 1;
        }

        // 5. Sleep 1s (granularly)
        for (int i = 0; i < 10 && *running; i++) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        tempCycleCounter--;
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
    auto configManager = std::make_shared<ConfigManager>();
    configManager->LoadConfig("TPFanCtrl2.ini");

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
    sensorManager->SetSensorName(4, "BAT");
    sensorManager->SetSensorName(5, "X7D");
    sensorManager->SetSensorName(6, "BAT");
    sensorManager->SetSensorName(7, "X7F");
    sensorManager->SetSensorName(8, "BUS");
    sensorManager->SetSensorName(9, "PCI");
    sensorManager->SetSensorName(10, "PWR");
    sensorManager->SetSensorName(11, "XC3");

    auto fanController = std::make_shared<FanController>(ecManager);

    fanController->SetOnChangeCallback([](int level) {
        g_AppLog.AddLog("[Fan] Level changed to %d", level);
    });

    // Start Hardware Thread
    bool hwRunning = true;
    std::thread hwThread(HardwareWorker, configManager, sensorManager, fanController, &hwRunning);

    // Window Init
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"TPFanCtrl2Vulkan", nullptr };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"TPFanCtrl2 - Vulkan Modernized", WS_OVERLAPPEDWINDOW, 100, 100, 1024, 768, nullptr, nullptr, wc.hInstance, nullptr);

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
    if (!io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 18.0f * dpiScale, &font_cfg)) {
        Log(LOG_WARN, "Failed to load segoeui.ttf");
    }
    
    // 2. Load Icons (Segoe MDL2 Assets is built-in on Windows)
    static const ImWchar icon_ranges[] = { 0xE700, 0xF800, 0 };
    ImFontConfig icon_cfg;
    icon_cfg.MergeMode = true;
    icon_cfg.PixelSnapH = true;
    if (!io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segmdl2.ttf", 16.0f * dpiScale, &icon_cfg, icon_ranges)) {
        Log(LOG_WARN, "Failed to load segmdl2.ttf");
    }
    
    ImGui::StyleColorsDark();
    auto& style = ImGui::GetStyle();
    style.ScaleAllSizes(dpiScale); // Scale UI elements (padding, spacing, etc.)
    style.WindowRounding = 8.0f * dpiScale;
    style.FrameRounding = 4.0f * dpiScale;
    style.ItemSpacing = ImVec2(10 * dpiScale, 8 * dpiScale);
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.1f, 0.1f, 0.1f, 0.7f);
    style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.0f, 0.6f, 0.9f, 1.0f);

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

    if (configManager->StartMinimized) {
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

        if (ImGui::BeginTabBar("MainTabs")) {
            if (ImGui::BeginTabItem("Dashboard")) {
                if (ImGui::BeginTable("Layout", 2, ImGuiTableFlags_Resizable)) {
                    ImGui::TableNextColumn();
                    
                    // --- Left Column: Sensors & Fan ---
                    ImGui::BeginChild("Sensors", ImVec2(0, 350 * dpiScale), true);
                    ImGui::TextColored(ImVec4(0, 1, 1, 1), "Temperature Sensors");
                    ImGui::Separator();
                    {
                        std::lock_guard<std::mutex> lock(g_UIState.Mutex);
                        for (const auto& s : g_UIState.Sensors) {
                            if (s.rawTemp > 0 && s.rawTemp < 128) {
                                float currentTemp = g_UIState.SmoothTemps[s.name].Current;
                                float progress = currentTemp / 100.0f;
                                ImVec4 color = ImVec4(0.0f, 0.6f, 0.9f, 1.0f);
                                if (currentTemp > 60.0f) color = ImVec4(0.9f, 0.6f, 0.0f, 1.0f);
                                if (currentTemp > 80.0f) color = ImVec4(0.9f, 0.1f, 0.1f, 1.0f);
                                
                                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, color);
                                ImGui::Text("%s %-10s: %.1f C", (s.name.find("GPU") != std::string::npos ? ICON_GPU : ICON_CPU), s.name.c_str(), currentTemp);
                                ImGui::ProgressBar(progress, ImVec2(-1, 0), "");
                                ImGui::PopStyleColor();
                            }
                        }
                    }
                    ImGui::EndChild();

                    ImGui::BeginChild("Control", ImVec2(0, 0), true);
                    ImGui::TextColored(ImVec4(1, 0.8f, 0, 1), "Fan Control");
                    ImGui::Separator();
                    {
                        std::lock_guard<std::mutex> lock(g_UIState.Mutex);
                        ImGui::Text("Fan 1: %d RPM | Fan 2: %d RPM", g_UIState.Fan1Speed, g_UIState.Fan2Speed);
                        
                        ImGui::Spacing();
                        ImGui::RadioButton("BIOS", &g_UIState.Mode, 0); ImGui::SameLine();
                        ImGui::RadioButton("Manual", &g_UIState.Mode, 1); ImGui::SameLine();
                        ImGui::RadioButton("Smart", &g_UIState.Mode, 2);

                        if (g_UIState.Mode == 1) {
                            ImGui::SliderInt("Level", &g_UIState.ManualLevel, 0, 7);
                        } else if (g_UIState.Mode == 2) {
                            const char* algoNames[] = { "Step (阶梯)", "PID (闭环)" };
                            int algoIdx = (int)g_UIState.Algorithm;
                            if (ImGui::Combo("Algorithm", &algoIdx, algoNames, 2)) {
                                g_UIState.Algorithm = (ControlAlgorithm)algoIdx;
                            }
                        }
                    }
                    if (ImGui::Button("Hide to Tray", ImVec2(-1, 40 * dpiScale))) {
                        ::ShowWindow(hwnd, SW_HIDE);
                    }
                    ImGui::EndChild();

                    ImGui::TableNextColumn();
                    
                    // --- Right Column: History & Logs ---
                    ImGui::BeginChild("History", ImVec2(0, 350 * dpiScale), true);
                    ImGui::TextColored(ImVec4(1, 0.5f, 1, 1), "Temperature History");
                    {
                        std::lock_guard<std::mutex> lock(g_UIState.Mutex);
                        DrawSimplePlot("TempPlot", g_UIState.TempHistory, 0, dpiScale);
                    }
                    ImGui::EndChild();

                    ImGui::BeginChild("Logs", ImVec2(0, 0), true);
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1), "System Logs");
                    ImGui::Separator();
                    {
                        std::lock_guard<std::mutex> lock(g_AppLog.Mutex);
                        for (const auto& line : g_AppLog.Items) ImGui::TextUnformatted(line.c_str());
                        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f);
                    }
                    ImGui::EndChild();

                    ImGui::EndTable();
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Settings")) {
                ImGui::BeginChild("SettingsScroll");
                
                ImGui::TextColored(ImVec4(0, 1, 0, 1), "PID Controller Parameters");
                ImGui::Separator();
                {
                    std::lock_guard<std::mutex> lock(g_UIState.Mutex);
                    ImGui::SliderFloat("Target Temp (C)", &g_UIState.PID.targetTemp, 40.0f, 90.0f);
                    ImGui::DragFloat("Kp (Proportional)", &g_UIState.PID.Kp, 0.01f, 0.0f, 10.0f);
                    ImGui::DragFloat("Ki (Integral)", &g_UIState.PID.Ki, 0.001f, 0.0f, 1.0f);
                    ImGui::DragFloat("Kd (Derivative)", &g_UIState.PID.Kd, 0.01f, 0.0f, 5.0f);
                }

                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0, 1, 0, 1), "Hardware Polling");
                ImGui::Separator();
                int cycle = configManager->Cycle;
                if (ImGui::SliderInt("Sensor Cycle (s)", &cycle, 1, 60)) {
                    configManager->Cycle = cycle;
                }
                ImGui::Text("Fan speed is always polled every 1s.");

                ImGui::Spacing();
                if (ImGui::Button("Save Settings to INI", ImVec2(200 * dpiScale, 40 * dpiScale))) {
                    // TODO: Implement SaveConfig
                    Log(LOG_INFO, "Settings saved (simulated).");
                }
                
                ImGui::EndChild();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::End();

        ImGui::Render();
        ImDrawData* draw_data = ImGui::GetDrawData();
        
        ImGui_ImplVulkanH_Window* wd = &g_MainWindowData;
        VkSemaphore image_acquired_semaphore  = wd->FrameSemaphores[wd->SemaphoreIndex].ImageAcquiredSemaphore;
        VkSemaphore render_complete_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
        VkResult err = vkAcquireNextImageKHR(g_Device, wd->Swapchain, UINT64_MAX, image_acquired_semaphore, VK_NULL_HANDLE, &wd->FrameIndex);
        if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR) {
            // Handle resize
            continue;
        }

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
        vkQueuePresentKHR(g_Queue, &p_info);

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
            ShowWindow(hWnd, SW_HIDE);
            return 0;
        }
        break;
    case WM_CLOSE:
        ShowWindow(hWnd, SW_HIDE);
        return 0;
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
