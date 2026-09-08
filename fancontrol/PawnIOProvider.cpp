#include "_prec.h"
#include "PawnIOProvider.h"
#include <filesystem>
#include <fstream>
#include <format>

namespace {
const unsigned char g_embeddedLpcAcpiEc[] = {
#include "LpcACPIEC_blob.inl"
};
}

PawnIOProvider::PawnIOProvider(std::function<void(const char*)> traceCallback)
    : m_trace(std::move(traceCallback)) {
}

PawnIOProvider::~PawnIOProvider() {
    Close();
}

void PawnIOProvider::Trace(const std::string& msg) {
    if (m_trace) {
        m_trace(msg.c_str());
    }
}

std::wstring PawnIOProvider::FindPawnIOLibPath() {
    wchar_t exePath[MAX_PATH] = { 0 };
    if (GetModuleFileNameW(nullptr, exePath, MAX_PATH) > 0) {
        std::filesystem::path p(exePath);
        auto candidate = p.parent_path() / L"PawnIOLib.dll";
        if (std::filesystem::exists(candidate)) {
            return candidate.wstring();
        }
    }

    const std::wstring standardPath = L"C:\\Program Files\\PawnIO\\PawnIOLib.dll";
    if (std::filesystem::exists(standardPath)) {
        return standardPath;
    }

    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                      L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\PawnIO",
                      0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t installDir[MAX_PATH] = { 0 };
        DWORD bufSize = sizeof(installDir);
        DWORD type = 0;
        if (RegQueryValueExW(hKey, L"InstallLocation", nullptr, &type, (LPBYTE)installDir, &bufSize) == ERROR_SUCCESS) {
            std::filesystem::path candidate = std::filesystem::path(installDir) / L"PawnIOLib.dll";
            if (std::filesystem::exists(candidate)) {
                RegCloseKey(hKey);
                return candidate.wstring();
            }
        }
        RegCloseKey(hKey);
    }

    return L"PawnIOLib.dll";
}

bool PawnIOProvider::LoadPawnIOLibrary() {
    if (m_hLib) return true;

    std::wstring libPath = FindPawnIOLibPath();
    Trace(std::format("Loading PawnIOLib from: {}", std::string(libPath.begin(), libPath.end())));
    m_hLib = LoadLibraryW(libPath.c_str());
    if (!m_hLib) {
        DWORD err = GetLastError();
        Trace(std::format("Failed to LoadLibraryW PawnIOLib.dll (Win32 error: {})", err));
        return false;
    }

    m_pfn_version = reinterpret_cast<pfn_pawnio_version>(GetProcAddress(m_hLib, "pawnio_version"));
    m_pfn_open = reinterpret_cast<pfn_pawnio_open>(GetProcAddress(m_hLib, "pawnio_open"));
    m_pfn_load = reinterpret_cast<pfn_pawnio_load>(GetProcAddress(m_hLib, "pawnio_load"));
    m_pfn_execute = reinterpret_cast<pfn_pawnio_execute>(GetProcAddress(m_hLib, "pawnio_execute"));
    m_pfn_close = reinterpret_cast<pfn_pawnio_close>(GetProcAddress(m_hLib, "pawnio_close"));

    if (!m_pfn_open || !m_pfn_load || !m_pfn_execute || !m_pfn_close) {
        Trace("Failed to locate required PawnIOLib exports");
        FreeLibrary(m_hLib);
        m_hLib = nullptr;
        return false;
    }

    return true;
}

std::vector<BYTE> PawnIOProvider::GetModuleBlob(const std::wstring& customPath) {
    if (!customPath.empty() && std::filesystem::exists(customPath)) {
        std::ifstream file(customPath, std::ios::binary | std::ios::ate);
        if (file.is_open()) {
            std::streamsize size = file.tellg();
            file.seekg(0, std::ios::beg);
            std::vector<BYTE> buffer(size);
            if (file.read(reinterpret_cast<char*>(buffer.data()), size)) {
                Trace(std::format("Loaded LpcACPIEC module from custom path: size {}", size));
                return buffer;
            }
        }
    }

    wchar_t exePath[MAX_PATH] = { 0 };
    if (GetModuleFileNameW(nullptr, exePath, MAX_PATH) > 0) {
        std::filesystem::path p(exePath);
        std::vector<std::filesystem::path> candidates = {
            p.parent_path() / L"LpcACPIEC.bin",
            p.parent_path() / L"res" / L"LpcACPIEC.bin",
            p.parent_path() / L"assets" / L"LpcACPIEC.bin",
            p.parent_path().parent_path() / L"assets" / L"LpcACPIEC.bin"
        };
        for (const auto& c : candidates) {
            if (std::filesystem::exists(c)) {
                std::ifstream file(c, std::ios::binary | std::ios::ate);
                if (file.is_open()) {
                    std::streamsize size = file.tellg();
                    file.seekg(0, std::ios::beg);
                    std::vector<BYTE> buffer(size);
                    if (file.read(reinterpret_cast<char*>(buffer.data()), size)) {
                        Trace(std::format("Loaded LpcACPIEC module from disk: size {}", size));
                        return buffer;
                    }
                }
            }
        }
    }

    Trace(std::format("Using embedded LpcACPIEC module: size {}", sizeof(g_embeddedLpcAcpiEc)));
    return std::vector<BYTE>(g_embeddedLpcAcpiEc, g_embeddedLpcAcpiEc + sizeof(g_embeddedLpcAcpiEc));
}

