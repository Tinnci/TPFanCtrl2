#include "Application.h"
#include "TVicPortProvider.h"
#include "TVicPort.h"
#include <spdlog/spdlog.h>
#include <vulkan/vulkan_win32.h>

namespace App {

/// Build ThermalConfig from the legacy ConfigManager
/// This is a bridge function for the migration period
Core::ThermalConfig BuildThermalConfig(const std::shared_ptr<ConfigManager>& config) {
    Core::ThermalConfig thermal;
    
    // Basic settings
    thermal.cycleSeconds = config->Cycle;
    thermal.iconCycleSeconds = config->IconCycle;
    thermal.isDualFan = config->DualFan != 0;
    thermal.fanSpeedAddr = config->FanSpeedLowByte;
    thermal.useBiasedTemps = config->ShowBiasedTemps != 0;
    thermal.noExtSensor = config->NoExtSensor != 0;
    thermal.useFahrenheit = config->Fahrenheit != 0;
    thermal.manualFanSpeed = config->ManFanSpeed;
    thermal.manModeExitTemp = config->ManModeExit;
    thermal.ignoreList = config->IgnoreSensors;
    
    // PID settings
    thermal.pid.Kp = config->PID_Kp;
    thermal.pid.Ki = config->PID_Ki;
    thermal.pid.Kd = config->PID_Kd;
    thermal.pid.targetTemp = config->PID_Target;
    thermal.pid.minFan = 0;
    thermal.pid.maxFan = 7;
    
    // Sensor configuration - use defaults and apply names/weights from config
    thermal.sensors = Core::CreateDefaultSensorConfig();
    
    const char* defaultNames[] = {
        "CPU", "APS", "PCM", "GPU", "BAT1", "X7D", 
        "BAT2", "X7F", "BUS", "PCI", "PWR", "XC3"
    };

    for (size_t i = 0; i < thermal.sensors.size(); i++) {
        // Use name from config if provided, otherwise use default ThinkPad name
        if (i < config->SensorNames.size() && !config->SensorNames[i].empty()) {
            thermal.sensors[i].name = config->SensorNames[i];
        } else if (i < 12) {
            thermal.sensors[i].name = defaultNames[i];
        }
    }

    for (size_t i = 0; i < config->SensorWeights.size() && i < thermal.sensors.size(); i++) {
        thermal.sensors[i].weight = config->SensorWeights[i];
    }
    
    // Smart profiles - convert SmartLevels1/2 to SmartLevelDefinition
    for (const auto& sl : config->SmartLevels1) {
        if (sl.temp >= 0) {
            thermal.smartProfiles[0].emplace_back(sl.temp, sl.fan, sl.hystUp, sl.hystDown);
        }
    }
    for (const auto& sl : config->SmartLevels2) {
        if (sl.temp >= 0) {
            thermal.smartProfiles[1].emplace_back(sl.temp, sl.fan, sl.hystUp, sl.hystDown);
        }
    }
    
    // Icon levels
    if (config->IconLevels.size() >= 3) {
        thermal.iconLevels.thresholds = { 
            config->IconLevels[0], 
            config->IconLevels[1], 
            config->IconLevels[2] 
        };
    }
    
    return thermal;
}

Application::Application() {
    m_config = std::make_shared<ConfigManager>();
}

Application::~Application() {
    Shutdown();
}

bool Application::Initialize(HWND hwnd, HINSTANCE hInstance) {
    m_hwnd = hwnd;
    
    // Initialize config manager
    m_config = std::make_shared<ConfigManager>();
    
    // Load config
    if (!m_config->LoadConfig("TPFanCtrl2.json")) {
        spdlog::warn("Failed to load TPFanCtrl2.json, using defaults.");
    }

    spdlog::info("Config loaded, initializing Vulkan...");
    // Initialize Vulkan
    if (!InitVulkan(hwnd, hInstance)) {
        spdlog::error("Failed to initialize Vulkan.");
        return false;
    }
    spdlog::info("Vulkan initialized successfully.");

    // Initialize hardware access
    spdlog::info("Initializing TVicPort driver...");
    bool driverOk = false;
    for (int i = 0; i < 5; i++) {
        if (OpenTVicPort()) {
            driverOk = true;
            break;
        }
        spdlog::warn("Failed to open TVicPort, retrying... ({}/5)", i + 1);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    if (driverOk) {
        spdlog::info("TVicPort driver opened successfully.");
        SetHardAccess(TRUE);
        if (TestHardAccess()) {
            spdlog::info("Hardware access (Ring 0) granted.");
        } else {
            spdlog::error("Hardware access denied even with driver opened.");
        }
    } else {
        spdlog::error("CRITICAL: Could not initialize TVicPort driver.");
        return false;
    }

    auto ioProvider = std::make_shared<TVicPortProvider>();
    auto ecManager = std::make_shared<ECManager>(ioProvider, [](const char* msg) {
        spdlog::debug("[EC] {}", msg);
    });

    // Initialize Core components
    Core::ThermalConfig thermalConfig = BuildThermalConfig(m_config);
    m_thermalManager = std::make_shared<Core::ThermalManager>(ecManager, thermalConfig);
    m_uiAdapter = std::make_unique<Core::UIAdapter>(m_thermalManager);

    // Start the thermal manager
    m_thermalManager->Start();

    return true;
}

void Application::Shutdown() {
    if (m_thermalManager && m_thermalManager->IsRunning()) {
        m_thermalManager->Stop();
    }
    m_uiAdapter.reset();
    m_thermalManager.reset();
    
    CleanupVulkan();
    CloseTVicPort();
}

void Application::Update(float deltaTime) {
    if (m_uiAdapter) {
        m_uiAdapter->Update(deltaTime);
    }
}

void Application::Render() {
    // Rendering logic moved from main loop
}

void Application::SetupVulkan(const char** extensions, uint32_t extensions_count) {
    VkResult err;
    VkInstanceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    
    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.apiVersion = VK_API_VERSION_1_2;
    create_info.pApplicationInfo = &app_info;
    
    create_info.enabledExtensionCount = extensions_count;
    create_info.ppEnabledExtensionNames = extensions;
    err = vkCreateInstance(&create_info, m_allocator, &Instance);
    if (err != VK_SUCCESS) { spdlog::error("vkCreateInstance failed: {}", (int)err); exit(1); }

    uint32_t gpu_count;
    vkEnumeratePhysicalDevices(Instance, &gpu_count, nullptr);
    if (gpu_count == 0) { spdlog::error("No GPU found"); exit(1); }
    
    std::vector<VkPhysicalDevice> gpus(gpu_count);
    vkEnumeratePhysicalDevices(Instance, &gpu_count, gpus.data());
    PhysicalDevice = gpus[0];

    uint32_t count;
    vkGetPhysicalDeviceQueueFamilyProperties(PhysicalDevice, &count, nullptr);
    std::vector<VkQueueFamilyProperties> queues(count);
    vkGetPhysicalDeviceQueueFamilyProperties(PhysicalDevice, &count, queues.data());
    for (uint32_t i = 0; i < count; i++)
        if (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) { QueueFamily = i; break; }

    float queue_priority[] = { 1.0f };
    VkDeviceQueueCreateInfo queue_info = {};
    queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info.queueFamilyIndex = QueueFamily;
    queue_info.queueCount = 1;
    queue_info.pQueuePriorities = queue_priority;
    
    const char* device_extensions[] = { "VK_KHR_swapchain" };
    VkDeviceCreateInfo device_info = {};
    device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_info.queueCreateInfoCount = 1;
    device_info.pQueueCreateInfos = &queue_info;
    device_info.enabledExtensionCount = 1;
    device_info.ppEnabledExtensionNames = device_extensions;
    err = vkCreateDevice(PhysicalDevice, &device_info, m_allocator, &Device);
    if (err != VK_SUCCESS) { spdlog::error("vkCreateDevice failed: {}", (int)err); exit(1); }
    vkGetDeviceQueue(Device, QueueFamily, 0, &Queue);

    VkDescriptorPoolSize pool_sizes[] = { { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100 } };
    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 100;
    pool_info.poolSizeCount = 1;
    pool_info.pPoolSizes = pool_sizes;
    vkCreateDescriptorPool(Device, &pool_info, m_allocator, &DescriptorPool);
}

void Application::SetupVulkanWindow(ImGui_ImplVulkanH_Window* wd, VkSurfaceKHR surface, int width, int height) {
    wd->Surface = surface;
    const VkFormat requestSurfaceImageFormat[] = { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM };
    const VkColorSpaceKHR requestSurfaceColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    wd->SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(PhysicalDevice, wd->Surface, requestSurfaceImageFormat, 2, requestSurfaceColorSpace);
    VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_FIFO_KHR };
    wd->PresentMode = ImGui_ImplVulkanH_SelectPresentMode(PhysicalDevice, wd->Surface, present_modes, 1);
    ImGui_ImplVulkanH_CreateOrResizeWindow(Instance, PhysicalDevice, Device, wd, QueueFamily, m_allocator, width, height, 2, 0);
}

bool Application::InitVulkan(HWND hwnd, HINSTANCE hInstance) {
    // NOTE: SetupVulkan() is already called in main() before Application is created
    // We only need to store the surface reference that was created in main()
    // This is a no-op for now - Vulkan is already initialized
    spdlog::info("Application::InitVulkan - Vulkan already initialized in main()");
    return true;
}

void Application::CleanupVulkan() {
    if (Device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(Device);
        vkDestroySurfaceKHR(Instance, m_surface, m_allocator);
        ImGui_ImplVulkanH_DestroyWindow(Instance, Device, &MainWindowData, m_allocator);
        vkDestroyDescriptorPool(Device, DescriptorPool, m_allocator);
        vkDestroyDevice(Device, m_allocator);
        vkDestroyInstance(Instance, m_allocator);
        
        Device = VK_NULL_HANDLE;
        Instance = VK_NULL_HANDLE;
    }
}

} // namespace App
