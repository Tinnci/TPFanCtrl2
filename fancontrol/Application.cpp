#include "Application.h"
#include "TVicPortProvider.h"
#include "TVicPort.h"
#include <spdlog/spdlog.h>

namespace App {

Application::Application() {
    m_config = std::make_shared<ConfigManager>();
}

Application::~Application() {
    Shutdown();
}

bool Application::Initialize(HWND hwnd) {
    m_hwnd = hwnd;
    
    // Load config
    if (!m_config->LoadConfig("TPFanCtrl2.ini")) {
        spdlog::warn("Failed to load TPFanCtrl2.ini, using defaults.");
    }

    // Initialize hardware access
    if (!InitTVicPort()) {
        spdlog::error("Failed to initialize TVicPort driver.");
        return false;
    }

    auto ioProvider = std::make_shared<TVicPortProvider>();
    auto ecManager = std::make_shared<ECManager>(ioProvider, [](const char* msg) {
        spdlog::debug("[EC] {}", msg);
    });

    // Initialize Core components
    // Note: BuildThermalConfig is currently in imgui_main.cpp, 
    // we might want to move it to a utility or into Application.
    // For now, we'll assume it's available or implement a simplified version.
    // Core::ThermalConfig thermalConfig = BuildThermalConfig(m_config);
    // m_thermalManager = std::make_shared<Core::ThermalManager>(ecManager, thermalConfig);
    // m_uiAdapter = std::make_unique<Core::UIAdapter>(m_thermalManager);

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

void Application::Render(ImGui_ImplVulkanH_Window* wd) {
    // Rendering logic moved from main loop
}

bool Application::InitVulkan(HWND hwnd) {
    // Vulkan initialization logic from imgui_main.cpp
    return true;
}

void Application::CleanupVulkan() {
    // Vulkan cleanup logic from imgui_main.cpp
}

} // namespace App
