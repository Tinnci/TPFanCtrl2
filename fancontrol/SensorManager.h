#pragma once

#include <vector>
#include <string>
#include <memory>
#include "ECManager.h"
#include "CommonTypes.h"

class SensorManager {
public:
    SensorManager(std::shared_ptr<ECManager> ecManager);

    bool UpdateSensors(bool showBiasedTemps, bool noExtSensor, bool useTWR);
    int GetMaxTemp(int& maxIndex, const std::string& ignoreList) const;
    const std::vector<SensorData>& GetSensors() const { return m_sensors; }
    const SensorData& GetSensor(int index) const { return m_sensors[index]; }
    void SetOffset(int index, int offset, int hystMin, int hystMax);
    void SetSensorName(int index, const std::string& name);
    void SetSensorWeight(int index, float weight);

private:
    std::shared_ptr<ECManager> m_ecManager;
    std::vector<SensorData> m_sensors;
    std::vector<SensorOffset> m_offsets;
    int m_lastMaxTemp = 0;
    
    struct SensorHistory {
        int values[5] = {0};
        int count = 0;
        int lastValid = 0;
        int invalidCycles = 0;
    };
    std::vector<SensorHistory> m_history;

    static constexpr int MAX_SENSORS = 12;
};
