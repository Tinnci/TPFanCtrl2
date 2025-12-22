# TPFanCtrl2 现代化重构技术架构与路线全报告

本报告旨在详细记录 TPFanCtrl2 项目的技术现状、重构目标、架构设计以及详细的实施计划。本文件作为项目重构过程中的“单一事实来源 (Source of Truth)”。

---

## 1. 当前技术基准 (Technical Baseline)

### 1.1 编译环境
*   **编译器**: Microsoft Visual C++ (MSVC) / 工具集 `v143` (Visual Studio 2022)。
*   **SDK**: Windows SDK 10.0.26100.0。
*   **目标架构**: `Win32` (x86)。由于底层驱动 `TVicPort` 的历史限制，目前必须以 32 位模式运行。
*   **字符集**: Multi-Byte Character Set (MBCS)。

### 1.2 核心架构
*   **模式**: 典型的旧式 Win32 GUI 应用程序。
*   **耦合度**: 极高。`FANCONTROL` 类同时承担了 UI 管理、配置解析、硬件 I/O 和控制算法。
*   **并发模型**: 使用 Win32 原生线程和互斥量 (`MUTEXSEM`)。

### 1.3 硬件交互 (I/O)
*   **驱动层**: `TVicPort` (Ring 0 驱动)。
*   **交互方式**: 通过端口 I/O (`0x62/0x66` 或 `0x1600/0x1604`) 直接与嵌入式控制器 (EC) 通信。
*   **协议**: 遵循 ACPI EC 规范（等待 IBF/OBF 标志位）。

---

## 2. 现代化架构设计 (Target Architecture)

为了实现解耦和可测试性，我们将架构划分为以下四个层次：

### 2.1 硬件抽象层 (HAL)
*   **接口**: `IIOProvider`。
*   **职责**: 仅负责最基础的 `ReadPort` 和 `WritePort`。
*   **实现**: 
    *   `TVicPortProvider`: 生产环境使用。
    *   `MockIOProvider`: 测试环境使用，模拟寄存器状态。

### 2.2 核心管理层 (Managers)
*   **ECManager**: 封装 EC 读写协议，处理 Type 1/Type 2 自动切换。
*   **SensorManager**: 负责传感器数据采集、偏移量 (Offset) 计算及最高温逻辑。
*   **FanController**: 负责风扇速度读取及 Smart Mode 阶梯算法（含滞后逻辑）。
*   **ConfigManager**: 负责 `.ini` 文件的解析与配置持久化。
    *   **管理变量**: `Cycle`, `IconCycle`, `FanBeepFreq`, `MaxReadErrors`, `SmartLevels`, `SensorOffsets` 等。
    *   **设计模式**: 单例或通过依赖注入传递，提供强类型的配置访问接口。

### 2.3 业务逻辑层 (Service Logic)
*   **职责**: 协调各 Manager，根据配置和传感器状态决定风扇行为。
*   **目标**: 最终移植到 Go 语言，作为 Windows 服务运行。

### 2.4 表现层 (UI/UX)
*   **职责**: 仅负责数据显示和用户输入。
*   **通信**: 通过 IPC (如 Named Pipes) 与逻辑层通信。

---

## 3. 技术选项分析 (Technical Options)

### 3.1 构建系统
| 选项 | 建议 | 理由 |
| :--- | :--- | :--- |
| **CMake** | **强烈建议** | 跨 IDE 支持，易于集成单元测试和 CI。 |
| **MSBuild** | 维持现状 | 仅适用于纯 Windows/VS 环境。 |

### 3.2 底层驱动
| 选项 | 状态 | 评价 |
| :--- | :--- | :--- |
| **TVicPort** | **当前使用** | 稳定但陈旧，闭源。 |
| **WinRing0** | 备选 | 开源，支持 64 位，但需解决签名问题。 |
| **InpOut32** | 备选 | 社区支持广，适合简单 I/O。 |

