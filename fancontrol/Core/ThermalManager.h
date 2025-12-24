// Core/ThermalManager.h - Central thermal management orchestrator
// Part of the Core library - NO Windows UI dependencies allowed here
#pragma once

#include "Events.h"
#include "IThermalObserver.h"
#include "SensorConfig.h"
#include "../ECManager.h"
#include "../SensorManager.h"
#include "../FanController.h"

#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <chrono>

namespace Core {

/// Central orchestrator for thermal management
/// This class coordinates sensor reading, fan control, and mode management.
/// It runs its own background thread and notifies observers of state changes.
/// 
/// IMPORTANT: This class has NO Windows UI dependencies. It does not know about
/// HWND, HINSTANCE, WM_TIMER, or any Win32 primitives.
class ThermalManager {
public:
    /// Constructor
    /// @param ecManager Shared pointer to the EC access manager
    /// @param config Initial thermal configuration
    explicit ThermalManager(
        std::shared_ptr<ECManager> ecManager,
        const ThermalConfig& config = ThermalConfig()
    );
    
    /// Destructor - automatically stops the worker thread
    ~ThermalManager();
    
    // Non-copyable, non-movable
    ThermalManager(const ThermalManager&) = delete;
    ThermalManager& operator=(const ThermalManager&) = delete;
    ThermalManager(ThermalManager&&) = delete;
    ThermalManager& operator=(ThermalManager&&) = delete;
    
    // --- Lifecycle ---
    
    /// Start the background control thread
    void Start();
    
    /// Stop the background control thread
    /// Blocks until the thread has terminated
    void Stop();
    
    /// Check if the manager is running
    bool IsRunning() const { return m_running.load(); }
    
    // --- State Access (Thread-Safe) ---
    
    /// Get a snapshot of the current thermal state
    /// This is safe to call from any thread
    ThermalState GetState() const;
    
    /// Get current control mode
    ControlMode GetMode() const;
    
    /// Get current smart profile index (0 or 1)
    int GetSmartProfileIndex() const;
    
    // --- Control (Thread-Safe) ---
    
    /// Set the control mode
    /// @param mode The new control mode
    /// @param smartProfile For Smart mode, which profile to use (0 or 1)
    void SetMode(ControlMode mode, int smartProfile = 0);
    
    /// Set manual fan level (only effective in Manual mode)
    /// @param level Fan level 0-7
    void SetManualLevel(int level);
    
    /// Update configuration
    /// @param config New configuration to apply
    void UpdateConfig(const ThermalConfig& config);
    
    /// Force an immediate sensor update
    void ForceUpdate();
    
    // --- Event Subscription ---
    
    /// Subscribe to thermal events
    /// @param callback Function to call when events occur
    /// @return Subscription ID for later unsubscription
    EventDispatcher::SubscriptionId Subscribe(EventDispatcher::Callback callback);
    
    /// Subscribe an observer object
    EventDispatcher::SubscriptionId Subscribe(std::weak_ptr<IThermalObserver> observer);
    
    /// Unsubscribe from events
    void Unsubscribe(EventDispatcher::SubscriptionId id);
    
private:
    // --- Internal Methods ---
    
    /// Main worker thread function
    void WorkerLoop(std::stop_token stopToken);
    
    /// Perform one control cycle
    void PerformCycle();
    
    /// Update all sensor readings
    bool UpdateSensors();
    
    /// Apply control logic based on current mode
    void ApplyControl();
    
    /// Apply BIOS mode (release control)
    void ApplyBIOSMode();
    
    /// Apply Smart mode control
    void ApplySmartMode();
    
    /// Apply Manual mode control
    void ApplyManualMode();
    
    /// Apply PID control
    void ApplyPIDMode(float dt);
    
    /// Log a message through the event system
    void Log(LogLevel level, const std::string& message);
    
    /// Report an error through the event system
    void ReportError(ErrorSeverity severity, const std::string& source, 
                     const std::string& message, int code = 0);
    
    // --- State ---
    
    // Core components
    std::shared_ptr<ECManager> m_ecManager;
    std::unique_ptr<SensorManager> m_sensorManager;
    std::unique_ptr<FanController> m_fanController;
    
    // Configuration (protected by m_configMutex)
    ThermalConfig m_config;
    mutable std::mutex m_configMutex;
    
    // Current state (protected by m_stateMutex)
    ThermalState m_state;
    mutable std::mutex m_stateMutex;
    
    // Control state
    std::atomic<ControlMode> m_mode{ControlMode::BIOS};
    std::atomic<int> m_smartProfile{0};
    std::atomic<int> m_manualLevel{7};
    
    // Worker thread
    std::jthread m_workerThread;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_forceUpdate{false};
    
    // Event dispatcher
    EventDispatcher m_dispatcher;
    
    // PID state
    float m_pidIntegral{0.0f};
    float m_pidLastError{0.0f};
    std::chrono::steady_clock::time_point m_lastCycleTime;
};

} // namespace Core
