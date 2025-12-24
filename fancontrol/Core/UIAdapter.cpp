// Core/UIAdapter.cpp - Implementation of the UI adapter
#include "UIAdapter.h"
#include <algorithm>
#include <chrono>
#include "../LogManager.h"

namespace Core {

UIAdapter::UIAdapter(std::shared_ptr<ThermalManager> manager)
    : m_manager(std::move(manager))
{
    // Initialize default state
    m_state.Sensors.resize(SensorAddresses::TOTAL_COUNT);
    m_state.SensorWeights.resize(16, 1.0f);
    m_state.SensorNames.resize(16);
    
    // Subscribe to thermal events
    m_subscriptionId = m_manager->Subscribe([this](const ThermalEvent& e) {
        OnThermalEvent(e);
    });
}

UIAdapter::~UIAdapter() {
    m_manager->Unsubscribe(m_subscriptionId);
}

void UIAdapter::Update(float deltaTime) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Update smooth animations
    for (auto& [name, smooth] : m_state.SmoothTemps) {
        smooth.Update(deltaTime);
    }
    
    // Update tray icon periodically
    if (m_trayCallback) {
        m_trayUpdateCounter++;
        if (m_trayUpdateCounter >= 10) {  // Every ~1 second at 60fps
            m_trayCallback(m_state.MaxTemp, m_state.Fan1Speed);
            m_trayUpdateCounter = 0;
        }
    }
}

UISnapshot UIAdapter::GetSnapshot() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_state;
}

int UIAdapter::GetMaxTemp() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_state.MaxTemp;
}

int UIAdapter::GetFan1Speed() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_state.Fan1Speed;
}

void UIAdapter::SetMode(int mode) {
    ControlMode coreMode;
    switch (mode) {
        case 0: coreMode = ControlMode::BIOS; break;
        case 1: coreMode = ControlMode::Manual; break;
        case 2: 
        default:
            coreMode = ControlMode::Smart; 
            break;
    }
    
    int profile;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        profile = m_state.SmartProfile;
        m_state.Mode = mode;
    }
    
    m_manager->SetMode(coreMode, profile);
}

void UIAdapter::SetManualLevel(int level) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_state.ManualLevel = level;
    }
    m_manager->SetManualLevel(level);
}

void UIAdapter::SetSmartProfile(int profile) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_state.SmartProfile = profile;
    }
    
    if (m_state.Mode == 2) {  // Smart mode
        m_manager->SetMode(ControlMode::Smart, profile);
    }
}

void UIAdapter::SetPIDSettings(const PIDSettings& settings) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_state.PID = settings;
    // TODO: Update ThermalManager config when PID config is exposed
}

void UIAdapter::SetAlgorithm(int algorithm) {
    int profile;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_state.Algorithm = algorithm;
        profile = m_state.SmartProfile;
    }
    
    if (algorithm == 1) {  // PID
        // ThermalManager would switch to PID mode
        m_manager->SetMode(ControlMode::PID, 0);
    } else {
        m_manager->SetMode(ControlMode::Smart, profile);
    }
}

void UIAdapter::StartAutotune() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_state.Autotune.Stage = AutotuneStep::WaitingForHeat;
    m_state.Autotune.CyclesCount = 0;
    m_state.Autotune.CurrentMax = -1.0f;
    m_state.Autotune.CurrentMin = 200.0f;
    m_state.Autotune.Rising = true;
    m_state.Autotune.Status = "Starting autotune...";
}

void UIAdapter::CancelAutotune() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_state.Autotune.Stage = AutotuneStep::Idle;
    m_state.Autotune.Status = "Autotune cancelled";
}

void UIAdapter::SetTrayUpdateCallback(TrayUpdateCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_trayCallback = std::move(callback);
}

std::deque<float> UIAdapter::GetTempHistory(const std::string& sensorName) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_state.TempHistory.find(sensorName);
    if (it != m_state.TempHistory.end()) {
        return it->second;
    }
    return {};
}

std::map<std::string, std::deque<float>> UIAdapter::GetAllTempHistory() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_state.TempHistory;
}

void UIAdapter::OnThermalEvent(const ThermalEvent& event) {
    std::visit([this](const auto& e) {
        using T = std::decay_t<decltype(e)>;
        if constexpr (std::is_same_v<T, TemperatureUpdateEvent>) {
            HandleTemperatureUpdate(e);
        } else if constexpr (std::is_same_v<T, FanStateChangeEvent>) {
            HandleFanStateChange(e);
        } else if constexpr (std::is_same_v<T, ModeChangeEvent>) {
            HandleModeChange(e);
        } else if constexpr (std::is_same_v<T, ErrorEvent>) {
            HandleError(e);
        } else if constexpr (std::is_same_v<T, LogEvent>) {
            HandleLog(e);
        }
    }, event);
}