### 3.3 进程间通信 (IPC)
| 选项 | 状态 | 评价 |
| :--- | :--- | :--- |
| **Named Pipes** | **首选** | Windows 原生高性能，适合服务与 UI 通信。 |
| **Local HTTP** | 备选 | 易于调试，但有网络栈开销。 |

---

## 4. 实施路线图 (Roadmap)

### 第一阶段：C++ 深度解耦 (进行中 - 65%)
1.  [x] 建立 `IIOProvider` 硬件抽象。
2.  [x] 抽离 `ECManager`, `SensorManager`, `FanController`。
3.  [x] 引入 `MockIOProvider` 进行逻辑验证。
4.  [ ] **下一步**: 抽离 `ConfigManager`。
5.  [ ] 清理 `fancontrol.cpp` 中的全局变量。

### 第二阶段：构建系统现代化
1.  引入 CMake 构建脚本。
2.  集成单元测试框架 (如 Google Test)。

### 第三阶段：Go 语言核心重构
1.  使用 CGO 封装驱动接口。
2.  移植控制逻辑至 Go。
3.  实现 Windows 服务化。

### 第四阶段：UI 焕新
1.  实现基于 IPC 的 UI 协议。
2.  使用现代框架（WinUI 3 / WebView2）重写界面。

---

## 5. 进度估计与风险

### 5.1 时间估计
*   **大胆估计 (Bold)**: **21.5 工作日** (假设一切顺利)。
*   **保守估计 (Conservative)**: **45 工作日** (考虑到硬件 Hack 和驱动签名风险)。

### 5.2 关键风险
1.  **硬件损坏风险**: 错误的 EC 写入可能导致风扇停转或硬件过热。
2.  **驱动加载失败**: Windows 11 内核隔离可能拦截未签名的旧驱动。
3.  **并发死锁**: Go 与底层 C 驱动交互时的锁竞争。

---

## 6. 核心数据结构定义 (Core Data Structures)

为了确保 C++ 与未来 Go 实现的一致性，定义以下核心结构：

### 7.1 传感器状态 (Sensor State)
```cpp
struct SensorEntry {
    std::string name;
    int rawValue;
    int offset;
    int processedValue;
};
```

### 7.2 风扇控制级别 (Smart Levels)
```cpp
struct SmartLevel {
    int temp;      // 触发温度
    int fanLevel;  // 风扇等级 (0-7, 64, 128)
    int hystUp;    // 上行滞后
    int hystDown;  // 下行滞后
};
```

### 7.3 全局配置 (Global Config)
```cpp
struct AppConfig {
    int refreshCycle;      // 数据刷新周期
    bool useFahrenheit;    // 是否使用华氏度
    int activeMode;        // 0: BIOS, 1: Smart, 2: Manual
    // ... 其他从 .ini 加载的 50+ 项配置
};
```

---

## 8. 测试与验证 (Testing & Verification)

为了确保重构过程中的逻辑正确性，我们引入了自动化逻辑测试：

### 8.1 测试架构
*   **测试目标**: `SensorManager`, `FanController`, `ECManager`。
*   **模拟对象**: `MockIOProvider` 模拟硬件端口。
*   **测试用例**: 涵盖温度读取、偏移量计算、传感器忽略逻辑、Smart Mode 滞后逻辑及双风扇控制。

### 8.2 如何运行测试
1.  确保已安装 Visual Studio (2019/2022)。
2.  运行根目录下的 `run_tests.bat`。
3.  该脚本会自动配置 MSVC 环境，编译并执行 `tests/logic_test.cpp`。

---

## 9. 交互定义 (Inter-op Definition)

*   **C++ 内部**: 采用 **构造函数注入 (DI)**。
*   **Go 与 C**: 采用 **CGO**，Go 侧负责高层逻辑，C 侧负责 Ring 0 端口操作。
*   **服务与 UI**: 采用 **JSON over Named Pipes**，实现跨语言、跨进程的异步通信。
