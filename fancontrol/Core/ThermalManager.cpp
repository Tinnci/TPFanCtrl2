// Core/ThermalManager.cpp - Implementation of the central thermal manager
#include "ThermalManager.h"
#include <format>
#include <algorithm>

namespace Core {

ThermalManager::ThermalManager(
    std::shared_ptr<ECManager> ecManager,
    const ThermalConfig& config
)
    : m_ecManager(std::move(ecManager))
    , m_config(config)
{
    // Create sensor manager with the EC manager
    m_sensorManager = std::make_unique<SensorManager>(m_ecManager);
    Log(LogLevel::Info, "Internal SensorManager created.");
    
    // Create fan controller
    m_fanController = std::make_unique<FanController>(m_ecManager);
    m_fanController->SetDualFanMode(m_config.isDualFan);
    m_fanController->SetFanSpeedAddr(m_config.fanSpeedAddr);
    
    // Apply initial sensor configuration
    for (const auto& sensor : m_config.sensors) {
        m_sensorManager->SetOffset(sensor.index, sensor.offset, sensor.hystMin, sensor.hystMax);
        m_sensorManager->SetSensorName(sensor.index, sensor.name);
        m_sensorManager->SetSensorWeight(sensor.index, sensor.weight);
    }
    
    // Initialize state
    m_state.currentMode = ControlMode::BIOS;
    m_state.isOperational = false;
    m_state.sensors.resize(SensorAddresses::TOTAL_COUNT);
    
    m_lastCycleTime = std::chrono::steady_clock::now();
}

ThermalManager::~ThermalManager() {
    Stop();
}

void ThermalManager::Start() {
    if (m_running.exchange(true)) {
        return; // Already running
    }
    
    Log(LogLevel::Info, "ThermalManager starting...");
    
    m_workerThread = std::jthread([this](std::stop_token token) {
        WorkerLoop(token);
    });
}

void ThermalManager::Stop() {
    if (!m_running.exchange(false)) {
        return; // Already stopped
    }
    
    Log(LogLevel::Info, "ThermalManager stopping...");
    
    // Request thread to stop
    m_workerThread.request_stop();
    
    // Wait for thread to finish
    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }
    
    // Return fan control to BIOS
    if (m_fanController) {
        m_fanController->SetFanLevel(0x80); // BIOS control
    }
    
    Log(LogLevel::Info, "ThermalManager stopped.");
}

ThermalState ThermalManager::GetState() const {
    std::lock_guard<std::mutex> lock(m_stateMutex);
    return m_state;
}

ControlMode ThermalManager::GetMode() const {
    return m_mode.load();
}

int ThermalManager::GetSmartProfileIndex() const {
    return m_smartProfile.load();
}

void ThermalManager::SetMode(ControlMode mode, int smartProfile) {
    ControlMode oldMode = m_mode.exchange(mode);
    m_smartProfile.store(smartProfile);
    
    if (oldMode != mode) {
        ModeChangeEvent event{
            .timestamp = std::chrono::steady_clock::now(),
            .newMode = mode,
            .previousMode = oldMode,
            .smartProfileIndex = smartProfile
        };
        m_dispatcher.Dispatch(event);
        
        Log(LogLevel::Info, std::format("Mode changed from {} to {}", 
            static_cast<int>(oldMode), static_cast<int>(mode)));
        
        // Force an immediate update when mode changes
        m_forceUpdate.store(true);
    }
}

void ThermalManager::SetManualLevel(int level) {
    m_manualLevel.store(std::clamp(level, 0, 7));
    if (m_mode.load() == ControlMode::Manual) {
        m_forceUpdate.store(true);
    }
}

void ThermalManager::UpdateConfig(const ThermalConfig& config) {
    {
        std::lock_guard<std::mutex> lock(m_configMutex);
        m_config = config;
    }
    
    // Reapply hardware settings
    if (m_fanController) {
        m_fanController->SetDualFanMode(config.isDualFan);
        m_fanController->SetFanSpeedAddr(config.fanSpeedAddr);
    }

    // Reapply sensor configuration
    for (const auto& sensor : config.sensors) {
        m_sensorManager->SetOffset(sensor.index, sensor.offset, sensor.hystMin, sensor.hystMax);
        m_sensorManager->SetSensorName(sensor.index, sensor.name);
        m_sensorManager->SetSensorWeight(sensor.index, sensor.weight);
    }
    
    Log(LogLevel::Info, "Configuration updated");
}

void ThermalManager::ForceUpdate() {
    m_forceUpdate.store(true);
}

EventDispatcher::SubscriptionId ThermalManager::Subscribe(EventDispatcher::Callback callback) {
    return m_dispatcher.Subscribe(std::move(callback));
}

EventDispatcher::SubscriptionId ThermalManager::Subscribe(std::weak_ptr<IThermalObserver> observer) {
    return m_dispatcher.Subscribe(std::move(observer));
}

