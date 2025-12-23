#include "_prec.h"
#include "SensorManager.h"
#include "TVicPort.h"
#include <algorithm>

SensorManager::SensorManager(std::shared_ptr<ECManager> ecManager)
    : m_ecManager(ecManager) {
    m_sensors.resize(MAX_SENSORS);
    m_offsets.resize(MAX_SENSORS, {0, -1, -1});
    m_history.resize(MAX_SENSORS);
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
        return false; 
    }

    // Helper lambda to read and process a single sensor
    auto readSensor = [this](int idx) -> bool {
        char temp;
        if (!m_ecManager->ReadByte(m_sensors[idx].addr, &temp)) {
            return false;
        }
        
        int raw = (unsigned char)temp;
        bool isValid = (raw > 0 && raw < 128);
        
        // Special case: 0 might be valid on cold start if it's the very first reading
        if (raw == 0 && !m_sensors[idx].isAvailable) {
            isValid = true; 
        }

        if (isValid) {
            m_sensors[idx].isAvailable = true;
            m_history[idx].lastValid = raw;
            m_history[idx].invalidCycles = 0;
            
            // Update moving average
            m_history[idx].values[m_history[idx].count % 5] = raw;
            m_history[idx].count++;
        } else {
            // If invalid, use last valid for a few cycles to prevent fan jitter/stoppage
            if (m_sensors[idx].isAvailable && m_history[idx].invalidCycles < 3) {
                raw = m_history[idx].lastValid;
                m_history[idx].invalidCycles++;
            } else {
                // Truly invalid or timed out
                m_sensors[idx].rawTemp = raw;
                m_sensors[idx].biasedTemp = raw;
                return true;
            }
        }

        // Calculate smoothed value
        int sum = 0;
        int samples = (std::min)(m_history[idx].count, 5);
        for (int i = 0; i < samples; i++) {
            sum += m_history[idx].values[i];
        }
        int smoothed = sum / samples;

        m_sensors[idx].rawTemp = smoothed;

        // Apply offset with hysteresis
        int offset = m_offsets[idx].offset;
        if (smoothed >= m_offsets[idx].hystMin && 
            smoothed <= m_offsets[idx].hystMax) {
            offset = 0;
        }
        m_sensors[idx].biasedTemp = smoothed - offset;
        return true;
    };

    // Read primary sensors (0-7) at addresses 0x78-0x7F
    for (int i = 0; i < 8; i++) {
        m_sensors[i].addr = TP_ECOFFSET_TEMP0 + i;
        if (!readSensor(i)) return false;
    }

    // Read extended sensors (8-11) at addresses 0xC0-0xC3
    for (int i = 0; i < 4; i++) {
        int idx = 8 + i;
        m_sensors[idx].addr = TP_ECOFFSET_TEMP1 + i;
        
        if (noExtSensor) {
            m_sensors[idx].rawTemp = 0;
            m_sensors[idx].biasedTemp = 0;
            continue;
        }
        
        if (!readSensor(idx)) return false;
    }

    return true;
}

int SensorManager::GetMaxTemp(int& maxIndex, const std::string& ignoreList) const {
    int maxTemp = 0;
    maxIndex = 0;
    int validCount = 0;

    for (int i = 0; i < MAX_SENSORS; i++) {
        // 1. Skip if not available (never returned a valid reading)
        if (!m_sensors[i].isAvailable) continue;

        // 2. Skip if in ignore list
        if (!m_sensors[i].name.empty() && ignoreList.find(m_sensors[i].name) != std::string::npos) {
            continue;
        }

        // 3. Skip if current reading is invalid (but it was available before)
        if (m_sensors[i].rawTemp <= 0 || m_sensors[i].rawTemp >= 128) {
            continue;
        }

        validCount++;
        int temp = (int)(m_sensors[i].biasedTemp * m_sensors[i].weight);
        if (temp > maxTemp) {
            maxTemp = temp;
            maxIndex = i;
        }
    }

    // If no valid sensors found, but we had some before, return last known max
    // to prevent fan from stopping suddenly.
    if (validCount == 0 && m_lastMaxTemp > 0) {
        return m_lastMaxTemp;
    }

    const_cast<SensorManager*>(this)->m_lastMaxTemp = maxTemp;
    return maxTemp;
}
