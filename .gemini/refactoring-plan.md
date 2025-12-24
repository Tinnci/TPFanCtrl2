# TPFanCtrl2 重构计划

## 概述

本文档描述了 TPFanCtrl2 项目的架构重构计划，目标是将核心业务逻辑从 Win32 UI 中解耦，最终完全迁移到 ImGui 界面。

**开始日期**: 2025-12-24
**目标**: 建立清晰的 Core 库边界，使用现代 C++20 模式替代遗留的 Win32 消息机制

---

## Phase 1: 止血 + 建立 Core 库边界 ✅ 进行中

### 目标
- 创建 `Core/` 目录，定义清晰的边界接口
- 停止向 `fancontrol.cpp` 添加新代码
- 建立类型安全的事件系统替代 `PostMessage`

### 已完成

| 文件 | 状态 | 说明 |
|------|------|------|
| `Core/Events.h` | ✅ 已创建 | 事件类型定义，替代 WM_USER 消息 |
| `Core/IThermalObserver.h` | ✅ 已创建 | 观察者接口 + EventDispatcher |
| `Core/SensorConfig.h` | ✅ 已创建 | 传感器配置结构，数据驱动替代硬编码 |
| `Core/ThermalManager.h` | ✅ 已创建 | 核心编排类头文件 |
| `Core/ThermalManager.cpp` | ✅ 已创建 | 核心编排类实现，含 jthread 工作循环 |
| `SensorManager.h` | ✅ 已更新 | 添加 `GetSensor()` 方法 |
| `FanController.h` | ✅ 已更新 | 添加 `GetCurrentLevel()` 别名 |

### 待完成

- [ ] 更新 `xmake.lua` 添加 Core 目录编译
- [ ] 修改 `imgui_main.cpp` 使用 `ThermalManager`
- [ ] 编写单元测试验证 Core 库
- [ ] 验证编译通过

---

## Phase 2: 独立控制线程

### 目标
- 用 `std::jthread` 替代 `WM_TIMER`
- UI 仅被动获取状态，不再驱动控制循环

### 关键变更

```cpp
// 旧模式 (fancontrol.cpp)
m_fanTimer = ::SetTimer(hwndDialog, 1, Cycle * 1000, NULL);
// 在 WM_TIMER 中调用 HandleData()

// 新模式 (ThermalManager)
m_workerThread = std::jthread([this](std::stop_token token) {
    WorkerLoop(token);
});
```

### 待完成

- [ ] 在 `imgui_main.cpp` 中替换 `HardwareWorker` 为 `ThermalManager`
- [ ] 移除 Win32 Dialog 中的 `SetTimer` 相关代码
- [ ] 验证 UI 线程不再卡顿时风扇控制仍正常工作

---

## Phase 3: 消除 DlgProc 迷宫 (可选，因为最终会废弃)

由于计划完全迁移到 ImGui，此阶段可跳过或简化。仅在需要并行维护 Win32 UI 时执行。

---

## Phase 4: 现代化清理

### 目标
- 消除代码异味
- 统一代码风格

### 待完成

- [ ] 魔术数字 → 枚举 (ControlID, MenuID)
- [ ] C 风格字符串 → `std::string` + `std::format`
- [ ] 全局变量 → 类成员或局部变量
- [ ] 传感器配置 → 数据驱动 (从 INI/JSON 加载)

---

## 架构图

