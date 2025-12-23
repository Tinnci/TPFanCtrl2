#include "_prec.h"
#include "ECManager.h"

ECManager::ECManager(std::shared_ptr<IIOProvider> ioProvider, std::function<void(const char*)> traceCallback)
    : m_io(ioProvider), m_trace(traceCallback), m_currentType(ECType::Type1) {
    m_ctrlPort = ACPI_EC_TYPE1_CTRLPORT;
    m_dataPort = ACPI_EC_TYPE1_DATAPORT;
    if (m_trace) m_trace("ECManager initialized with ACPI_EC_TYPE1");
}

ECManager::~ECManager() {}

bool ECManager::WaitForFlags(USHORT port, char flags, bool onoff, int timeout) {
    char data;
    int time = 0, sleepTicks = 10;

    for (time = 0; time < timeout; time += sleepTicks) {
        data = m_io->ReadPort(port);
        bool flagstate = (data & flags) != 0;
        if (flagstate == onoff) return true;
        ::Sleep(sleepTicks);
    }
    return false;
}

void ECManager::SwitchECType() {
    if (m_currentType == ECType::Type1) {
        m_currentType = ECType::Type2;
        m_ctrlPort = ACPI_EC_TYPE2_CTRLPORT;
        m_dataPort = ACPI_EC_TYPE2_DATAPORT;
        if (m_trace) m_trace("Switched to ACPI_EC_TYPE2");
    } else {
        m_currentType = ECType::Type1;
        m_ctrlPort = ACPI_EC_TYPE1_CTRLPORT;
        m_dataPort = ACPI_EC_TYPE1_DATAPORT;
        if (m_trace) m_trace("Switched to ACPI_EC_TYPE1");
    }
}

bool ECManager::ReadByte(int offset, char* pdata) {
    std::lock_guard<std::recursive_timed_mutex> lock(m_mutex);
    
    auto attemptRead = [&]() -> bool {
        if (!WaitForFlags(m_ctrlPort, ACPI_EC_FLAG_IBF | ACPI_EC_FLAG_OBF)) {
            return false;
        }

        m_io->WritePort(m_ctrlPort, ACPI_EC_COMMAND_READ);

        if (!WaitForFlags(m_ctrlPort, ACPI_EC_FLAG_IBF)) {
            return false;
        }

        m_io->WritePort(m_dataPort, (char)offset);

        if (!WaitForFlags(m_ctrlPort, ACPI_EC_FLAG_IBF)) {
            return false;
        }

        *pdata = m_io->ReadPort(m_dataPort);
        return true;
    };

    if (attemptRead()) return true;

    // If failed, switch type and try one more time
    if (m_trace) m_trace("readec: timed out, switching EC type and retrying...");
    SwitchECType();
    
    if (attemptRead()) return true;

    if (m_trace) m_trace("readec: critical timeout on both EC types");
    return false;
}

bool ECManager::WriteByte(int offset, char data) {
    std::lock_guard<std::recursive_timed_mutex> lock(m_mutex);

    auto attemptWrite = [&]() -> bool {
        if (!WaitForFlags(m_ctrlPort, ACPI_EC_FLAG_IBF | ACPI_EC_FLAG_OBF)) {
            return false;
        }

        m_io->WritePort(m_ctrlPort, ACPI_EC_COMMAND_WRITE);

        if (!WaitForFlags(m_ctrlPort, ACPI_EC_FLAG_IBF)) {
            return false;
        }

        m_io->WritePort(m_dataPort, (char)offset);

        if (!WaitForFlags(m_ctrlPort, ACPI_EC_FLAG_IBF)) {
            return false;
        }

        m_io->WritePort(m_dataPort, data);

        if (!WaitForFlags(m_ctrlPort, ACPI_EC_FLAG_IBF)) {
            return false;
        }

        return true;
    };

    if (attemptWrite()) return true;

    // If failed, switch type and try one more time
    if (m_trace) m_trace("writeec: timed out, switching EC type and retrying...");
    SwitchECType();

    if (attemptWrite()) return true;

    if (m_trace) m_trace("writeec: critical timeout on both EC types");
    return false;
}

bool ECManager::ToggleBitsWithVerify(int offset, char bits, char anywayBit, char& resultValue) {
    std::lock_guard<std::recursive_timed_mutex> lock(m_mutex);
    char currentVal;
    char targetVal;
    bool ok = false;

    for (int i = 0; i < 5; i++) {
        if (!ReadByte(offset, &currentVal)) {
            ::Sleep(300);
            continue;
        }

        if (currentVal & bits) {
            targetVal = (currentVal - bits) | anywayBit;
        } else {
            targetVal = (currentVal + bits) | anywayBit;
        }

        if (!WriteByte(offset, targetVal)) {
            ::Sleep(300);
            continue;
        }

        if (ReadByte(offset, &resultValue) && resultValue == targetVal) {
            ok = true;
            break;
        }

        ::Sleep(300);
    }
    return ok;
}
