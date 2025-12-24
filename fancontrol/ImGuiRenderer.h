#pragma once

// ImGuiRenderer.h - Refactored UI rendering components
// Extracted from imgui_main.cpp to reduce the size of main() function

#include "imgui.h"
#include "ConfigManager.h"
#include "Theme.h"
#include "I18nManager.h"
#include "CommonTypes.h"
#include <vector>
#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <format>

// Forward declarations
struct UIState;
struct AppLog;

namespace ImGuiUI {

// ============================================================================
// Dashboard Tab Components
// ============================================================================

// Draw a metric card with icon, label, value, and optional sub-value
inline void DrawMetricCard(const char* icon, const char* label, const char* value,
                          const char* subValue, ImVec4 color, float dpiScale) {
    ImGui::BeginChild(label, ImVec2(0, Theme::Layout::CardHeight * dpiScale), true);
    ImGui::TextColored(Theme::TextMuted(), "%s %s", icon, label);
    ImGui::Spacing();
    
    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
    ImGui::TextColored(color, "%s", value);
    ImGui::PopFont();

    if (subValue && subValue[0] != '\0') {
        ImGui::SameLine();
        ImGui::TextColored(Theme::TextDark(), "(%s)", subValue);
    }
    ImGui::EndChild();
}

// Draw a segmented control button (mode selector)
inline bool DrawSegmentedButton(const char* label, int id, int* current, float width, float dpiScale) {
    bool active = (*current == id);
    bool clicked = false;
    
    if (active) {
        ImGui::PushStyleColor(ImGuiCol_Button, Theme::PrimaryTransparent(0.8f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Theme::PrimaryTransparent(0.9f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, Theme::Primary());
    }
    
    if (ImGui::Button(label, ImVec2(width, Theme::Layout::ButtonHeight * dpiScale))) {
        *current = id;
        clicked = true;
    }
    
    if (active) ImGui::PopStyleColor(3);
    ImGui::SameLine();
    
    return clicked;
}

// Draw a sidebar item for settings panel
inline bool DrawSidebarItem(int id, int currentTab, const char* icon, const char* labelKey, float dpiScale) {
    bool selected = (currentTab == id);
    bool clicked = false;
    
    if (selected) ImGui::PushStyleColor(ImGuiCol_Header, Theme::PrimaryTransparent(0.2f));
    if (selected) ImGui::PushStyleColor(ImGuiCol_Text, Theme::Primary());
    
    char buf[128];
    sprintf_s(buf, "%s  %s", icon, _TR(labelKey));
    if (ImGui::Selectable(buf, selected, 0, ImVec2(0, Theme::Layout::LargeButtonHeight * dpiScale))) {
        clicked = true;
    }
    
    if (selected) ImGui::PopStyleColor(2);
    return clicked;
}

// Draw section header with icon
inline void DrawSectionHeader(const char* icon, const char* labelKey) {
    ImGui::TextColored(Theme::Primary(), "%s %s", icon, _TR(labelKey));
    ImGui::Separator();
    ImGui::Spacing();
}

// Draw a subsection header
inline void DrawSubsectionHeader(const char* labelKey) {
    ImGui::TextColored(Theme::TextMuted(), "%s", _TR(labelKey));
    ImGui::Spacing();
}

// ============================================================================
// Log Panel Component
// ============================================================================

inline void DrawLogPanel(const std::deque<std::string>& items) {
    ImGui::BeginChild("LogScroll");
    for (const auto& line : items) {
        if (line.find("[ERROR]") != std::string::npos) {
            ImGui::TextColored(Theme::TempHot(), "%s", line.c_str());
        } else if (line.find("[WARN]") != std::string::npos) {
            ImGui::TextColored(Theme::TempWarm(), "%s", line.c_str());
        } else {
            ImGui::TextUnformatted(line.c_str());
        }
    }
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();
}

// ============================================================================
// Sensor Grid Component
// ============================================================================

inline void DrawSensorCard(const std::string& name, float currentTemp, bool isGpu, 
                          const char* iconCpu, const char* iconGpu, float dpiScale) {
    float progress = currentTemp / 100.0f;
    ImVec4 color = Theme::GetTempColor(currentTemp);
    
    ImGui::BeginChild(name.c_str(), ImVec2(0, 65 * dpiScale), true);
    ImGui::Text("%s %s", (isGpu ? iconGpu : iconCpu), name.c_str());
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 45 * dpiScale);
    ImGui::Text("%.0f\xC2\xB0\x43", currentTemp);
    
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, color);
    ImGui::ProgressBar(progress, ImVec2(-1, 5 * dpiScale), "");
    ImGui::PopStyleColor();
    ImGui::EndChild();
}

// ============================================================================
// Settings Components
// ============================================================================

inline void DrawCheckboxSetting(const char* labelKey, int* value) {
    bool checked = (*value != 0);
    if (ImGui::Checkbox(_TR(labelKey), &checked)) {
        *value = checked ? 1 : 0;
    }
}

inline void DrawIntInputSetting(const char* labelKey, int* value, int minVal, int maxVal, float width, float dpiScale) {
    ImGui::Text("%s", _TR(labelKey));
    ImGui::PushItemWidth(width * dpiScale);
    if (ImGui::InputInt("##IntInput", value)) {
        if (*value < minVal) *value = minVal;
        if (*value > maxVal) *value = maxVal;
    }
    ImGui::PopItemWidth();
}

inline void DrawFloatInputSetting(const char* label, float* value, float step, float stepFast, 
                                  const char* format, float width, float labelWidth, float dpiScale) {
    ImGui::Text("%s:", label); 
    ImGui::SameLine(labelWidth * dpiScale);
    ImGui::PushItemWidth(width * dpiScale);
    ImGui::InputFloat("##FloatInput", value, step, stepFast, format);
    ImGui::PopItemWidth();
}

} // namespace ImGuiUI
