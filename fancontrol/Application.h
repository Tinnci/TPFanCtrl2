#pragma once

#include <memory>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>
#include "ConfigManager.h"
#include "Core/ThermalManager.h"
#include "Core/UIAdapter.h"
#include "imgui_impl_vulkan.h"

namespace App {

// Helper to convert ConfigManager to ThermalConfig
Core::ThermalConfig BuildThermalConfig(const std::shared_ptr<ConfigManager>& config);

/// Application context to encapsulate global state and lifecycle management
class Application {
public:
    Application();
    ~Application();

    // Lifecycle
    bool Initialize(HWND hwnd, HINSTANCE hInstance);
    void Shutdown();
    
    // Main Loop components
    void Update(float deltaTime);
    void Render();

    // Accessors
    std::shared_ptr<ConfigManager> GetConfig() const { return m_config; }
    std::shared_ptr<Core::ThermalManager> GetThermalManager() const { return m_thermalManager; }
    Core::UIAdapter* GetUIAdapter() const { return m_uiAdapter.get(); }
    HWND GetHWND() const { return m_hwnd; }
    
    // Vulkan state
    VkInstance Instance = VK_NULL_HANDLE;
    VkPhysicalDevice PhysicalDevice = VK_NULL_HANDLE;
    VkDevice Device = VK_NULL_HANDLE;
    VkQueue Queue = VK_NULL_HANDLE;
    VkDescriptorPool DescriptorPool = VK_NULL_HANDLE;
    uint32_t QueueFamily = (uint32_t)-1;
    ImGui_ImplVulkanH_Window MainWindowData;
    bool SwapChainRebuild = false;
    int SwapChainResizeWidth = 0;
    int SwapChainResizeHeight = 0;

private:
    bool InitVulkan(HWND hwnd, HINSTANCE hInstance);
    void CleanupVulkan();
    void SetupVulkan(const char** extensions, uint32_t extensions_count);
    void SetupVulkanWindow(ImGui_ImplVulkanH_Window* wd, VkSurfaceKHR surface, int width, int height);

    std::shared_ptr<ConfigManager> m_config;
    std::shared_ptr<Core::ThermalManager> m_thermalManager;
    std::unique_ptr<Core::UIAdapter> m_uiAdapter;
    
    HWND m_hwnd = NULL;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    VkAllocationCallbacks* m_allocator = nullptr;
};

} // namespace App
