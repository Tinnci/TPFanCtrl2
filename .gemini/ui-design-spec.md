# TPFanCtrl2 UI 设计规范

## 1. 滚动策略 (Scroll Policy)

### ❌ 避免不必要的滚动
- **固定内容区域**不应出现滚动条
- 侧边栏、控制面板等固定布局组件应完全可见
- 如果内容放不下，考虑重新布局而非添加滚动

### ✅ 需要滚动的场景
- 日志面板 (动态增长内容)
- 传感器历史图表 (可能有多个传感器)
- 大量配置选项 (如传感器详细设置表格)

### ImGui 实现
```cpp
// ❌ 错误：固定内容可能产生滚动
ImGui::BeginChild("Panel", ImVec2(0, 0), true);

// ✅ 正确：禁用滚动
ImGui::BeginChild("Panel", ImVec2(0, 0), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);We should we want 
```

## 2. 布局尺寸规范

### 控制面板 (Dashboard > Control Panel)
- 最小高度：200px (DPI缩放后)
- 包含：模式选择(3按钮)、算法/模式设置、预设、最小化按钮
- **不应滚动**

### 设置侧边栏 (Settings > Sidebar)
- 宽度：180px (DPI缩放后)
- 包含：3个菜单项 + 底部保存按钮
- **不应滚动**

### 内容区域高度分配
- 卡片高度：90px
- 按钮高度：35px / 40px
- 侧边栏项高度：40px

## 3. 组件间距规范

### 标准间距
- 组件间：ImGui::Spacing() (自动)
- 区域分隔：ImGui::Separator()
- 大间距：ImGui::NewLine()

### 边距
- 窗口内边距：15px × 15px
- 项目间距：12px × 10px

## 4. 禁用滚动的组件清单

| 组件名称 | 位置 | 滚动策略 |
|---------|------|---------|
| ControlPanel | Dashboard右上 | 禁用 |
| SettingsSidebar | Settings左侧 | 禁用 |
| MetricCards | Dashboard顶部 | 禁用 |
| SensorsArea | Dashboard左侧 | 允许(内部SensorsScroll) |
| LogsPanel | Dashboard右下 | 允许(内部LogScroll) |
| SettingsContent | Settings右侧 | 允许(PID/Sensors内容多) |

## 5. 响应式设计原则

- 使用相对尺寸而非固定像素
- 考虑 DPI 缩放：所有尺寸乘以 dpiScale
- 最小窗口尺寸：保证所有核心内容可见
