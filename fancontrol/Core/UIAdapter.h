// Core/UIAdapter.h - Adapter between ThermalManager and ImGui UI state
// This provides a bridge for gradual migration from the legacy HardwareWorker
#pragma once

#include "ThermalManager.h"
#include "Events.h"
#include "../CommonTypes.h"
#include <mutex>
#include <deque>
#include <map>
#include <vector>
#include <string>
#include <functional>

namespace Core {

/// Smooth animation helper for temperature values
struct SmoothValue {
    float Current = 0.0f;
    float Target = 0.0f;
    void Update(float dt, float speed = 5.0f) {
        Current += (Target - Current) * (1.0f - expf(-speed * dt));
    }
};

/// Autotune state (kept for backwards compatibility with existing UI)
enum class AutotuneStep { Idle, WaitingForHeat, Oscillating, Success, Failed };

struct AutotuneContext {
    AutotuneStep Stage = AutotuneStep::Idle;
    int CyclesCount = 0;
    float Peaks[10] = {};
    float Troughs[10] = {};
    float PeakTimes[10] = {};
    float TroughTimes[10] = {};
    float LastTemp = 0.0f;
    bool Rising = true;
    float CurrentMax = -1.0f;
    float CurrentMin = 200.0f;
    std::string Status;
};

/// UI-friendly state snapshot
/// This replaces the legacy g_UIState and provides a clean interface for ImGui
struct UISnapshot {
    // Sensor data
    std::vector<SensorData> Sensors;
    std::map<std::string, SmoothValue> SmoothTemps;
    std::map<std::string, std::deque<float>> TempHistory;
    
    // Fan data
    int Fan1Speed = 0;
    int Fan2Speed = 0;
    int CurrentFanLevel = 0;
    
    // Control state
    int Mode = 2;  // 0: BIOS, 1: Manual, 2: Smart
    int ManualLevel = 0;
    int SmartProfile = 0;
    
    // Algorithm selection (for UI display)
    int Algorithm = 0;  // 0: Step, 1: PID
    PIDSettings PID;
    
    // Max temp info
    int MaxTemp = 0;
    int MaxTempIndex = 0;
    std::string MaxSensorName;
    
    // Status
    time_t LastUpdate = 0;
    bool IsOperational = false;
    std::string LastError;
    
    // UI-specific
    int SelectedSettingsTab = 0;
    std::vector<float> SensorWeights;
    std::vector<std::string> SensorNames;
    AutotuneContext Autotune;
};

/// Adapter class that bridges ThermalManager to the ImGui UI
/// 
/// Usage:
/// 1. Create UIAdapter with a ThermalManager instance
/// 2. Call Update() each frame to process events and update smooth animations
/// 3. Use GetSnapshot() to get the current state for rendering
/// 4. Use SetMode(), SetManualLevel(), etc. to control the ThermalManager
class UIAdapter {
public:
    using TrayUpdateCallback = std::function<void(int temp, int fanSpeed)>;
    
    explicit UIAdapter(std::shared_ptr<ThermalManager> manager);
    ~UIAdapter();
    
    // Non-copyable
    UIAdapter(const UIAdapter&) = delete;
    UIAdapter& operator=(const UIAdapter&) = delete;
    
    // --- Frame Update ---
    
    /// Call this once per frame to update animations and process events
    void Update(float deltaTime);
    
    // --- State Access ---
    
    /// Get a thread-safe copy of the current UI state
    UISnapshot GetSnapshot() const;
    
    /// Get max temperature (convenience method)
    int GetMaxTemp() const;
    
    /// Get fan1 speed (convenience method)
    int GetFan1Speed() const;
    
    // --- Control ---
    
    /// Set control mode (0=BIOS, 1=Manual, 2=Smart)
    void SetMode(int mode);
    
    /// Set manual fan level (0-7)
    void SetManualLevel(int level);
    
    /// Set smart profile index (0 or 1)
    void SetSmartProfile(int profile);
    
    /// Set PID parameters
    void SetPIDSettings(const PIDSettings& settings);
    
    /// Set algorithm type (0=Step, 1=PID)
    void SetAlgorithm(int algorithm);
    
    /// Start autotune process
    void StartAutotune();
    
    /// Cancel autotune process
    void CancelAutotune();
    
    // --- Callbacks ---
    
    /// Set callback for tray icon updates
    void SetTrayUpdateCallback(TrayUpdateCallback callback);
    
    // --- History ---
    
    /// Get temperature history for a sensor (for plotting)
    std::deque<float> GetTempHistory(const std::string& sensorName) const;
    
    /// Get all temperature history (for plotting)
    std::map<std::string, std::deque<float>> GetAllTempHistory() const;
    
private:
    void OnThermalEvent(const ThermalEvent& event);
    void HandleTemperatureUpdate(const TemperatureUpdateEvent& e);
    void HandleFanStateChange(const FanStateChangeEvent& e);
    void HandleModeChange(const ModeChangeEvent& e);
    void HandleError(const ErrorEvent& e);
    void HandleLog(const LogEvent& e);
    void UpdateAutotuneLogic(float currentTemp);
    
    std::shared_ptr<ThermalManager> m_manager;
    EventDispatcher::SubscriptionId m_subscriptionId;
    
    mutable std::mutex m_mutex;
    UISnapshot m_state;
    
    TrayUpdateCallback m_trayCallback;
    int m_trayUpdateCounter = 0;
    
    static constexpr int HISTORY_MAX_SIZE = 300;
};

} // namespace Core
