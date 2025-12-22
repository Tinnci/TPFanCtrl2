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
    int GetMaxTemp(const std::string& ignoreList, int& maxIndex);
    const std::vector<SensorData>& GetSensors() const { return m_sensors; }
    void SetOffset(int index, int offset, int hystMin, int hystMax);
    void SetSensorName(int index, const std::string& name);

private:
    std::shared_ptr<ECManager> m_ecManager;
    std::vector<SensorData> m_sensors;
    std::vector<SensorOffset> m_offsets;

    static constexpr int MAX_SENSORS = 12;
    static constexpr auto TP_ECOFFSET_TEMP0 = 0x78;
    static constexpr auto TP_ECOFFSET_TEMP1 = 0xC0;
};
