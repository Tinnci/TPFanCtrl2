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
    if (!WaitForFlags(m_ctrlPort, ACPI_EC_FLAG_IBF | ACPI_EC_FLAG_OBF)) {
        if (m_trace) m_trace("readec: timed out #1, switching EC type");
        SwitchECType();
        return false;
    }

    m_io->WritePort(m_ctrlPort, ACPI_EC_COMMAND_READ);

    if (!WaitForFlags(m_ctrlPort, ACPI_EC_FLAG_IBF)) {
        if (m_trace) m_trace("readec: timed out #2");
        return false;
    }

    m_io->WritePort(m_dataPort, (char)offset);

    if (!WaitForFlags(m_ctrlPort, ACPI_EC_FLAG_IBF)) {
        if (m_trace) m_trace("readec: timed out #3");
        return false;
    }

    *pdata = m_io->ReadPort(m_dataPort);
    return true;
}

bool ECManager::WriteByte(int offset, char data) {
    if (!WaitForFlags(m_ctrlPort, ACPI_EC_FLAG_IBF | ACPI_EC_FLAG_OBF)) {
        if (m_trace) m_trace("writeec: timed out #1");
        return false;
    }

    m_io->WritePort(m_ctrlPort, ACPI_EC_COMMAND_WRITE);

    if (!WaitForFlags(m_ctrlPort, ACPI_EC_FLAG_IBF)) {
        if (m_trace) m_trace("writeec: timed out #2");
        return false;
    }

    m_io->WritePort(m_dataPort, (char)offset);

    if (!WaitForFlags(m_ctrlPort, ACPI_EC_FLAG_IBF)) {
        if (m_trace) m_trace("writeec: timed out #3");
        return false;
    }

    m_io->WritePort(m_dataPort, data);

    if (!WaitForFlags(m_ctrlPort, ACPI_EC_FLAG_IBF)) {
        if (m_trace) m_trace("writeec: timed out #4");
        return false;
    }

    return true;
}
