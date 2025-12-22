#pragma once
#include "IIOProvider.h"

class TVicPortProvider : public IIOProvider {
public:
    TVicPortProvider();
    virtual ~TVicPortProvider();

    virtual BYTE ReadPort(USHORT port) override;
    virtual void WritePort(USHORT port, BYTE value) override;
};