void UIAdapter::HandleTemperatureUpdate(const TemperatureUpdateEvent& e) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Update sensor data
    for (const auto& reading : e.sensors) {
        if (reading.index >= 0 && reading.index < (int)m_state.Sensors.size()) {
            auto& sensor = m_state.Sensors[reading.index];
            sensor.name = reading.name;
            sensor.addr = reading.address;
            sensor.rawTemp = reading.rawTemp;
            sensor.biasedTemp = reading.biasedTemp;
            sensor.weight = reading.weight;
            sensor.isAvailable = reading.isAvailable;
            
            // Update smooth animation target
            if (reading.isAvailable && reading.rawTemp > 0 && reading.rawTemp < 128) {
                m_state.SmoothTemps[reading.name].Target = (float)reading.rawTemp;
            }
            
            // Update history
            if (reading.isAvailable) {
                auto& history = m_state.TempHistory[reading.name];
                float valToPush = (float)reading.rawTemp;
                
                // Use last valid value if current is invalid
                if ((reading.rawTemp <= 0 || reading.rawTemp >= 128) && !history.empty()) {
                    valToPush = history.back();
                }
                
                history.push_back(valToPush);
                while (history.size() > HISTORY_MAX_SIZE) {
                    history.pop_front();
                }
            }
        }
    }
    
    // Update max temp info
    m_state.MaxTemp = e.maxTemp;
    m_state.MaxTempIndex = e.maxTempIndex;
    m_state.MaxSensorName = e.maxSensorName;
    m_state.LastUpdate = time(nullptr);
    m_state.IsOperational = true;
    
    // Run autotune logic if active
    if (m_state.Autotune.Stage != AutotuneStep::Idle &&
        m_state.Autotune.Stage != AutotuneStep::Success &&
        m_state.Autotune.Stage != AutotuneStep::Failed) {
        UpdateAutotuneLogic((float)e.maxTemp);
    }
}

void UIAdapter::HandleFanStateChange(const FanStateChangeEvent& e) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_state.Fan1Speed = e.fan1Speed;
    m_state.Fan2Speed = e.fan2Speed;
    m_state.CurrentFanLevel = e.currentLevel;
}

void UIAdapter::HandleModeChange(const ModeChangeEvent& e) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    switch (e.newMode) {
        case ControlMode::BIOS:   m_state.Mode = 0; break;
        case ControlMode::Manual: m_state.Mode = 1; break;
        case ControlMode::Smart:  m_state.Mode = 2; break;
        case ControlMode::PID:    m_state.Mode = 2; m_state.Algorithm = 1; break;
    }
    
    m_state.SmartProfile = e.smartProfileIndex;
}

void UIAdapter::HandleError(const ErrorEvent& e) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_state.LastError = e.message;
    
    Log::Level level = (e.severity == ErrorSeverity::Critical) ? Log::Level::Error : Log::Level::Warn;
    Log::UILogBuffer::Get().Add(level, "[{}] {}", e.source, e.message);

    if (e.severity == ErrorSeverity::Critical) {
        m_state.IsOperational = false;
    }
}

void UIAdapter::HandleLog(const LogEvent& e) {
    Log::Level level;
    switch (e.level) {
        case LogLevel::Debug:   level = Log::Level::Debug; break;
        case LogLevel::Info:    level = Log::Level::Info; break;
        case LogLevel::Warning: level = Log::Level::Warn; break;
        case LogLevel::Error:   level = Log::Level::Error; break;
        default:                level = Log::Level::Info; break;
    }
    Log::UILogBuffer::Get().Add(level, "{}", e.message);
}

void UIAdapter::UpdateAutotuneLogic(float currentTemp) {
    // This is kept for backwards compatibility with the existing autotune UI
    float target = m_state.PID.targetTemp;
    
    if (m_state.Autotune.LastTemp > 0) {
        if (m_state.Autotune.Rising && currentTemp < m_state.Autotune.LastTemp - 0.1f) {
            // Peak found
            if (m_state.Autotune.CyclesCount < 10) {
                m_state.Autotune.Peaks[m_state.Autotune.CyclesCount] = m_state.Autotune.CurrentMax;
                auto now = std::chrono::steady_clock::now();
                m_state.Autotune.PeakTimes[m_state.Autotune.CyclesCount] = 
                    (float)std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() / 1000.0f;
            }
            m_state.Autotune.Rising = false;
            m_state.Autotune.CurrentMin = currentTemp;
        } else if (!m_state.Autotune.Rising && currentTemp > m_state.Autotune.LastTemp + 0.1f) {
            // Trough found
            if (m_state.Autotune.CyclesCount < 10) {
                m_state.Autotune.Troughs[m_state.Autotune.CyclesCount] = m_state.Autotune.CurrentMin;
                auto now = std::chrono::steady_clock::now();
                m_state.Autotune.TroughTimes[m_state.Autotune.CyclesCount] = 
                    (float)std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() / 1000.0f;
                m_state.Autotune.CyclesCount++;
            }
            m_state.Autotune.Rising = true;
            m_state.Autotune.CurrentMax = currentTemp;
            
            if (m_state.Autotune.CyclesCount >= 4) {
                // Calculate Ziegler-Nichols parameters
                float sumA = 0, sumP = 0;
                for (int i = 1; i < 4; i++) {
                    sumA += (m_state.Autotune.Peaks[i] - m_state.Autotune.Troughs[i]);
                    sumP += (m_state.Autotune.PeakTimes[i] - m_state.Autotune.PeakTimes[i-1]);
                }
                float A = sumA / 3.0f;
                float P = sumP / 3.0f;
                float Ku = (4.0f * 100.0f) / (3.14159f * A);
                
                // No-overshoot parameters (conservative)
                m_state.PID.Kp = 0.2f * Ku;
                float Ti = 0.5f * P;
                float Td = 0.33f * P;
                m_state.PID.Ki = m_state.PID.Kp / Ti;
                m_state.PID.Kd = m_state.PID.Kp * Td;
                
                m_state.Autotune.Stage = AutotuneStep::Success;
                m_state.Autotune.Status = "Autotune complete!";
            }
        }
    }
    
    if (currentTemp > m_state.Autotune.CurrentMax) m_state.Autotune.CurrentMax = currentTemp;
    if (currentTemp < m_state.Autotune.CurrentMin) m_state.Autotune.CurrentMin = currentTemp;
    m_state.Autotune.LastTemp = currentTemp;
}

} // namespace Core
