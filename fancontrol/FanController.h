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

    bool SetFanLevel(int level, bool isDualFan = true);
    bool UpdateSmartControl(int maxTemp, const std::vector<SmartLevel>& levels);
    bool UpdatePIDControl(float currentTemp, const PIDSettings& settings, float dt);
    
    bool GetFanSpeeds(int& fan1, int& fan2);
    int GetCurrentFanCtrl() const { return m_currentFanCtrl; }
    void SetCurrentFanCtrl(int ctrl) { m_currentFanCtrl = ctrl; }
    void SetOnChangeCallback(std::function<void(int)> callback) { m_onChange = callback; }
    void SetWriteCallback(std::function<bool(int)> callback) { m_writeCallback = callback; }

private:
    std::shared_ptr<ECManager> m_ecManager;
    int m_currentFanCtrl;
    int m_lastSmartLevelIndex;
    
    // PID State
    float m_integral = 0.0f;
    float m_lastError = 0.0f;
    
    std::function<void(int)> m_onChange;
    std::function<bool(int)> m_writeCallback;
};
