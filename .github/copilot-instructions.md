# TPFanCtrl2 Development Guide

## Build System

**Build Tool**: xmake (Lua-based build system)

**Toolchain Support**: Multi-toolchain configuration (MSVC primary, Clang/Zig experimental)

```bash
# Configure for x86 release build (required - TVicPort is 32-bit only)
xmake f -m release -a x86          # Uses default toolchain (MSVC)
xmake f --toolchain=msvc -a x86    # Explicit MSVC
xmake f --toolchain=zig -a x86     # Zig (experimental, see BUILD_TOOLCHAINS.md)

# Build all targets
xmake

# Build specific target
xmake build TPFanCtrl2
xmake build logic_test
xmake build core_test

# Output directory: bin/
```

See [BUILD_TOOLCHAINS.md](../BUILD_TOOLCHAINS.md) for details on Clang/Zig support status.

## Testing

```bash
# Run all tests
xmake run logic_test
xmake run core_test

# Tests use Google Test framework
# Test files: tests/logic_test.cpp, tests/core_test.cpp
# Use MockIOProvider for hardware abstraction in tests
```

## Architecture

### Core Library Pattern

The project follows a **Core Library** architecture to decouple business logic from UI:

```
fancontrol/Core/           # Pure business logic (NO Win32 dependencies)
├── ThermalManager         # Central orchestrator, owns std::jthread control loop
├── UIAdapter              # Bridges Core to ImGui, handles smoothing & history
├── Events.h               # Type-safe event definitions (replaces WM_USER messages)
├── IThermalObserver.h     # Observer pattern for UI updates
└── SensorConfig.h         # Data-driven sensor configuration

fancontrol/                # Hardware & UI integration
├── AppInit.h              # Platform initialization (DPI, logging, Win11 effects)
├── ImGuiRenderer.h        # ImGui rendering helpers (plots, charts)
├── ECManager              # Embedded Controller I/O (via IIOProvider abstraction)
├── SensorManager          # Temperature sensor reading with 5-sample moving average
├── FanController          # Fan control (Smart/PID/Manual modes)
├── ConfigManager          # INI/JSON config parsing
└── imgui_main.cpp         # ImGui UI entry point (active development)
```

**Key Principle**: Core library classes (`ThermalManager`, `UIAdapter`) must NEVER depend on:
- `HWND`, `HINSTANCE`, `WM_*` messages
- `PostMessage`, `SendMessage`, `SetTimer`
- Any Win32 UI primitives

### Control Flow

1. **ThermalManager** runs a background `std::jthread` with 10Hz control loop
2. Uses **EventDispatcher** to broadcast events (TemperatureUpdate, FanLevelChange, etc.)
3. **UIAdapter** subscribes to events and maintains smoothed state for rendering
4. UI thread calls `UIAdapter::GetSnapshot()` for thread-safe state access
5. UI sends commands via `ThermalManager::SetMode()`, `SetManualLevel()`, etc.

### Hardware Abstraction

- **IIOProvider** interface abstracts EC port I/O
- **TVicPortProvider**: Production (kernel-mode driver for Win32 port access)
- **MockIOProvider**: Testing (in-memory register simulation)

## Key Conventions

### Helper Modules

- **AppInit.h**: Platform initialization utilities (header-only)
  - `EnableDPIAwareness()`: Enable per-monitor DPI awareness
  - `InitLogging()`: Setup spdlog with console/file/MSVC sinks
  - `IsRunningAsAdmin()`: Check and log admin privileges
  - `GetDpiScale(HWND)`: Get DPI scale factor for a window
  - `ApplyWindows11Effect(HWND)`: Apply dark mode, rounded corners, mica backdrop

- **ImGuiRenderer.h**: ImGui rendering helpers (header-only)
  - `DrawSimplePlot()`: Temperature history line chart with grid
  - `DrawPIDRadarChart()`: Radar visualization for PID parameters

### Threading Model

- **Control loop**: Independent `std::jthread` in `ThermalManager` (10Hz cycle)
- **UI thread**: ImGui rendering at 60fps, polls state via `GetSnapshot()`
- **Synchronization**: `std::mutex` for shared state, atomic for simple flags
- **Pattern**: UI never blocks control thread; control thread never calls UI

### Sensor Fusion Strategy

- **Weighted Max**: Each sensor has a weight (default 1.0). Effective temp = `max(temp[i] * weight[i])`
- **Moving Average**: 5-sample window to smooth sensor noise
- **Safety Fallback**: If temp > 90°C or EC read errors exceed threshold, release to BIOS control

