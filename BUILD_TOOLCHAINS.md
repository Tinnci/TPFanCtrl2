# Build System Notes

## Toolchain Support

The `xmake.lua` has been configured to support multiple C++ toolchains:

### ✅ MSVC (Primary - Recommended)
```bash
xmake f --toolchain=msvc -a x86 -m release
xmake
```
- **Status**: Fully working
- **Features**: Full optimization with `/GL` and `/LTCG`, all tests pass
- **Use for**: Production builds, releases

### ⚠️ Clang/Zig (Experimental)
```bash
xmake f --toolchain=zig -a x86 -m release
```
- **Status**: Configuration supported, but package linking issues
- **Issue**: Zig's LLD linker (v0.15.2) has compatibility issues with some CMake-built packages on Windows
- **Blocker**: Dependencies (zlib, gtest, vulkan-loader) fail to link with exit code 1112
- **Future**: May work when Zig improves Windows MSVC ABI compatibility

### Flag Mapping

| Feature | MSVC | Clang/Zig |
|---------|------|-----------|
| Unsigned char | `/J` | `-funsigned-char` |
| UTF-8 encoding | `/utf-8` | `-finput-charset=UTF-8` |
| Warnings | `/W4` | `-Wall -Wextra` |
| Link-time optimization | `/GL` + `/LTCG` | `-flto` |
| Subsystem (GUI) | `/SUBSYSTEM:WINDOWS` | `-Wl,/SUBSYSTEM:WINDOWS` |

## Why Multi-Toolchain?

Even though Zig doesn't work yet, having multi-toolchain support future-proofs the project:
- Easier to test with Clang for better error messages
- Portable flag management
- Ready for when Zig/Clang-on-Windows matures

## Current Recommendation

**Use MSVC** until Zig/LLVM improve Windows support for C++ projects using CMake dependencies.
