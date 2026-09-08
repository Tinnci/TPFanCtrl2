#pragma once
#include <string>
#include <string_view>

#ifndef TPFC_VERSION
#define TPFC_VERSION "2.8.0"
#endif

#ifndef TPFC_COMMIT
#define TPFC_COMMIT "dev"
#endif

#ifndef TPFC_BUILD_DATE
#define TPFC_BUILD_DATE "2026-09-08"
#endif

namespace AppVersion {
inline constexpr std::string_view Version = TPFC_VERSION;
inline constexpr std::string_view Commit = TPFC_COMMIT;
inline constexpr std::string_view BuildDate = TPFC_BUILD_DATE;

inline std::string GetFullVersionString() {
    return std::string("v") + std::string(Version) + " (" + std::string(Commit) + ", " + std::string(BuildDate) + ")";
}
}