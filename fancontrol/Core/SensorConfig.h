// Core/SensorConfig.h - Sensor configuration structures
// Part of the Core library - NO Windows UI dependencies allowed here
#pragma once

#include <string>
#include <vector>
#include <array>

namespace Core {

/// Configuration for a single temperature sensor
struct SensorDefinition {
    int index;                  // 0-11
    int address;                // EC register address
    std::string name;           // Display name (e.g., "CPU", "GPU")
    int offset;                 // Temperature offset/bias
    int hystMin;                // Hysteresis minimum threshold
    int hystMax;                // Hysteresis maximum threshold
    float weight;               // Weight factor (1.0 = normal)
    bool enabled;               // Whether to include in max temp calculation
    
    // Defaults
    SensorDefinition()
        : index(0), address(0), name(""), offset(0),
          hystMin(-1), hystMax(-1), weight(1.0f), enabled(true) {}
          
    SensorDefinition(int idx, int addr, const std::string& n = "")
        : index(idx), address(addr), name(n), offset(0),
          hystMin(-1), hystMax(-1), weight(1.0f), enabled(true) {}
};

/// Smart mode fan level configuration
struct SmartLevelDefinition {
    int temperature;            // Threshold temperature
    int fanLevel;               // Fan level (0-7, or 64 for max, 128 for BIOS)
    int hystUp;                 // Hysteresis when heating up
    int hystDown;               // Hysteresis when cooling down
    
    SmartLevelDefinition()
        : temperature(-1), fanLevel(0), hystUp(0), hystDown(0) {}
        
    SmartLevelDefinition(int temp, int fan, int up = 0, int down = 0)
        : temperature(temp), fanLevel(fan), hystUp(up), hystDown(down) {}
        
    bool IsValid() const { return temperature >= 0; }
};

/// PID controller settings
struct PIDConfig {
    float Kp;                   // Proportional gain
    float Ki;                   // Integral gain
    float Kd;                   // Derivative gain
    float targetTemp;           // Target temperature
    int minFan;                 // Minimum fan level
    int maxFan;                 // Maximum fan level
    
    PIDConfig()
        : Kp(1.0f), Ki(0.1f), Kd(0.5f), targetTemp(60.0f), minFan(0), maxFan(7) {}
};

/// Icon color level thresholds
struct IconLevelConfig {
    std::vector<int> thresholds; // Temperature thresholds for each color level
    
    IconLevelConfig() {
        // Default thresholds: Blue < 50, Green < 60, Yellow < 70, Red >= 70
        thresholds = {50, 60, 70};
    }
};

/// Complete thermal configuration
struct ThermalConfig {
    // Sensor configuration
    std::vector<SensorDefinition> sensors;
    std::string ignoreList;     // Comma-separated sensor names to ignore
    
    // Smart mode configuration (two profiles)
    std::array<std::vector<SmartLevelDefinition>, 2> smartProfiles;
    
    // PID configuration
    PIDConfig pid;
    
    // Hardware settings
    bool isDualFan;
    bool useBiasedTemps;
    bool noExtSensor;           // Don't read extended sensors (0xC0-0xC3)
    
    // Timing
    int cycleSeconds;           // Main control loop interval
    int iconCycleSeconds;       // Icon update interval
    
    // Display
    bool useFahrenheit;
    IconLevelConfig iconLevels;
    
    // Behavior
    int manualFanSpeed;         // Default manual mode fan level
    int manModeExitTemp;        // Auto-exit manual mode above this temp
    
    ThermalConfig() 
        : isDualFan(false), useBiasedTemps(true), noExtSensor(false),
          cycleSeconds(5), iconCycleSeconds(3), useFahrenheit(false),
          manualFanSpeed(7), manModeExitTemp(90) {}
};

/// Standard sensor addresses for ThinkPad EC
namespace SensorAddresses {
    // Primary sensors (0x78-0x7F)
    constexpr int PRIMARY_BASE = 0x78;
    constexpr int PRIMARY_COUNT = 8;
    
    // Extended sensors (0xC0-0xC3)
    constexpr int EXTENDED_BASE = 0xC0;
    constexpr int EXTENDED_COUNT = 4;
    
    // Total sensor count
    constexpr int TOTAL_COUNT = PRIMARY_COUNT + EXTENDED_COUNT;
    
    /// Get address for a sensor by index
    constexpr int GetAddress(int index) {
        if (index < PRIMARY_COUNT) {
            return PRIMARY_BASE + index;
        } else if (index < TOTAL_COUNT) {
            return EXTENDED_BASE + (index - PRIMARY_COUNT);
        }
        return 0;
    }
}

/// Create default sensor definitions for a standard ThinkPad
inline std::vector<SensorDefinition> CreateDefaultSensorConfig() {
    std::vector<SensorDefinition> sensors;
    sensors.reserve(SensorAddresses::TOTAL_COUNT);
    
    // Primary sensors
    for (int i = 0; i < SensorAddresses::PRIMARY_COUNT; i++) {
        sensors.emplace_back(i, SensorAddresses::GetAddress(i));
    }
    
    // Extended sensors
    for (int i = 0; i < SensorAddresses::EXTENDED_COUNT; i++) {
        int idx = SensorAddresses::PRIMARY_COUNT + i;
        sensors.emplace_back(idx, SensorAddresses::GetAddress(idx));
    }
    
    return sensors;
}

} // namespace Core
