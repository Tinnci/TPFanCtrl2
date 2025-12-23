#pragma once

#include <string>

// EC Offsets and Values
constexpr auto TP_ECOFFSET_FAN = 0x2F;
constexpr auto TP_ECOFFSET_FANSPEED = 0x84;
constexpr auto TP_ECOFFSET_TEMP0 = 0x78;
constexpr auto TP_ECOFFSET_TEMP1 = 0xC0;
constexpr auto TP_ECOFFSET_FAN_SWITCH = 0x31;
constexpr auto TP_ECVALUE_SELFAN1 = 0x00;
constexpr auto TP_ECVALUE_SELFAN2 = 0x01;

struct SmartLevel {
    int temp;
    int fan;
    int hystUp;
    int hystDown;
};

struct SensorOffset {
    int offset;
    int hystMin;
    int hystMax;
};

struct SensorData {
    std::string name;
    int addr;
    int rawTemp;
    int biasedTemp;
    bool isAvailable = false; // True if it has ever returned a valid reading
};
