#pragma once

#include <memory>
#include <vector>
#include <functional>
#include "ECManager.h"
#include "CommonTypes.h"

enum class ControlAlgorithm {
    Step,
    PID
};

struct PIDSettings {
    float targetTemp = 60.0f;
    float Kp = 0.5f;
    float Ki = 0.01f;
    float Kd = 0.1f;
    float minFan = 0.0f;
    float maxFan = 7.0f;
};

class FanController {
public:
    FanController(std::shared_ptr<ECManager> ecManager);

    bool SetFanLevel(int level, bool isDualFan);
    bool SetFanLevel(int level); // Uses internal m_isDualFan
    bool SetFanLevels(int level1, int level2);
    bool UpdateSmartControl(int maxTemp, const std::vector<SmartLevel>& levels);
    bool UpdatePIDControl(float currentTemp, const PIDSettings& settings, float dt);
    
    bool GetFanSpeeds(int& fan1, int& fan2);
    bool RefreshCurrentLevel();
    int GetCurrentFanCtrl() const { return m_currentFanCtrl; }
    int GetCurrentLevel() const { return m_currentFanCtrl; }  // Alias for Core compatibility
    void SetCurrentFanCtrl(int ctrl) { m_currentFanCtrl = ctrl; }
    void SetOnChangeCallback(std::function<void(int)> callback) { m_onChange = callback; }
    void SetWriteCallback(std::function<bool(int)> callback) { m_writeCallback = callback; }
    void SetDualFanMode(bool isDual) {
        m_isDualFan = isDual;
        m_dualFanOperational = isDual;
        m_fan2ZeroCount = 0;
    }
    void SetFanSpeedAddr(int addr) { m_fanSpeedAddr = addr; }
    bool IsDualFanActive() const { return m_isDualFan && m_dualFanOperational; }

private:
    std::shared_ptr<ECManager> m_ecManager;
    int m_currentFanCtrl;
    int m_lastSmartLevelIndex;
    bool m_isDualFan = false;
    bool m_dualFanOperational = false;
    int m_fanSpeedAddr = TP_ECOFFSET_FANSPEED;
    int m_fan2ZeroCount = 0;
    static constexpr int kFan2DisableThreshold = 5;
    static constexpr int kFan1ActiveRpmThreshold = 400;
    
    // PID State
    float m_integral = 0.0f;
    float m_lastError = 0.0f;
    
    std::function<void(int)> m_onChange;
    std::function<bool(int)> m_writeCallback;
};
