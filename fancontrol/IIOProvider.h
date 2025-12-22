#pragma once

#include <windows.h>

// Abstract I/O Provider interface for decoupling hardware access
class IIOProvider {
public:
    virtual ~IIOProvider() {}
    virtual BYTE ReadPort(USHORT port) = 0;
    virtual void WritePort(USHORT port, BYTE data) = 0;
};
