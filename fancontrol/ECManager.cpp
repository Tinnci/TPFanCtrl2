#include "_prec.h"
#include "ECManager.h"

ECManager::ECManager(std::shared_ptr<IIOProvider> ioProvider, std::function<void(const char*)> traceCallback)
    : m_io(ioProvider), m_trace(traceCallback), m_currentType(ECType::Type1) {
    ApplyECType(ECType::Type1);

    // Probe both controller types quickly and stick to whichever responds first.
    if (!ProbeECType(m_currentType)) {
        if (ProbeECType(ECType::Type2)) {
            if (m_trace) m_trace("ECManager detected ACPI_EC_TYPE2 as responsive default");
        } else {
            // Fall back to Type1 even if probe failed so existing logic can still try switching
            ApplyECType(ECType::Type1);
            if (m_trace) m_trace("ECManager probe: neither EC type responded, defaulting to ACPI_EC_TYPE1");
        }
    } else {
        if (m_trace) m_trace("ECManager initialized with responsive ACPI_EC_TYPE1");
    }
}

ECManager::~ECManager() {}

bool ECManager::WaitForFlags(USHORT port, char flags, bool onoff, int timeout) {
    char data;
    int time = 0, sleepTicks = 10;

    for (time = 0; time < timeout; time += sleepTicks) {
        data = m_io->ReadPort(port);
        
        // If we get 0xFF, the port might not be accessible
        if ((unsigned char)data == 0xFF && time > 100) {
            // But don't return immediately, some ECs might transiently return 0xFF
        }

        bool flagstate = (data & flags) != 0;
        if (flagstate == onoff) return true;
        ::Sleep(sleepTicks);
    }
    return false;
}

void ECManager::SwitchECType() {
    auto newType = (m_currentType == ECType::Type1) ? ECType::Type2 : ECType::Type1;
    ApplyECType(newType);
    if (m_trace) {
        if (m_currentType == ECType::Type2) {
            m_trace("Switched to ACPI_EC_TYPE2");
        } else {
            m_trace("Switched to ACPI_EC_TYPE1");
        }
    }
}

void ECManager::ApplyECType(ECType type) {
    m_currentType = type;
    if (type == ECType::Type1) {
        m_ctrlPort = ACPI_EC_TYPE1_CTRLPORT;
        m_dataPort = ACPI_EC_TYPE1_DATAPORT;
    } else {
        m_ctrlPort = ACPI_EC_TYPE2_CTRLPORT;
        m_dataPort = ACPI_EC_TYPE2_DATAPORT;
    }
}

bool ECManager::ProbeECType(ECType type, int timeoutMs) {
    ApplyECType(type);
    return WaitForFlags(m_ctrlPort, ACPI_EC_FLAG_IBF | ACPI_EC_FLAG_OBF, false, timeoutMs);
}

bool ECManager::ReadByte(int offset, char* pdata) {
    std::lock_guard<std::recursive_timed_mutex> lock(m_mutex);
    
    auto drainObf = [&]() {
        for (int i = 0; i < 10; i++) {
            char status = m_io->ReadPort(m_ctrlPort);
            if (!(status & ACPI_EC_FLAG_OBF)) break;
            m_io->ReadPort(m_dataPort);
            ::Sleep(1);
        }
    };

    auto attemptRead = [&]() -> bool {
        drainObf();

        if (!WaitForFlags(m_ctrlPort, ACPI_EC_FLAG_IBF | ACPI_EC_FLAG_OBF)) {
            if (m_trace) m_trace(std::format("readec: flags timeout before cmd at offset 0x{:02X}", offset).c_str());
            return false;
        }

        m_io->WritePort(m_ctrlPort, ACPI_EC_COMMAND_READ);

        if (!WaitForFlags(m_ctrlPort, ACPI_EC_FLAG_IBF)) {
            if (m_trace) m_trace(std::format("readec: IBF timeout after cmd at offset 0x{:02X}", offset).c_str());
            return false;
        }

        m_io->WritePort(m_dataPort, (char)offset);

        if (!WaitForFlags(m_ctrlPort, ACPI_EC_FLAG_IBF)) {
            if (m_trace) m_trace(std::format("readec: IBF timeout after address at offset 0x{:02X}", offset).c_str());
            return false;
        }

        *pdata = m_io->ReadPort(m_dataPort);
        if (m_trace) m_trace(std::format("readec: offset 0x{:02X} -> 0x{:02X}", offset, (unsigned char)*pdata).c_str());
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

    auto drainObf = [&]() {
        for (int i = 0; i < 10; i++) {
            char status = m_io->ReadPort(m_ctrlPort);
            if (!(status & ACPI_EC_FLAG_OBF)) break;
            m_io->ReadPort(m_dataPort);
            ::Sleep(1);
        }
    };

    auto attemptWrite = [&]() -> bool {
        drainObf();

        if (!WaitForFlags(m_ctrlPort, ACPI_EC_FLAG_IBF | ACPI_EC_FLAG_OBF)) {
            if (m_trace) m_trace(std::format("writeec: flags timeout before cmd at offset 0x{:02X}", offset).c_str());
            return false;
        }

        m_io->WritePort(m_ctrlPort, ACPI_EC_COMMAND_WRITE);

        if (!WaitForFlags(m_ctrlPort, ACPI_EC_FLAG_IBF)) {
            if (m_trace) m_trace(std::format("writeec: IBF timeout after cmd at offset 0x{:02X}", offset).c_str());
            return false;
        }

        m_io->WritePort(m_dataPort, (char)offset);

        if (!WaitForFlags(m_ctrlPort, ACPI_EC_FLAG_IBF)) {
            if (m_trace) m_trace(std::format("writeec: IBF timeout after address at offset 0x{:02X}", offset).c_str());
            return false;
        }

        m_io->WritePort(m_dataPort, data);

        if (!WaitForFlags(m_ctrlPort, ACPI_EC_FLAG_IBF)) {
            if (m_trace) m_trace(std::format("writeec: IBF timeout after data at offset 0x{:02X}", offset).c_str());
            return false;
        }

        if (m_trace) m_trace(std::format("writeec: offset 0x{:02X} <= 0x{:02X}", offset, (unsigned char)data).c_str());
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
