#include "_prec.h"
#include "FanController.h"

FanController::FanController(std::shared_ptr<ECManager> ecManager)
    : m_ecManager(ecManager), m_currentFanCtrl(-1), m_lastSmartLevelIndex(-1) {}

bool FanController::SetFanLevel(int level, bool isDualFan) {
    std::lock_guard<std::recursive_timed_mutex> lock(m_ecManager->GetMutex());
    bool ok = false;
    if (m_writeCallback) {
        ok = m_writeCallback(level);
    } else {
        for (int i = 0; i < 5; i++) {
            if (isDualFan) {
                // Set Fan 1
                m_ecManager->WriteByte(TP_ECOFFSET_FAN_SWITCH, TP_ECVALUE_SELFAN1);
                m_ecManager->WriteByte(TP_ECOFFSET_FAN, (char)level);
                ::Sleep(100);

                // Set Fan 2
                m_ecManager->WriteByte(TP_ECOFFSET_FAN_SWITCH, TP_ECVALUE_SELFAN2);
                m_ecManager->WriteByte(TP_ECOFFSET_FAN, (char)level);
                ::Sleep(100);

                // Verify Fan 2
                char currentFan2;
                bool fan2_ok = m_ecManager->ReadByte(TP_ECOFFSET_FAN, &currentFan2);
                ::Sleep(100);

                // Switch back to Fan 1 and Verify
                m_ecManager->WriteByte(TP_ECOFFSET_FAN_SWITCH, TP_ECVALUE_SELFAN1);
                ::Sleep(100);
                char currentFan1;
                bool fan1_ok = m_ecManager->ReadByte(TP_ECOFFSET_FAN, &currentFan1);

                if (fan1_ok && fan2_ok && (unsigned char)currentFan1 == (unsigned char)level && (unsigned char)currentFan2 == (unsigned char)level) {
                    ok = true;
                    break;
                }
            } else {
                m_ecManager->WriteByte(TP_ECOFFSET_FAN, (char)level);
                ::Sleep(100);
                char currentFan;
                if (m_ecManager->ReadByte(TP_ECOFFSET_FAN, &currentFan) && (unsigned char)currentFan == (unsigned char)level) {
                    ok = true;
                    break;
                }
            }
            ::Sleep(300);
        }
    }

    if (ok) {
        m_currentFanCtrl = level;
        if (m_onChange) m_onChange(level);
    }
    return ok;
}

bool FanController::SetFanLevels(int level1, int level2) {
    std::lock_guard<std::recursive_timed_mutex> lock(m_ecManager->GetMutex());
    bool ok1 = false, ok2 = false;

    for (int i = 0; i < 3; i++) {
        // Set Fan 1
        m_ecManager->WriteByte(TP_ECOFFSET_FAN_SWITCH, TP_ECVALUE_SELFAN1);
        m_ecManager->WriteByte(TP_ECOFFSET_FAN, (char)level1);
        ::Sleep(50);

        // Set Fan 2
        m_ecManager->WriteByte(TP_ECOFFSET_FAN_SWITCH, TP_ECVALUE_SELFAN2);
        m_ecManager->WriteByte(TP_ECOFFSET_FAN, (char)level2);
        ::Sleep(50);

        // Verify
        char c1, c2;
        m_ecManager->WriteByte(TP_ECOFFSET_FAN_SWITCH, TP_ECVALUE_SELFAN1);
        m_ecManager->ReadByte(TP_ECOFFSET_FAN, &c1);
        m_ecManager->WriteByte(TP_ECOFFSET_FAN_SWITCH, TP_ECVALUE_SELFAN2);
        m_ecManager->ReadByte(TP_ECOFFSET_FAN, &c2);

        if ((unsigned char)c1 == (unsigned char)level1 && (unsigned char)c2 == (unsigned char)level2) {
            ok1 = ok2 = true;
            break;
        }
        ::Sleep(100);
    }

    if (ok1 && ok2) {
        m_currentFanCtrl = level1; // For legacy compatibility
        if (m_onChange) m_onChange(level1);
    }
    return ok1 && ok2;
}

