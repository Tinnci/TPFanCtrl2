// Core/Events.h - Event definitions for the thermal management system
// Part of the Core library - NO Windows UI dependencies allowed here
#pragma once

#include <string>
#include <chrono>
#include <variant>
#include <vector>

namespace Core {

// Forward declarations
struct SensorReading;
struct FanState;

// --- Event Types ---

/// Event fired when sensor temperatures are updated
struct TemperatureUpdateEvent {
    std::chrono::steady_clock::time_point timestamp;
    std::vector<SensorReading> sensors;
    int maxTempIndex;           // Index of the hottest sensor
    int maxTemp;                // Maximum temperature value
    std::string maxSensorName;  // Name of the hottest sensor
};

/// Event fired when fan speed or level changes
struct FanStateChangeEvent {
    std::chrono::steady_clock::time_point timestamp;
    int fan1Speed;      // RPM
    int fan2Speed;      // RPM (0 if single fan)
    int currentLevel;   // 0-7, or 0x80 for BIOS control
    int previousLevel;
};

/// Event fired when the control mode changes
enum class ControlMode {
    BIOS = 1,       // Let BIOS control the fan
    Smart = 2,      // TPFanCtrl2 smart mode (temperature-based levels)
    Manual = 3,     // User-defined fixed speed
    PID = 4         // PID controller mode
};

struct ModeChangeEvent {
    std::chrono::steady_clock::time_point timestamp;
    ControlMode newMode;
    ControlMode previousMode;
    int smartProfileIndex;  // 0 or 1 for Smart Mode 1/2
};

/// Event fired when an error occurs
enum class ErrorSeverity {
    Warning,    // Non-fatal, operation can continue
    Error,      // Operation failed but system is stable
    Critical    // System should shut down or switch to safe mode
};

struct ErrorEvent {
    std::chrono::steady_clock::time_point timestamp;
    ErrorSeverity severity;
    std::string source;     // e.g., "ECManager", "SensorManager"
    std::string message;
    int errorCode;          // Optional platform-specific error code
};

/// Event fired for logging/tracing
enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error
};

struct LogEvent {
    std::chrono::steady_clock::time_point timestamp;
    LogLevel level;
    std::string message;
};

// --- Unified Event Type ---
using ThermalEvent = std::variant<
    TemperatureUpdateEvent,
    FanStateChangeEvent,
    ModeChangeEvent,
    ErrorEvent,
    LogEvent
>;

// --- Data Structures ---

/// A single sensor reading
struct SensorReading {
    int index;              // 0-11
    int address;            // EC address (e.g., 0x78)
    std::string name;       // User-defined name from config
    int rawTemp;            // Raw temperature from EC
    int biasedTemp;         // Temperature after offset adjustment
    float weight;           // Weight factor for max temp calculation
    bool isAvailable;       // Whether the sensor returned valid data
};

/// Current fan state
struct FanState {
    int fan1Speed;          // RPM
    int fan2Speed;          // RPM
    int currentLevel;       // 0-7 or 0x80
    bool isDualFan;         // Whether this is a dual-fan system
};

/// Complete thermal system state (immutable snapshot)
struct ThermalState {
    std::chrono::steady_clock::time_point timestamp;
    std::vector<SensorReading> sensors;
    FanState fanState;
    ControlMode currentMode;
    int smartProfileIndex;
    int maxTemp;
    int maxTempIndex;
    bool isOperational;     // False if EC communication failed
    std::string lastError;
};

} // namespace Core
