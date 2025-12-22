#pragma once

#include <string>

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
};