bool FanController::UpdateSmartControl(int maxTemp, const std::vector<SmartLevel>& levels) {
    if (levels.empty()) return false;

    int newFanCtrl = -1;
    int levelIndex = -1;

    // Logic for finding the right level based on maxTemp and hysteresis
    // This is a simplified version of the original logic
    for (int i = 0; i < (int)levels.size(); i++) {
        if (levels[i].temp == -1) break;
        if (maxTemp >= levels[i].temp) {
            newFanCtrl = levels[i].fan;
            levelIndex = i;
        }
    }

    if (newFanCtrl != -1 && newFanCtrl != m_currentFanCtrl) {
        if (m_lastSmartLevelIndex != -1) {
            const auto& lastLevel = levels[m_lastSmartLevelIndex];
            
            if (levelIndex < m_lastSmartLevelIndex) { // Cooling down
                if (maxTemp >= lastLevel.temp - lastLevel.hystDown)
                    return true; // Stay in current (higher) level
            } else { // Heating up
                const auto& newLevel = levels[levelIndex];
                if (maxTemp < newLevel.temp + newLevel.hystUp)
                    return true; // Stay in current (lower) level
            }
        }

        m_lastSmartLevelIndex = levelIndex;
        return SetFanLevel(newFanCtrl);
    }

    return true;
}

bool FanController::UpdatePIDControl(float currentTemp, const PIDSettings& settings, float dt) {
    float error = currentTemp - settings.targetTemp;
    
    // Proportional
    float P = settings.Kp * error;
    
    // Integral (with anti-windup)
    m_integral += error * dt;
    if (m_integral > 10.0f) m_integral = 10.0f;
    if (m_integral < -10.0f) m_integral = -10.0f;
    float I = settings.Ki * m_integral;
    
    // Derivative
    float D = settings.Kd * (error - m_lastError) / (dt > 0 ? dt : 1.0f);
    m_lastError = error;
    
    float output = P + I + D;
    
    // Map output to fan levels (0-7)
    // If output is positive, we need more cooling.
    // Base level could be 1 or 2 if we are near target.
    int targetLevel = 0;
    if (output > 0) {
        targetLevel = (int)(output + 1.0f); // Simple mapping
    } else if (currentTemp > settings.targetTemp - 5.0f) {
        targetLevel = 1; // Keep a low spin if we are close to target
    }
    
    if (targetLevel < (int)settings.minFan) targetLevel = (int)settings.minFan;
    if (targetLevel > (int)settings.maxFan) targetLevel = (int)settings.maxFan;
    
    if (targetLevel != m_currentFanCtrl) {
        return SetFanLevel(targetLevel);
    }
    
    return true;
}

bool FanController::GetFanSpeeds(int& fan1, int& fan2) {
    std::lock_guard<std::recursive_timed_mutex> lock(m_ecManager->GetMutex());

    // Helper lambda to read a single fan's speed
    auto readFanSpeed = [this](char fanSelect) -> int {
        m_ecManager->WriteByte(TP_ECOFFSET_FAN_SWITCH, fanSelect);
        char lo, hi;
        if (!m_ecManager->ReadByte(TP_ECOFFSET_FANSPEED, &lo) || 
            !m_ecManager->ReadByte(TP_ECOFFSET_FANSPEED + 1, &hi)) {
            return -1;  // Error indicator
        }
        return ((unsigned char)hi << 8) | (unsigned char)lo;
    };

    int speed2 = readFanSpeed(TP_ECVALUE_SELFAN2);
    if (speed2 < 0) return false;
    fan2 = speed2;

    int speed1 = readFanSpeed(TP_ECVALUE_SELFAN1);
    if (speed1 < 0) return false;
    fan1 = speed1;

    return true;
}
