#pragma once

#include "imgui.h"
#include "Theme.h"
#include "I18nManager.h"
#include <map>
#include <deque>
#include <string>
#include <format>

// Translation helper macro (defined in imgui_main.cpp)
#define _TR(key) I18nManager::Get().Translate(key)

namespace ImGuiRenderer {

/// Draw a simple line plot for temperature history
/// @param label Plot identifier
/// @param history Map of sensor name -> temperature history deque
/// @param height Optional fixed height, 0 = auto
/// @param dpiScale DPI scaling factor
inline void DrawSimplePlot(const char* label, const std::map<std::string, std::deque<float>>& history, float height, float dpiScale) {
    ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();
    ImVec2 canvas_sz = ImGui::GetContentRegionAvail();
    if (canvas_sz.x < 50.0f) canvas_sz.x = 50.0f;
    if (height > 0) canvas_sz.y = height;
    ImVec2 canvas_p1 = ImVec2(canvas_p0.x + canvas_sz.x, canvas_p0.y + canvas_sz.y);

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(canvas_p0, canvas_p1, IM_COL32(30, 30, 30, 255));
    draw_list->AddRect(canvas_p0, canvas_p1, IM_COL32(100, 100, 100, 255));

    // Grid lines at 30, 50, 70, 90 degrees
    float temps[] = { 30.0f, 50.0f, 70.0f, 90.0f };
    for (float t : temps) {
        float y = canvas_p1.y - (t / 100.0f) * canvas_sz.y;
        draw_list->AddLine(ImVec2(canvas_p0.x, y), ImVec2(canvas_p1.x, y), IM_COL32(60, 60, 60, 255));
        char buf[16]; 
        snprintf(buf, 16, "%d", (int)t);
        draw_list->AddText(ImVec2(canvas_p0.x + 5 * dpiScale, y - 15 * dpiScale), IM_COL32(150, 150, 150, 255), buf);
    }

    // Plot lines for each sensor
    int colorIdx = 0;
    ImU32 colors[] = {
        IM_COL32(0, 255, 255, 255),   // Cyan
        IM_COL32(255, 165, 0, 255),   // Orange
        IM_COL32(124, 252, 0, 255),   // LawnGreen
        IM_COL32(255, 105, 180, 255), // HotPink
        IM_COL32(173, 216, 230, 255)  // LightBlue
    };

    float legendX = canvas_p0.x + 40 * dpiScale;
    for (auto const& [name, data] : history) {
        // Note: Ignore check should be done by caller before passing history
        if (data.size() < 2) continue;

        ImU32 color = colors[colorIdx % 5];
        float stepX = canvas_sz.x / 300.0f;
        
        for (size_t i = 0; i < data.size() - 1; i++) {
            ImVec2 p1 = ImVec2(canvas_p1.x - (data.size() - i) * stepX, 
                               canvas_p1.y - (data[i] / 100.0f) * canvas_sz.y);
            ImVec2 p2 = ImVec2(canvas_p1.x - (data.size() - (i + 1)) * stepX, 
                               canvas_p1.y - (data[i+1] / 100.0f) * canvas_sz.y);
            
            if (p1.x < canvas_p0.x) continue;
            draw_list->AddLine(p1, p2, color, 2.0f * dpiScale);
        }
        
        // Legend
        draw_list->AddText(ImVec2(legendX, canvas_p0.y + 5 * dpiScale), color, name.c_str());
        legendX += ImGui::CalcTextSize(name.c_str()).x + 15 * dpiScale;
        colorIdx++;
    }

    ImGui::Dummy(canvas_sz);
}

/// Draw a radar chart for PID parameters visualization
/// @param pid PID settings to visualize
/// @param dpiScale DPI scaling factor
inline void DrawPIDRadarChart(const struct PIDSettings& pid, float dpiScale) {
    ImVec2 size = ImVec2(200 * dpiScale, 200 * dpiScale);
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    ImVec2 center = ImVec2(pos.x + size.x / 2, pos.y + size.y / 2);
    float radius = (size.x / 2) * 0.7f;

    // Background circles
    for (int i = 1; i <= 4; i++) {
        draw_list->AddCircle(center, radius * i / 4.0f, IM_COL32(100, 100, 100, 80), 32);
    }

    // Axes and values
    const char* labels[] = { "Kp", "Ki", "Kd" };
    float maxValues[] = { 2.0f, 0.1f, 1.0f };  // Reasonable max for visualization
    float values[] = { pid.Kp, pid.Ki, pid.Kd };
    ImVec2 points[3];

    for (int i = 0; i < 3; i++) {
        float angle = i * 2.0f * 3.1415926535f / 3.0f - 3.1415926535f / 2.0f;
        ImVec2 axisEnd = ImVec2(center.x + cosf(angle) * radius, center.y + sinf(angle) * radius);
        draw_list->AddLine(center, axisEnd, IM_COL32(150, 150, 150, 150));
        
        // Label
        ImVec2 labelPos = ImVec2(center.x + cosf(angle) * (radius + 25 * dpiScale), 
                                 center.y + sinf(angle) * (radius + 15 * dpiScale));
        ImVec2 labelSize = ImGui::CalcTextSize(labels[i]);
        draw_list->AddText(ImVec2(labelPos.x - labelSize.x / 2, labelPos.y - labelSize.y / 2), 
                          IM_COL32(200, 200, 200, 255), labels[i]);

        // Value point
        float valNorm = values[i] / maxValues[i];
        if (valNorm > 1.2f) valNorm = 1.2f;  // Allow slight overflow
        if (valNorm < 0.0f) valNorm = 0.0f;
        points[i] = ImVec2(center.x + cosf(angle) * radius * valNorm, 
                           center.y + sinf(angle) * radius * valNorm);
    }

    // Draw filled polygon
    draw_list->AddConvexPolyFilled(points, 3, IM_COL32(255, 100, 100, 100));
    draw_list->AddPolyline(points, 3, IM_COL32(255, 100, 100, 255), ImDrawFlags_Closed, 2.0f * dpiScale);

    ImGui::Dummy(size);
}

} // namespace ImGuiRenderer
