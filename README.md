# TPFanCtrl2

[![CI](https://github.com/Tinnci/TPFanCtrl2/actions/workflows/ci.yml/badge.svg)](https://github.com/Tinnci/TPFanCtrl2/actions/workflows/ci.yml)
[![Release](https://github.com/Tinnci/TPFanCtrl2/actions/workflows/release.yml/badge.svg)](https://github.com/Tinnci/TPFanCtrl2/actions/workflows/release.yml)
[![Version](https://img.shields.io/github/v/release/Tinnci/TPFanCtrl2)](https://github.com/Tinnci/TPFanCtrl2/releases)
[![License](https://img.shields.io/badge/license-Unlicense-blue.svg)](LICENSE)

TPFanCtrl2 is a fan control software utility for Lenovo ThinkPad laptops on Windows 10 and Windows 11. It provides a graphical user interface (GUI) and a command-line interface (CLI) to monitor hardware temperatures and control fan speeds.

## Features

- **Graphical User Interface (`TPFanCtrl2.exe`)**: Built with Dear ImGui and Vulkan for real-time monitoring and configuration.
- **Interactive Terminal Dashboard (`TPFanCtrl2-top.exe`)**: A modern, full-screen TUI (like btop/htop) with ANSI TrueColor rendering, real-time tachometer trend sparklines, multi-sensor temperature gauges, and instant keyboard controls.
- **Command-Line Interface (`TPFanCtrl2-cli.exe`)**: Enables script-friendly terminal control, status queries, and PowerShell/JSON automation.
- **Hardware Access**: Uses the WHQL-compliant, signed [PawnIO](https://github.com/namazso/PawnIO) kernel driver. Fully compatible with Windows 11 Memory Integrity (HVCI / Core Isolation).
- **Control Modes**:
  - **BIOS / Firmware Mode**: Returns fan control to the system Embedded Controller (EC).
  - **Manual Mode**: Sets a fixed fan level from 0 (off) to 7 (maximum speed).
  - **Smart Curve Mode**: Automatically adjusts fan speed according to a temperature threshold table.
  - **Dual-Fan Support**: Monitors and controls primary and secondary fans independently.
- **Safety Protection**: Restores automatic hardware control if temperature exceeds 90 °C or if a communication error occurs.

> [!NOTE]
> **Pure 64-bit Modern Architecture:**  
> Starting from version 2.8.0, TPFanCtrl2 is built as a pure native 64-bit application powered exclusively by the secure PawnIO driver. Legacy 32-bit systems requiring the retired TVicPort driver can continue using the [v2.7.1 Final Legacy Release](https://github.com/Tinnci/TPFanCtrl2/releases/tag/v2.7.1).

## System Requirements

- **Operating System**: Windows 10 (64-bit) or Windows 11 (64-bit).
- **Supported Hardware**: Lenovo ThinkPad laptops with a standard ACPI Embedded Controller.
- **Privileges**: Administrator permissions are required to access hardware ports.
- **Driver**: [PawnIO](https://github.com/namazso/PawnIO) driver (`winget install namazso.PawnIO`).

## Installation

### Method 1: Install with Windows Package Manager (WinGet)

Install the PawnIO driver and TPFanCtrl2:

```powershell
# Install the required kernel driver
winget install namazso.PawnIO

# Install TPFanCtrl2
winget install Tinnci.TPFanCtrl2
```

### Method 2: Manual Download

1. Download the latest release package (`TPFanCtrl2-v*-windows-x64-app.zip`) from the [Latest Release](https://github.com/Tinnci/TPFanCtrl2/releases/latest) page.
2. Extract the archive files to a folder of your choice.
3. Install the `PawnIO` kernel driver (`winget install namazso.PawnIO`).

## Usage

### Graphical User Interface (GUI)

1. Right-click `TPFanCtrl2.exe` and select **Run as administrator**.
2. The application opens the dashboard and displays temperature sensor values and fan speed.
3. Select a control mode:
   - **Smart**: Follows the temperature thresholds defined in `TPFanCtrl2.ini`.
   - **Manual**: Select a fan speed level from 0 to 7.
   - **BIOS**: Lets the system firmware control the fan.
4. Close the main window to minimize the application to the Windows notification area (system tray).

### Command-Line Interface (CLI)

Run `TPFanCtrl2-cli.exe` in an elevated terminal (Administrator):

```powershell
# View hardware status (fan speed and current EC level)
.\TPFanCtrl2-cli.exe status

# Output status in JSON format
.\TPFanCtrl2-cli.exe status --json

# Set fan speed to level 7
.\TPFanCtrl2-cli.exe fan --level 7

# Set fan speed to level 3 for 60 seconds, then return to automatic control
.\TPFanCtrl2-cli.exe fan --level 3 --duration 60

# Return fan control to the system EC firmware
.\TPFanCtrl2-cli.exe mode auto

# View all available CLI options
.\TPFanCtrl2-cli.exe --help
```

### Interactive Terminal Dashboard (TUI)

Launch the full-screen real-time hardware monitor (btop-style):

```powershell
.\TPFanCtrl2-top.exe
```

- **Interactive Hotkeys**:
  - `[0-7]`: Instantly set manual fan speed levels.
  - `[A]`: Return to EC firmware automatic control curve.
  - `[D]`: Toggle dual-fan / single-fan control mode.
  - `[R]`: Force an immediate hardware refresh.
  - `[Q]` or `[Esc]`: Safely restore EC automatic control and cleanly exit.

## Configuration

Edit `TPFanCtrl2.ini` in the program folder to customize settings:

- **`Active`**: Set initial operating mode (`0` for BIOS, `1` for Smart curve, `2` for Manual).
- **`ManMode`**: Default manual fan level (0 to 7).
- **`Cycle`**: Sensor sampling period in seconds.
- **`Level` entries**: Temperature threshold points for the fan curve. Example:
  ```ini
  Level=60 0
  Level=65 1
  Level=75 3
  Level=80 7
  ```
- **`IconColorFan`**: Display fan speed or highest temperature in the taskbar icon.

## Build from Source

This project uses [xmake](https://xmake.io/) and Microsoft Visual Studio 2022.

### Prerequisites

- Microsoft Visual Studio 2022 (Desktop development with C++)
- [xmake](https://xmake.io/) (version 2.8 or later)

### Build Commands

```powershell
# Clone the repository
git clone https://github.com/Tinnci/TPFanCtrl2.git
cd TPFanCtrl2

# Configure and build all targets (native 64-bit)
xmake f -m release -a x64 -y
xmake

# Run unit tests
xmake run logic_test
xmake run core_test
```

### Create Release Package

```powershell
# Package 64-bit release (version auto-detected from current Git tag)
powershell -ExecutionPolicy Bypass -File scripts/package-release.ps1 -Architecture x64
```

## Safety Disclaimer

Use this software at your own risk. Setting fan speeds too low can cause hardware overheating or thermal throttling. The authors assume no liability for hardware damage.

## License

This project is dedicated to the public domain under the [Unlicense](LICENSE).
