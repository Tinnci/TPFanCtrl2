# TPFanCtrl2 <img src="fancontrol/res/app.ico" alt="App Icon" width="32"/>

[![Build and Release](https://github.com/Tinnci/TPFanCtrl2/actions/workflows/release.yml/badge.svg)](https://github.com/Tinnci/TPFanCtrl2/actions/workflows/release.yml)
[![Release](https://img.shields.io/github/v/release/Tinnci/TPFanCtrl2)](https://github.com/Tinnci/TPFanCtrl2/releases)
[![License](https://img.shields.io/badge/license-Unlicense-blue.svg)](http://unlicense.org/)

**TPFanCtrl2** is a modernized, high-performance fan control utility for ThinkPad laptops on Windows 10/11. This version is a complete overhaul of the original TPFC, featuring a modern GUI, advanced control algorithms, and full DPI awareness.

## 📸 Screenshots

<p align="center">
  <img src="assets/eng.png" alt="English Dashboard" width="30%">
  <img src="assets/chn.png" alt="Chinese Dashboard" width="30%">
  <img src="assets/eng2.png" alt="English Settings" width="30%">
</p>

---

## ✨ Key Features

- **Modern UI**: Built with **ImGui** and **Vulkan 1.2** for a smooth, hardware-accelerated experience.
- **Advanced Control**: 
  - **PID Control**: 10Hz closed-loop PID algorithm for precise temperature management.
  - **Smart Mode**: Classic step-based fan curve with hysteresis support.
  - **Dual Fan Support**: Independent or synchronized control for high-end ThinkPad models (P-series, etc.).
- **DPI Aware**: Fully responsive UI and dynamic tray icons that scale perfectly on 4K+ displays.
- **Internationalization (I18n)**: Built-in support for multiple languages (English, Chinese, etc.).
- **Dynamic Tray Icon**: Real-time temperature and fan speed rendering directly in the system tray using Segoe UI.
- **Lightweight**: No external dependencies required at runtime; minimal CPU and memory footprint.

---

## 🚀 Quick Start

### Download
Get the latest pre-compiled binaries from the [Releases](https://github.com/Tinnci/TPFanCtrl2/releases) page.

### Installation
1. Extract the contents to a folder of your choice.
2. **Requirement**: Ensure you have the `TVicPort` driver installed (usually comes with original TPFanControl or can be installed manually).
3. Run `TPFanCtrl2.exe` as **Administrator**.

### Configuration
Edit `TPFanCtrl2.ini` in the application directory to customize:
- Temperature thresholds and fan levels.
- PID parameters (`Kp`, `Ki`, `Kd`).
- Sensor offsets and ignore lists.
- UI language and theme settings.

---

## 🛠 Build Instructions

This project uses [xmake](https://xmake.io/) for a modern build experience.

### Prerequisites
- [xmake](https://xmake.io/)
- Visual Studio 2022 (C++ Desktop Development workload)

### Build
```bash
# Clone the repository
git clone https://github.com/Tinnci/TPFanCtrl2.git
cd TPFanCtrl2

# Configure and build (Release x86)
xmake f -m release -a x86
xmake

# Binaries will be in the bin/ directory
```

### Run Tests
```bash
xmake run logic_test
```

---

## 🤝 Contributing

Contributions are welcome! Please feel free to submit Pull Requests.
By contributing, you agree to dedicate your changes to the **Public Domain** under the [Unlicense](http://unlicense.org/).

---

## 📜 License

This project is released into the **Public Domain** under the [Unlicense](LICENSE). You are free to use, modify, and distribute it without any restrictions.

---
*Disclaimer: Use this software at your own risk. Incorrect fan settings can lead to hardware damage.*