bool PawnIOProvider::Initialize(const std::wstring& customModulePath) {
    if (!LoadPawnIOLibrary()) {
        return false;
    }

    if (m_pfn_version) {
        ULONG ver = 0;
        if (SUCCEEDED(m_pfn_version(&ver))) {
            m_version = ver;
            Trace(std::format("PawnIO driver version: 0x{:06X}", m_version));
        }
    }

    HRESULT hr = m_pfn_open(&m_handle);
    if (FAILED(hr) || !m_handle) {
        Trace(std::format("pawnio_open failed (HRESULT: 0x{:08X})", (uint32_t)hr));
        Close();
        return false;
    }
    Trace(std::format("pawnio_open succeeded, handle={:p}", m_handle));

    auto blob = GetModuleBlob(customModulePath);
    if (blob.empty()) {
        Trace("Failed to get LpcACPIEC binary module");
        Close();
        return false;
    }

    hr = m_pfn_load(m_handle, blob.data(), blob.size());
    if (FAILED(hr)) {
        Trace(std::format("pawnio_load(LpcACPIEC) failed (HRESULT: 0x{:08X})", (uint32_t)hr));
        Close();
        return false;
    }
    Trace("pawnio_load(LpcACPIEC) succeeded");

    // Acquire or create the Access_EC mutex used by ACPI.sys
    m_ecMutex = CreateMutexW(nullptr, FALSE, L"Global\\Access_EC");
    if (!m_ecMutex) {
        m_ecMutex = CreateMutexW(nullptr, FALSE, L"Access_EC");
    }
    if (m_ecMutex) {
        Trace("Access_EC mutex handle created/acquired");
    } else {
        Trace(std::format("Warning: CreateMutex(Access_EC) failed (Win32: {})", GetLastError()));
    }

    return true;
}

void PawnIOProvider::Close() {
    if (m_handle && m_pfn_close) {
        m_pfn_close(m_handle);
        m_handle = nullptr;
    }
    if (m_ecMutex) {
        CloseHandle(m_ecMutex);
        m_ecMutex = nullptr;
    }
    if (m_hLib) {
        FreeLibrary(m_hLib);
        m_hLib = nullptr;
    }
    m_pfn_open = nullptr;
    m_pfn_load = nullptr;
    m_pfn_execute = nullptr;
    m_pfn_close = nullptr;
    m_pfn_version = nullptr;
}

bool PawnIOProvider::AcquireLock(DWORD timeoutMs) {
    if (m_ecMutex) {
        DWORD res = WaitForSingleObject(m_ecMutex, timeoutMs);
        return (res == WAIT_OBJECT_0 || res == WAIT_ABANDONED);
    }
    return true;
}

void PawnIOProvider::ReleaseLock() {
    if (m_ecMutex) {
        ReleaseMutex(m_ecMutex);
    }
}

BYTE PawnIOProvider::ReadPort(USHORT port) {
    if (!m_handle || !m_pfn_execute) return 0xFF;

    if (port != 0x62 && port != 0x66) {
        return 0xFF;
    }

    ULONG64 in[1] = { port };
    ULONG64 out[1] = { 0 };
    SIZE_T return_size = 0;

    HRESULT hr = m_pfn_execute(m_handle, "ioctl_pio_read", in, 1, out, 1, &return_size);
    if (SUCCEEDED(hr) && return_size >= 1) {
        return static_cast<BYTE>(out[0] & 0xFF);
    }
    return 0xFF;
}

void PawnIOProvider::WritePort(USHORT port, BYTE value) {
    if (!m_handle || !m_pfn_execute) return;

    if (port != 0x62 && port != 0x66) {
        return;
    }

    ULONG64 in[2] = { port, value };
    SIZE_T return_size = 0;

    m_pfn_execute(m_handle, "ioctl_pio_write", in, 2, nullptr, 0, &return_size);
}
