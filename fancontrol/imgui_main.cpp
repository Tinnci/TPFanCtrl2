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

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_vulkan.h"
#include "imgui_freetype.h"
#include "implot.h"

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

// Project includes
#include "ConfigManager.h"
#include "SensorManager.h"
#include "FanController.h"
#include "ECManager.h"
#include "TVicPortProvider.h"

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

// --- Logging System ---
enum LogLevel { LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR };
static LogLevel g_MinLogLevel = LOG_INFO;

void Log(LogLevel level, const char* fmt, ...) {
    if (level < g_MinLogLevel) return;
    const char* prefix = "[INFO] ";
    if (level == LOG_DEBUG) prefix = "[DEBUG] ";
    if (level == LOG_WARN)  prefix = "[WARN]  ";
    if (level == LOG_ERROR) prefix = "[ERROR] ";
    
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, 1024, fmt, args);
    va_end(args);

    // Print to console
    printf("%s%s\n", prefix, buf);

    // Also write to file for elevated process diagnostics
    FILE* f = fopen("TPFanCtrl2_debug.log", "a");
    if (f) {
        fprintf(f, "%s%s\n", prefix, buf);
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
    while (*running) {
        // 1. Update Hardware
        sensors->UpdateSensors(config->ShowBiasedTemps, config->NoExtSensor, config->UseTWR);
        
        int maxIdx = 0;
        int maxTemp = sensors->GetMaxTemp(maxIdx);
        fans->UpdateSmartControl(maxTemp, config->SmartLevels1);

        // 2. Sync to UI State
        {
            std::lock_guard<std::mutex> lock(g_UIState.Mutex);
            g_UIState.Sensors = sensors->GetSensors();
            for (const auto& s : g_UIState.Sensors) {
                g_UIState.SmoothTemps[s.name].Target = (float)s.rawTemp;
                auto& history = g_UIState.TempHistory[s.name];
                history.push_back((float)s.rawTemp);
                if (history.size() > 100) history.pop_front();
            }
            fans->GetFanSpeeds(g_UIState.Fan1Speed, g_UIState.Fan2Speed);
        }

        // 3. Throttle (Hardware doesn't need 60FPS)
        std::this_thread::sleep_for(std::chrono::milliseconds(config->Cycle * 1000));
    }
}

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// --- Helper Functions ---
bool IsUserAdmin() {
    BOOL isAdmin = FALSE;
    PSID adminGroup = NULL;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
        CheckTokenMembership(NULL, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    return isAdmin;
}

int main(int argc, char** argv) {
    // Clear old log file for a fresh session
    remove("TPFanCtrl2_debug.log");
    Log(LOG_INFO, "--- TPFanCtrl2 Session Started ---");

    // 0. Check Privileges
    bool isAdmin = IsUserAdmin();
    Log(LOG_INFO, "Running as Administrator: %s", isAdmin ? "YES" : "NO");

    if (!isAdmin) {
        int result = MessageBoxW(NULL, 
            L"TPFanCtrl2 requires Administrator privileges to access hardware (TVicPort).\n\n"
            L"Would you like to restart as Administrator?", 
            L"Privilege Elevation Required", 
            MB_YESNO | MB_ICONWARNING);

        if (result == IDYES) {
            wchar_t szPath[MAX_PATH];
            GetModuleFileNameW(NULL, szPath, MAX_PATH);
            
            wchar_t szDir[MAX_PATH];
            wcscpy(szDir, szPath);
            wchar_t* lastSlash = wcsrchr(szDir, L'\\');
            if (lastSlash) *lastSlash = L'\0';

            SHELLEXECUTEINFOW sei = { sizeof(sei) };
            sei.lpVerb = L"runas";
            sei.lpFile = szPath;
            sei.lpDirectory = szDir; // Set working directory to exe location
            sei.hwnd = NULL;
            sei.nShow = SW_NORMAL;
            
            if (ShellExecuteExW(&sei)) {
                return 0;
            } else {
                DWORD err = GetLastError();
                wchar_t msg[256];
                swprintf(msg, 256, L"Failed to elevate privileges. Error code: %lu", err);
                MessageBoxW(NULL, msg, L"Elevation Error", MB_OK | MB_ICONERROR);
            }
        }
        // If user chose NO or elevation failed, we continue but log a warning
        Log(LOG_WARN, "Running without Administrator privileges. Hardware access will likely fail.");
    }

    HINSTANCE hInstance = GetModuleHandle(NULL);
    // Logic Init
    auto configManager = std::make_shared<ConfigManager>();
    configManager->LoadConfig("TPFanCtrl2.ini");
    auto ecManager = std::make_shared<ECManager>(std::make_shared<TVicPortProvider>(), [](const char* msg) { g_AppLog.AddLog("[EC] %s", msg); });
    auto sensorManager = std::make_shared<SensorManager>(ecManager);
    auto fanController = std::make_shared<FanController>(ecManager);

    // Start Hardware Thread
    bool hwRunning = true;
    std::thread hwThread(HardwareWorker, configManager, sensorManager, fanController, &hwRunning);

    // Window Init
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"TPFanCtrl2Vulkan", nullptr };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"TPFanCtrl2 - Vulkan Modernized", WS_OVERLAPPEDWINDOW, 100, 100, 1024, 768, nullptr, nullptr, wc.hInstance, nullptr);

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
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    
    Log(LOG_INFO, "Loading fonts...");
    // 1. Optimize Font Rendering with FreeType
    ImFontConfig font_cfg;
    font_cfg.FontLoaderFlags |= ImGuiFreeTypeBuilderFlags_LightHinting;
    if (!io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 18.0f, &font_cfg)) {
        Log(LOG_WARN, "Failed to load segoeui.ttf");
    }
    
    // 2. Load Icons (Segoe MDL2 Assets is built-in on Windows)
    static const ImWchar icon_ranges[] = { 0xE700, 0xF800, 0 };
    ImFontConfig icon_cfg;
    icon_cfg.MergeMode = true;
    icon_cfg.PixelSnapH = true;
    if (!io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segmdl2.ttf", 16.0f, &icon_cfg, icon_ranges)) {
        Log(LOG_WARN, "Failed to load segmdl2.ttf");
    }
    
    ImGui::StyleColorsDark();
    auto& style = ImGui::GetStyle();
    style.WindowRounding = 8.0f;
    style.FrameRounding = 4.0f;
    style.ItemSpacing = ImVec2(10, 8);
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.1f, 0.1f, 0.1f, 0.7f);
    style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.0f, 0.6f, 0.9f, 1.0f);

    Log(LOG_INFO, "Initializing Win32 backend...");
    ImGui_ImplWin32_Init(hwnd);
    
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

    Log(LOG_INFO, "Showing window...");
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);

    bool done = false;
    Log(LOG_INFO, "Entering main loop...");
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

        Log(LOG_DEBUG, "NewFrame...");
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        Log(LOG_DEBUG, "Update Animations...");
        float dt = ImGui::GetIO().DeltaTime;
        {
            std::lock_guard<std::mutex> lock(g_UIState.Mutex);
            for (auto& pair : g_UIState.SmoothTemps) {
                pair.second.Update(dt);
            }
        }

        Log(LOG_DEBUG, "Dashboard...");
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin("Dashboard", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBackground);

        ImGui::TextColored(ImVec4(0.0f, 0.8f, 1.0f, 1.0f), "TPFanCtrl2 Modern Dashboard (Vulkan)");
        ImGui::Separator();

        if (ImGui::BeginTable("Layout", 2, ImGuiTableFlags_Resizable)) {
            ImGui::TableNextColumn();
            ImGui::BeginChild("Sensors", ImVec2(0, 300), true);
            ImGui::Text("Temperature Sensors");
            ImGui::Separator();
            
            {
                std::lock_guard<std::mutex> lock(g_UIState.Mutex);
                for (const auto& s : g_UIState.Sensors) {
                    if (s.rawTemp > 0 && s.rawTemp < 128) {
                        float currentTemp = g_UIState.SmoothTemps[s.name].Current;
                        float progress = currentTemp / 100.0f;
                        
                        // Dynamic color based on temperature
                        ImVec4 color = ImVec4(0.0f, 0.6f, 0.9f, 1.0f); // Blue
                        if (currentTemp > 60.0f) color = ImVec4(0.9f, 0.6f, 0.0f, 1.0f); // Orange
                        if (currentTemp > 80.0f) color = ImVec4(0.9f, 0.1f, 0.1f, 1.0f); // Red
                        
                        const char* icon = ICON_CPU;
                        if (s.name.find("GPU") != std::string::npos) icon = ICON_GPU;

                        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, color);
                        ImGui::Text("%s %-10s: %.1f C", icon, s.name.c_str(), currentTemp);
                        ImGui::ProgressBar(progress, ImVec2(-1, 0), "");
                        ImGui::PopStyleColor();
                    }
                }
            }
            ImGui::EndChild();

            ImGui::BeginChild("Fan", ImVec2(0, 150), true);
            ImGui::Text("%s Fan Status", ICON_FAN);
            ImGui::Separator();
            
            int f1, f2;
            {
                std::lock_guard<std::mutex> lock(g_UIState.Mutex);
                f1 = g_UIState.Fan1Speed;
                f2 = g_UIState.Fan2Speed;
            }
            ImGui::Text("Fan 1: %d RPM", f1);
            ImGui::Text("Fan 2: %d RPM", f2);
            
            static int mLevel = 0;
            ImGui::SliderInt("Manual", &mLevel, 0, 7);
            if (ImGui::Button("Apply Manual", ImVec2(-1, 0))) {
                fanController->SetFanLevel(mLevel);
            }
            ImGui::EndChild();

            ImGui::BeginChild("History", ImVec2(0, 0), true);
            ImGui::Text("Temperature History");
            /*
            if (ImPlot::BeginPlot("##TempPlot", ImVec2(-1, -1))) {
                ImPlot::SetupAxes("Time", "Celsius", ImPlotAxisFlags_NoTickLabels, 0);
                ImPlot::SetupAxisLimits(ImAxis_Y1, 30, 100, ImGuiCond_Once);
                
                std::lock_guard<std::mutex> lock(g_UIState.Mutex);
                for (auto& pair : g_UIState.TempHistory) {
                    if (pair.second.size() > 1) {
                        std::vector<float> data(pair.second.begin(), pair.second.end());
                        ImPlot::PlotLine(pair.first.c_str(), data.data(), (int)data.size());
                    }
                }
                ImPlot::EndPlot();
            }
            */
            ImGui::EndChild();

            ImGui::TableNextColumn();
            ImGui::BeginChild("Logs", ImVec2(0, -200), true);
            ImGui::Text("System Logs");
            ImGui::Separator();
            {
                std::lock_guard<std::mutex> lock(g_AppLog.Mutex);
                for (const auto& line : g_AppLog.Items) ImGui::TextUnformatted(line.c_str());
                if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f);
            }
            ImGui::EndChild();

            ImGui::BeginChild("HardwareInfo", ImVec2(0, 0), true);
            ImGui::Text("Hardware Backend Status");
            ImGui::Separator();
            ImGui::Text("Renderer: Vulkan 1.3");
            
            /*
            VmaTotalStatistics stats;
            vmaCalculateStatistics(g_VmaAllocator, &stats);
            ImGui::Text("VMA Usage: %.2f MB", (float)stats.total.statistics.allocationBytes / (1024.0f * 1024.0f));
            ImGui::Text("VMA Blocks: %u", stats.total.statistics.blockCount);
            */
            
            ImGui::EndChild();
            ImGui::EndTable();
        }
        ImGui::End();

        Log(LOG_DEBUG, "Render...");
        ImGui::Render();
        ImDrawData* draw_data = ImGui::GetDrawData();
        
        Log(LOG_DEBUG, "AcquireNextImage...");
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

        Log(LOG_DEBUG, "RenderDrawData...");
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

    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    Log(LOG_INFO, "Application terminated successfully.");
    return 0;
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;
    if (msg == WM_DESTROY) { ::PostQuitMessage(0); return 0; }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}