### Fan Control Modes

- **BIOS**: Release EC control (write 0x80 to offset 0x2F)
- **Smart**: Step-based curve with hysteresis (`SmartLevel` array from config)
- **Manual**: Fixed level 0-7
- **PID**: 10Hz closed-loop PID controller (`PID_Kp`, `PID_Ki`, `PID_Kd` from config)

### EC Communication

- **Ports**: 0x1600/0x1604 (Type1) or 0x62/0x66 (Type2), auto-detected
- **Thread-safe**: `ECManager` uses `std::recursive_timed_mutex`
- **Retry logic**: `WaitForFlags()` with 2s timeout for IBF/OBF flags
- **Critical offsets**:
  - `0x2F`: Fan control register
  - `0x78-0x7F`: Temperature sensors (CPU/GPU/etc.)
  - `0xC0-0xCF`: Extended temp sensors
  - `0x84`: Fan speed tachometer

### UI Scrolling Policy

(See `.gemini/ui-design-spec.md`)

- **Disable scrolling** for fixed panels: Control Panel, Settings Sidebar, Metric Cards
- **Enable scrolling** for dynamic content: Logs, Sensor List, Settings Content
- Use `Theme::WindowFlags::NoScroll` vs `Theme::WindowFlags::Scrollable`

### DPI Awareness

- All layout constants in `Theme::Layout` are base values (96 DPI)
- Multiply by `dpiScale` before use in ImGui calls
- Theme applies DPI scaling via `style.ScaleAllSizes(dpiScale)`

### Config Management

- **File**: `TPFanCtrl2.ini` (INI format with JSON serialization via nlohmann_json)
- **Structure**: `ConfigManager` class with `NLOHMANN_DEFINE_TYPE_INTRUSIVE` macros
- **Loading**: `ConfigManager::LoadConfig()` at startup
- **Saving**: `ConfigManager::SaveConfig()` via Settings > Save button

## C++20 Features in Use

- `std::jthread` with `std::stop_token` for clean thread lifecycle
- `std::format` for string formatting (where MSVC supports it)
- Concepts and constraints (limited use due to MSVC compatibility)
- Designated initializers for structs

## Dependencies

Managed via xmake's `add_requires`:

- **imgui** (master branch): UI framework with Win32 + Vulkan backend
- **vulkan-loader**, **vulkan-memory-allocator**: Graphics rendering
- **freetype**: Font rendering
- **spdlog**: High-performance logging
- **nlohmann_json**: JSON serialization
- **gtest**: Unit testing

## Constraints

**x86 (32-bit) ONLY**: The TVicPort driver is 32-bit. Do NOT attempt x64 builds.

**MSVC Required**: Uses `/J`, `/utf-8`, `/W4` flags. MinGW/Clang untested.

**Administrator Rights**: EC port access requires elevated privileges.

## Naming Conventions

- **Classes**: PascalCase (`ThermalManager`, `ECManager`)
- **Methods**: PascalCase (`GetState()`, `UpdateSensors()`)
- **Private members**: `m_` prefix (`m_sensorManager`, `m_config`)
- **Constants**: SCREAMING_SNAKE_CASE or constexpr PascalCase
- **Namespaces**: PascalCase (`Core`, `Theme`, `App`)

## Common Patterns

### Event Subscription

```cpp
auto subId = thermalManager->Subscribe([](const Core::ThermalEvent& event) {
    std::visit([](const auto& e) {
        if constexpr (std::is_same_v<std::decay_t<decltype(e)>, 
                      Core::TemperatureUpdateEvent>) {
            // Handle temperature update
        }
    }, event);
});
```

### Thread-Safe State Access

```cpp
// From UI thread
auto snapshot = uiAdapter->GetSnapshot();
ImGui::Text("CPU: %d°C", snapshot.maxTemp);

// From control thread
std::lock_guard lock(m_stateMutex);
m_state.maxTemp = newTemp;
```

### Adding a New Sensor

1. Add offset to `CommonTypes.h` (`TP_ECOFFSET_TEMP*`)
2. Update `SensorManager::UpdateSensors()` to read from EC
3. Add config entry in `ConfigManager` for name/weight/offset
4. Update `CreateDefaultSensorConfig()` in `Core/SensorConfig.h`

## Files to Check Before Major Changes

- `.gemini/refactoring-plan.md`: Current modernization status and migration plan
- `.gemini/ui-design-spec.md`: UI layout and scrolling policies
- `fancontrol/Core/ThermalManager.h`: Core orchestration logic
- `xmake.lua`: Build configuration and dependencies
