# Legacy TVicPort Driver Toolkit

## Overview

This directory contains the legacy **TVicPort** driver files and setup helpers for backward compatibility with 32-bit (x86) legacy ThinkPad hardware environments.

> [!IMPORTANT]
> **Final Legacy Release Notice:**  
> This is the final release of TPFanCtrl2 that includes and bundles the legacy TVicPort driver. Future releases will completely retire TVicPort in favor of the modern, WHQL-compliant, and secure **PawnIO** driver.

## Included Files

- `TVicPort.dll`: 32-bit user-mode interface dynamic link library.
- `TVicPort.sys`: 32-bit kernel-mode driver for legacy 32-bit Windows.
- `TVicPort64.sys`: 64-bit kernel-mode driver for older 64-bit Windows.
- `install-driver.cmd`: Helper script to register and start the TVicPort driver service.
- `uninstall-driver.cmd`: Helper script to stop and remove the TVicPort driver service.

## Installation

1. Right-click `install-driver.cmd` and select **Run as administrator**.
2. The script will detect your system architecture, copy the driver to `%SystemRoot%\system32\drivers\`, register the service, and start it.
3. Launch `TPFanCtrl2.exe` or `TPFanCtrl2-cli.exe`.

## Security & Compatibility Warning

- **Windows 11 / Modern Windows 10**: If **Memory Integrity (HVCI / Core Isolation)** is enabled, Windows Defender will block `TVicPort.sys` / `TVicPort64.sys` from loading due to driver blocklist policies.
- For all modern systems, **[PawnIO](https://github.com/namazso/PawnIO)** is the recommended, secure, and supported hardware driver.