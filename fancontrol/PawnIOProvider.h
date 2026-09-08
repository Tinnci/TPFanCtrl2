#pragma once

#include "IIOProvider.h"
#include <windows.h>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

class PawnIOProvider : public IIOProvider {
public:
    explicit PawnIOProvider(std::function<void(const char*)> traceCallback = nullptr);
    virtual ~PawnIOProvider();

    // Initialize PawnIO connection and load LpcACPIEC module.
    // If customModulePath is empty, attempts to load from disk or fallback to embedded blob.
    bool Initialize(const std::wstring& customModulePath = L"");
    void Close();

    bool IsActive() const { return m_handle != nullptr && m_handle != INVALID_HANDLE_VALUE; }

    virtual BYTE ReadPort(USHORT port) override;
    virtual void WritePort(USHORT port, BYTE value) override;

    virtual bool AcquireLock(DWORD timeoutMs = 1000) override;
    virtual void ReleaseLock() override;

    uint32_t GetVersion() const { return m_version; }

private:
    std::function<void(const char*)> m_trace;
    HMODULE m_hLib = nullptr;
    HANDLE m_handle = nullptr;
    HANDLE m_ecMutex = nullptr;
    uint32_t m_version = 0;

    typedef HRESULT (WINAPI *pfn_pawnio_version)(PULONG version);
    typedef HRESULT (WINAPI *pfn_pawnio_open)(PHANDLE handle);
    typedef HRESULT (WINAPI *pfn_pawnio_load)(HANDLE handle, const UCHAR* blob, SIZE_T size);
    typedef HRESULT (WINAPI *pfn_pawnio_execute)(
        HANDLE handle,
        PCSTR name,
        const ULONG64* in,
        SIZE_T in_size,
        PULONG64 out,
        SIZE_T out_size,
        PSIZE_T return_size
    );
    typedef HRESULT (WINAPI *pfn_pawnio_close)(HANDLE handle);

    pfn_pawnio_version m_pfn_version = nullptr;
    pfn_pawnio_open m_pfn_open = nullptr;
    pfn_pawnio_load m_pfn_load = nullptr;
    pfn_pawnio_execute m_pfn_execute = nullptr;
    pfn_pawnio_close m_pfn_close = nullptr;

    bool LoadPawnIOLibrary();
    std::vector<BYTE> GetModuleBlob(const std::wstring& customPath);
    std::wstring FindPawnIOLibPath();
    void Trace(const std::string& msg);
};
