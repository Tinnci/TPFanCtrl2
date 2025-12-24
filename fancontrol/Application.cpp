#include "Application.h"
#include "TVicPortProvider.h"
#include "TVicPort.h"
#include <spdlog/spdlog.h>

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

bool Application::Initialize(HWND hwnd) {
    m_hwnd = hwnd;
    
    // Load config
    if (!m_config->LoadConfig("TPFanCtrl2.json")) {
        spdlog::warn("Failed to load TPFanCtrl2.json, using defaults.");
    }

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
