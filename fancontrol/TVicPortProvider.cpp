#include "_prec.h"
#include "TVicPortProvider.h"

#ifdef ENABLE_TVICPORT
#include "TVicPort.h"

TVicPortProvider::TVicPortProvider() {}
TVicPortProvider::~TVicPortProvider() {}

BYTE TVicPortProvider::ReadPort(USHORT port) {
    return ::ReadPort(port);
}

void TVicPortProvider::WritePort(USHORT port, BYTE value) {
    ::WritePort(port, value);
}
#endif