void ThermalManager::Unsubscribe(EventDispatcher::SubscriptionId id) {
    m_dispatcher.Unsubscribe(id);
}

void ThermalManager::WorkerLoop(std::stop_token stopToken) {
    Log(LogLevel::Debug, "Worker thread started");
    
    int cycleMs;
    {
        std::lock_guard<std::mutex> lock(m_configMutex);
        cycleMs = m_config.cycleSeconds * 1000;
    }
    
    while (!stopToken.stop_requested()) {
        auto cycleStart = std::chrono::steady_clock::now();
        
        // Perform control cycle
        PerformCycle();
        
        // Wait for next cycle, but wake up early if force update is requested
        auto cycleEnd = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(cycleEnd - cycleStart);
        auto sleepTime = std::chrono::milliseconds(cycleMs) - elapsed;
        
        if (sleepTime > std::chrono::milliseconds(0)) {
            // Sleep in small intervals to be responsive to stop requests
            auto sleepEnd = cycleEnd + sleepTime;
            while (!stopToken.stop_requested() && 
                   !m_forceUpdate.load() &&
                   std::chrono::steady_clock::now() < sleepEnd) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
        
        m_forceUpdate.store(false);
        
        // Update config-based timing
        {
            std::lock_guard<std::mutex> lock(m_configMutex);
            cycleMs = m_config.cycleSeconds * 1000;
        }
    }
    
    Log(LogLevel::Debug, "Worker thread exiting");
}

void ThermalManager::PerformCycle() {
    auto now = std::chrono::steady_clock::now();
    float dt = std::chrono::duration<float>(now - m_lastCycleTime).count();
    m_lastCycleTime = now;
    
    // Update sensors
    if (!UpdateSensors()) {
        ReportError(ErrorSeverity::Error, "ThermalManager", 
                    "Critical: Failed to communicate with EC! Sensor readings stopped.", 0xFF01);
        return;
    }
    
    // Apply control based on mode
    ApplyControl();
}

bool ThermalManager::UpdateSensors() {
    bool useBiasedTemps, noExtSensor;
    {
        std::lock_guard<std::mutex> lock(m_configMutex);
        useBiasedTemps = m_config.useBiasedTemps;
        noExtSensor = m_config.noExtSensor;
    }
    
    // Legacy-style double sampling and retry logic to ensure reliable EC communication
    int numTries = 10;
    int sleepTicks = 200;
    
    bool success = false;
    int fan1 = 0, fan2 = 0;
    int maxTemp = 0, maxIndex = 0;
    std::vector<SensorReading> readings;
    int currentLevel = 0;

    for (int i = 0; i < numTries; i++) {
        // Sample 1
        if (!m_sensorManager->UpdateSensors(useBiasedTemps, noExtSensor, false) ||
            !m_fanController->RefreshCurrentLevel()) {
            Log(LogLevel::Warning, "Cycle sample1 failed: sensor or fan level read error");
            std::this_thread::sleep_for(std::chrono::milliseconds(sleepTicks));
            continue;
        }
        int level1 = m_fanController->GetCurrentLevel();

        // Sample 2
        if (!m_sensorManager->UpdateSensors(useBiasedTemps, noExtSensor, false) ||
            !m_fanController->RefreshCurrentLevel()) {
            Log(LogLevel::Warning, "Cycle sample2 failed: sensor or fan level read error");
            std::this_thread::sleep_for(std::chrono::milliseconds(sleepTicks));
            continue;
        }
        int level2 = m_fanController->GetCurrentLevel();

        // Match criteria (legacy only matched FanCtrl)
        if (level1 == level2) {
            // Success! Now get the rest of the data
            m_fanController->GetFanSpeeds(fan1, fan2);
            currentLevel = level2;
            
            std::string ignoreList;
            {
                std::lock_guard<std::mutex> lock(m_configMutex);
                ignoreList = m_config.ignoreList;
            }
            maxTemp = m_sensorManager->GetMaxTemp(maxIndex, ignoreList);
            
            readings.clear();
            readings.reserve(SensorAddresses::TOTAL_COUNT);
            for (int j = 0; j < SensorAddresses::TOTAL_COUNT; j++) {
                const auto& sensor = m_sensorManager->GetSensor(j);
                readings.push_back({
                    .index = j,
                    .address = sensor.addr,
                    .name = sensor.name,
                    .rawTemp = sensor.rawTemp,
                    .biasedTemp = sensor.biasedTemp,
                    .weight = sensor.weight,
                    .isAvailable = sensor.isAvailable
                });
            }
            success = true;
            break;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(sleepTicks));
    }

    if (!success) {
        Log(LogLevel::Error, "UpdateSensors failed after retries");
        return false;
    }
    
    // Update state
    int availableCount = 0;
    for (const auto& r : readings) if (r.isAvailable) availableCount++;

    FanState previousFanState;
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        previousFanState = m_state.fanState;
        m_state.timestamp = std::chrono::steady_clock::now();
        m_state.sensors = readings;
        m_state.maxTemp = maxTemp;
        m_state.maxTempIndex = maxIndex;
        m_state.fanState.fan1Speed = fan1;
        m_state.fanState.fan2Speed = fan2;
        m_state.fanState.currentLevel = currentLevel;
        m_state.fanState.isDualFan = m_fanController->IsDualFanActive();
        m_state.currentMode = m_mode.load();
        m_state.smartProfileIndex = m_smartProfile.load();
        m_state.isOperational = true;
    }

    FanStateChangeEvent fanEvent{
        .timestamp = std::chrono::steady_clock::now(),
        .fan1Speed = fan1,
        .fan2Speed = fan2,
        .currentLevel = currentLevel,
        .previousLevel = previousFanState.currentLevel
    };
    m_dispatcher.Dispatch(fanEvent);

    if (!m_state.isOperational || availableCount == 0) {
        static int warnCounter = 0;
        if (warnCounter++ % 10 == 0) { // Throttled logging
             Log(LogLevel::Warning, "No sensors detected as available.");
        }
    }
    
    // Dispatch temperature update event
    TemperatureUpdateEvent event{
        .timestamp = std::chrono::steady_clock::now(),
        .sensors = readings,
        .maxTempIndex = maxIndex,
        .maxTemp = maxTemp,
        .maxSensorName = readings[maxIndex].name
    };
    m_dispatcher.Dispatch(event);
    
    return true;
}

