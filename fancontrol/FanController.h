#pragma once

#include <memory>
#include <vector>
#include <functional>
#include "ECManager.h"
#include "CommonTypes.h"

class FanController {
public:
    FanController(std::shared_ptr<ECManager> ecManager);

    bool SetFanLevel(int level, bool isDualFan = true);
    bool UpdateSmartControl(int maxTemp, const std::vector<SmartLevel>& levels);
    bool GetFanSpeeds(int& fan1, int& fan2);
    int GetCurrentFanCtrl() const { return m_currentFanCtrl; }
    void SetCurrentFanCtrl(int ctrl) { m_currentFanCtrl = ctrl; }
    void SetOnChangeCallback(std::function<void(int)> callback) { m_onChange = callback; }
    void SetWriteCallback(std::function<bool(int)> callback) { m_writeCallback = callback; }

private:
    std::shared_ptr<ECManager> m_ecManager;
    int m_currentFanCtrl;
    int m_lastSmartLevelIndex;
    std::function<void(int)> m_onChange;
    std::function<bool(int)> m_writeCallback;
};
