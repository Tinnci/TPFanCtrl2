#include "FanController.h"

FanController::FanController(std::shared_ptr<ECManager> ecManager)
    : m_ecManager(ecManager), m_currentFanCtrl(-1), m_lastSmartLevelIndex(-1) {}

bool FanController::SetFanLevel(int level, bool isDualFan) {
    bool ok = true;
    if (isDualFan) {
        // Set Fan 1
        ok &= m_ecManager->WriteByte(TP_ECOFFSET_FAN_SWITCH, TP_ECVALUE_SELFAN1);
        ok &= m_ecManager->WriteByte(TP_ECOFFSET_FAN, (char)level);
        ::Sleep(100);
        // Set Fan 2
        ok &= m_ecManager->WriteByte(TP_ECOFFSET_FAN_SWITCH, TP_ECVALUE_SELFAN2);
        ok &= m_ecManager->WriteByte(TP_ECOFFSET_FAN, (char)level);
        ::Sleep(100);
    } else {
        ok &= m_ecManager->WriteByte(TP_ECOFFSET_FAN, (char)level);
    }

    if (ok) {
        m_currentFanCtrl = level;
        if (m_onChange) m_onChange(level);
    }
    return ok;
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

bool FanController::GetFanSpeeds(int& fan1, int& fan2) {
    char lo, hi;

    // Fan 2
    m_ecManager->WriteByte(TP_ECOFFSET_FAN_SWITCH, TP_ECVALUE_SELFAN2);
    if (!m_ecManager->ReadByte(TP_ECOFFSET_FANSPEED, &lo) || !m_ecManager->ReadByte(TP_ECOFFSET_FANSPEED + 1, &hi)) {
        return false;
    }
    fan2 = ((unsigned char)hi << 8) | (unsigned char)lo;

    // Fan 1
    m_ecManager->WriteByte(TP_ECOFFSET_FAN_SWITCH, TP_ECVALUE_SELFAN1);
    if (!m_ecManager->ReadByte(TP_ECOFFSET_FANSPEED, &lo) || !m_ecManager->ReadByte(TP_ECOFFSET_FANSPEED + 1, &hi)) {
        return false;
    }
    fan1 = ((unsigned char)hi << 8) | (unsigned char)lo;

    return true;
}
