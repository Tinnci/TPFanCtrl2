#pragma once
#include "IIOProvider.h"
#include <map>

// Constants for EC simulation (matching ECManager)
#define ACPI_EC_COMMAND_READ 0x80
#define ACPI_EC_COMMAND_WRITE 0x81
#define ACPI_EC_TYPE1_CTRLPORT 0x1604
#define ACPI_EC_TYPE1_DATAPORT 0x1600
#define ACPI_EC_TYPE2_CTRLPORT 0x66
#define ACPI_EC_TYPE2_DATAPORT 0x62

class MockIOProvider : public IIOProvider {
public:
    virtual BYTE ReadPort(USHORT port) override {
        if ((port == ACPI_EC_TYPE1_DATAPORT || port == ACPI_EC_TYPE2_DATAPORT) && m_ecReadPending) {
            m_ecReadPending = false;
            return m_ecMemory[m_ecAddress];
        }
        return m_ports[port];
    }

    virtual void WritePort(USHORT port, BYTE value) override {
        m_lastWritePort = port;
        m_lastWriteValue = value;

        if (port == ACPI_EC_TYPE1_CTRLPORT || port == ACPI_EC_TYPE2_CTRLPORT) {
            if ((BYTE)value == (BYTE)ACPI_EC_COMMAND_READ) {
                m_ecState = 1; // Expecting address
            } else if ((BYTE)value == (BYTE)ACPI_EC_COMMAND_WRITE) {
                m_ecState = 2; // Expecting address for write
            }
            // Don't store command in status register
            m_ports[port] = 0; 
        } else if (port == ACPI_EC_TYPE1_DATAPORT || port == ACPI_EC_TYPE2_DATAPORT) {
            if (m_ecState == 1) {
                m_ecAddress = value;
                m_ecState = 0;
                m_ecReadPending = true;
            } else if (m_ecState == 2) {
                m_ecAddress = value;
                m_ecState = 3; // Expecting data
            } else if (m_ecState == 3) {
                m_ecMemory[m_ecAddress] = value;
                m_ecState = 0;
            }
            m_ports[port] = value;
        } else {
            m_ports[port] = value;
        }
    }

    void SetPortValue(USHORT port, BYTE value) {
        m_ports[port] = value;
    }

    void SetECByte(BYTE addr, BYTE value) {
        m_ecMemory[addr] = value;
    }

    USHORT GetLastWritePort() const { return m_lastWritePort; }
    BYTE GetLastWriteValue() const { return m_lastWriteValue; }

private:
    std::map<USHORT, BYTE> m_ports;
    std::map<BYTE, BYTE> m_ecMemory;
    USHORT m_lastWritePort = 0;
    BYTE m_lastWriteValue = 0;
    int m_ecState = 0;
    BYTE m_ecAddress = 0;
    bool m_ecReadPending = false;
};