void ThermalManager::ApplyControl() {
    ControlMode mode = m_mode.load();
    
    switch (mode) {
        case ControlMode::BIOS:
            ApplyBIOSMode();
            break;
        case ControlMode::Smart:
            ApplySmartMode();
            break;
        case ControlMode::Manual:
            ApplyManualMode();
            break;
        case ControlMode::PID:
            {
                auto now = std::chrono::steady_clock::now();
                float dt = std::chrono::duration<float>(now - m_lastCycleTime).count();
                ApplyPIDMode(dt);
            }
            break;
    }
}

void ThermalManager::ApplyBIOSMode() {
    // Set fan to BIOS control (0x80)
    if (m_fanController->GetCurrentLevel() != 0x80) {
        m_fanController->SetFanLevel(0x80);
    }
}

void ThermalManager::ApplySmartMode() {
    int maxTemp;
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        maxTemp = m_state.maxTemp;
    }
    
    std::vector<SmartLevel> levels;
    {
        std::lock_guard<std::mutex> lock(m_configMutex);
        int profileIndex = m_smartProfile.load();
        const auto& profile = m_config.smartProfiles[profileIndex];
        
        for (const auto& def : profile) {
            if (def.IsValid()) {
                levels.push_back({def.temperature, def.fanLevel, def.hystUp, def.hystDown});
            }
        }
    }
    
    m_fanController->UpdateSmartControl(maxTemp, levels);
}

void ThermalManager::ApplyManualMode() {
    int level = m_manualLevel.load();
    int manModeExitTemp;
    {
        std::lock_guard<std::mutex> lock(m_configMutex);
        manModeExitTemp = m_config.manModeExitTemp;
    }
    
    int maxTemp;
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        maxTemp = m_state.maxTemp;
    }
    
    // Check if we should auto-exit manual mode
    if (maxTemp > manModeExitTemp) {
        Log(LogLevel::Warning, std::format(
            "Temperature {}°C exceeds manual mode exit threshold {}°C, switching to Smart mode",
            maxTemp, manModeExitTemp));
        SetMode(ControlMode::Smart);
        return;
    }
    
    m_fanController->SetFanLevel(level);
}

void ThermalManager::ApplyPIDMode(float dt) {
    int maxTemp;
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        maxTemp = m_state.maxTemp;
    }
    
    PIDConfig pid;
    {
        std::lock_guard<std::mutex> lock(m_configMutex);
        pid = m_config.pid;
    }
    
    PIDSettings settings{
        .targetTemp = pid.targetTemp,
        .Kp = pid.Kp,
        .Ki = pid.Ki,
        .Kd = pid.Kd,
        .minFan = static_cast<float>(pid.minFan),
        .maxFan = static_cast<float>(pid.maxFan)
    };
    
    m_fanController->UpdatePIDControl(static_cast<float>(maxTemp), settings, dt);
}

void ThermalManager::Log(LogLevel level, const std::string& message) {
    LogEvent event{
        .timestamp = std::chrono::steady_clock::now(),
        .level = level,
        .message = message
    };
    m_dispatcher.Dispatch(event);
}

void ThermalManager::ReportError(ErrorSeverity severity, const std::string& source,
                                  const std::string& message, int code) {
    ErrorEvent event{
        .timestamp = std::chrono::steady_clock::now(),
        .severity = severity,
        .source = source,
        .message = message,
        .errorCode = code
    };
    m_dispatcher.Dispatch(event);
}

} // namespace Core
