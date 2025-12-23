#include "_prec.h"
#include "SensorManager.h"
#include "TVicPort.h"
#include <algorithm>

SensorManager::SensorManager(std::shared_ptr<ECManager> ecManager)
    : m_ecManager(ecManager) {
    m_sensors.resize(MAX_SENSORS);
    m_offsets.resize(MAX_SENSORS, {0, -1, -1});
}

void SensorManager::SetOffset(int index, int offset, int hystMin, int hystMax) {
    if (index >= 0 && index < MAX_SENSORS) {
        m_offsets[index] = {offset, hystMin, hystMax};
    }
}

void SensorManager::SetSensorName(int index, const std::string& name) {
    if (index >= 0 && index < MAX_SENSORS) {
        m_sensors[index].name = name;
    }
}

bool SensorManager::UpdateSensors(bool showBiasedTemps, bool noExtSensor, bool useTWR) {
    std::lock_guard<std::recursive_timed_mutex> lock(m_ecManager->GetMutex());
    if (useTWR) {
        // TWR logic is complex and specific to some models, 
        // for now we focus on the standard EC read.
        // TODO: Implement TWR if needed for the baseline.
        return false; 
    }

    for (int i = 0; i < 8; i++) {
        m_sensors[i].addr = TP_ECOFFSET_TEMP0 + i;
        char temp;
        if (m_ecManager->ReadByte(m_sensors[i].addr, &temp)) {
            m_sensors[i].rawTemp = (unsigned char)temp;
            int offset = m_offsets[i].offset;
            if (m_sensors[i].rawTemp >= m_offsets[i].hystMin && m_sensors[i].rawTemp <= m_offsets[i].hystMax) {
                offset = 0;
            }
            m_sensors[i].biasedTemp = m_sensors[i].rawTemp - offset;
        } else {
            return false;
        }
    }

    for (int i = 0; i < 4; i++) {
        int idx = 8 + i;
        m_sensors[idx].addr = TP_ECOFFSET_TEMP1 + i;
        if (noExtSensor) {
            m_sensors[idx].rawTemp = 0;
            m_sensors[idx].biasedTemp = 0;
            continue;
        }
        char temp;
        if (m_ecManager->ReadByte(m_sensors[idx].addr, &temp)) {
            m_sensors[idx].rawTemp = (unsigned char)temp;
            int offset = m_offsets[idx].offset;
            if (m_sensors[idx].rawTemp >= m_offsets[idx].hystMin && m_sensors[idx].rawTemp <= m_offsets[idx].hystMax) {
                offset = 0;
            }
            m_sensors[idx].biasedTemp = m_sensors[idx].rawTemp - offset;
        } else {
            return false;
        }
    }

    return true;
}

int SensorManager::GetMaxTemp(int& maxIndex) const {
    int maxTemp = 0;
    maxIndex = 0;

    // Normalize ignore list: replace commas with pipes and wrap in pipes
    std::string normalizedList = "|" + m_ignoreList + "|";
    for (auto& c : normalizedList) if (c == ',') c = '|';

    for (int i = 0; i < MAX_SENSORS; i++) {
        if (!m_sensors[i].name.empty()) {
            std::string sensorMatch = "|" + m_sensors[i].name + "|";
            if (normalizedList.find(sensorMatch) != std::string::npos) {
                continue;
            }
        }

        if (m_sensors[i].rawTemp != 0x80 && m_sensors[i].rawTemp != 0x00) {
            int temp = m_sensors[i].biasedTemp;
            if (temp < 128) {
                if (temp > maxTemp) {
                    maxTemp = temp;
                    maxIndex = i;
                }
            }
        }
    }
    return maxTemp;
}