```
┌─────────────────────────────────────────────────────────────────┐
│                         UI Layer                                 │
│  ┌──────────────────┐    ┌──────────────────────────────────┐   │
│  │ fancontrol.cpp   │    │        imgui_main.cpp            │   │
│  │ (Legacy Win32)   │    │        (Modern ImGui)            │   │
│  │ [DEPRECATED]     │    │        [Active Development]      │   │
│  └────────┬─────────┘    └────────────────┬─────────────────┘   │
│           │                               │                      │
│           │ Direct calls                  │ Subscribe + GetState │
│           │ (gradually remove)            │                      │
└───────────┼───────────────────────────────┼──────────────────────┘
            │                               │
            ▼                               ▼
┌─────────────────────────────────────────────────────────────────┐
│                      Core Layer (NEW)                            │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │                   ThermalManager                          │   │
│  │  - Owns std::jthread worker loop                         │   │
│  │  - Coordinates SensorManager + FanController             │   │
│  │  - Dispatches events via EventDispatcher                 │   │
│  │  - NO HWND, NO WM_*, NO Win32 dependencies               │   │
│  └──────────────────────────────────────────────────────────┘   │
│                              │                                   │
│         ┌────────────────────┼────────────────────┐              │
│         ▼                    ▼                    ▼              │
│  ┌─────────────┐    ┌─────────────────┐    ┌────────────┐       │
│  │SensorManager│    │  FanController  │    │ ECManager  │       │
│  └─────────────┘    └─────────────────┘    └────────────┘       │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                    Hardware Abstraction                          │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │                   IIOProvider                             │   │
│  │  ┌─────────────────┐         ┌───────────────────┐       │   │
│  │  │ TVicPortProvider│         │  MockIOProvider   │       │   │
│  │  │ (Production)    │         │  (Testing)        │       │   │
│  │  └─────────────────┘         └───────────────────┘       │   │
│  └──────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
```

---

## 文件结构

```
fancontrol/
├── Core/                          # 新建：纯业务逻辑库
│   ├── Events.h                   # 事件定义
│   ├── IThermalObserver.h         # 观察者接口
│   ├── SensorConfig.h             # 传感器配置结构
│   ├── ThermalManager.h           # 核心编排类
│   └── ThermalManager.cpp         # 核心编排实现
│
├── ECManager.h/cpp                # 已存在：EC 硬件访问
├── SensorManager.h/cpp            # 已存在：传感器管理
├── FanController.h/cpp            # 已存在：风扇控制
├── ConfigManager.h/cpp            # 已存在：配置管理
│
├── imgui_main.cpp                 # 主入口：ImGui UI (活跃开发)
├── fancontrol.cpp                 # 遗留：Win32 Dialog (逐步废弃)
└── ...
```

---

## 迁移策略

### ImGui UI 集成 ThermalManager

```cpp
// imgui_main.cpp 中的变更

// 1. 创建 ThermalManager 替代 HardwareWorker
auto thermalManager = std::make_shared<Core::ThermalManager>(
    ecManager, thermalConfig);

// 2. 订阅事件用于 UI 更新
thermalManager->Subscribe([&uiState](const Core::ThermalEvent& event) {
    std::visit([&uiState](const auto& e) {
        if constexpr (std::is_same_v<std::decay_t<decltype(e)>, 
                      Core::TemperatureUpdateEvent>) {
            std::lock_guard lock(uiState.Mutex);
            // 更新 UI 状态
            for (const auto& sensor : e.sensors) {
                uiState.Sensors[sensor.index] = /* convert */;
            }
        }
    }, event);
});

// 3. 启动
thermalManager->Start();

// 4. 主循环中只需渲染，不再驱动控制逻辑
while (running) {
    // ImGui 渲染
    auto state = thermalManager->GetState();
    RenderUI(state);
}

// 5. 退出
thermalManager->Stop();
```

---

## 测试策略

1. **单元测试**: 使用 `MockIOProvider` 测试 `ThermalManager` 逻辑
2. **集成测试**: 验证 `ThermalManager` 与真实 EC 通信
3. **UI 测试**: 验证 ImGui 界面正确显示状态

---

## 风险与缓解

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| EC 通信稳定性 | 高 | 保留现有 `ECManager` 逻辑不变 |
| 线程安全问题 | 中 | 使用 `std::mutex` 保护共享状态 |
| 向后兼容性 | 低 | 保留 `fancontrol.cpp` 直到完全迁移 |

---

## 下一步行动

1. ✅ 创建 Core 库结构
2. ⏳ 更新 `xmake.lua` 编译配置
3. ⏳ 修改 `imgui_main.cpp` 集成 `ThermalManager`
4. ⏳ 验证编译和基本功能
5. ⏳ 编写单元测试
