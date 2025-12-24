#pragma once

#include <windows.h>
#include <functional>
#include <memory>
#include <mutex>
#include "IIOProvider.h"

class ECManager {
public:
    enum class ECType {
        Type1, // 0x1600 / 0x1604
        Type2  // 0x62 / 0x66
    };

    ECManager(std::shared_ptr<IIOProvider> ioProvider, std::function<void(const char*)> traceCallback);
    ~ECManager();

    bool ReadByte(int offset, char* pdata);
    bool WriteByte(int offset, char data);
    bool ToggleBitsWithVerify(int offset, char bits, char anywayBit, char& resultValue);
    std::recursive_timed_mutex& GetMutex() { return m_mutex; }

private:
    bool WaitForFlags(USHORT port, char flags, bool onoff = false, int timeout = 2000);
    void SwitchECType();
    void ApplyECType(ECType type);
    bool ProbeECType(ECType type, int timeoutMs = 100);

    std::shared_ptr<IIOProvider> m_io;
    int m_ctrlPort;
    int m_dataPort;
    ECType m_currentType;
    std::function<void(const char*)> m_trace;
    std::recursive_timed_mutex m_mutex;

    static constexpr auto ACPI_EC_TYPE1_CTRLPORT = 0x1604;
    static constexpr auto ACPI_EC_TYPE1_DATAPORT = 0x1600;
    static constexpr auto ACPI_EC_TYPE2_CTRLPORT = 0x66;
    static constexpr auto ACPI_EC_TYPE2_DATAPORT = 0x62;

    static constexpr auto ACPI_EC_FLAG_OBF = 0x01;
    static constexpr auto ACPI_EC_FLAG_IBF = 0x02;
    static constexpr auto ACPI_EC_FLAG_CMD = 0x08;

    static constexpr auto ACPI_EC_COMMAND_READ = (char)0x80;
    static constexpr auto ACPI_EC_COMMAND_WRITE = (char)0x81;
};
