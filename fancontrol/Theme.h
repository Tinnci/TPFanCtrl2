#pragma once

// Theme constants for TPFanCtrl2 UI
// Inspired by ThinkPad's signature red color scheme

#include "imgui.h"

namespace Theme {
    // Primary Brand Colors
    constexpr float PRIMARY_R = 0.89f;
    constexpr float PRIMARY_G = 0.12f;
    constexpr float PRIMARY_B = 0.16f;
    
    inline ImVec4 Primary() { return ImVec4(PRIMARY_R, PRIMARY_G, PRIMARY_B, 1.0f); }
    inline ImVec4 PrimaryHover() { return ImVec4(1.0f, 0.2f, 0.25f, 1.0f); }
    inline ImVec4 PrimaryTransparent(float alpha = 0.3f) { return ImVec4(PRIMARY_R, PRIMARY_G, PRIMARY_B, alpha); }
    
    // Text Colors
    inline ImVec4 TextMuted() { return ImVec4(0.6f, 0.6f, 0.6f, 1.0f); }
    inline ImVec4 TextDark() { return ImVec4(0.4f, 0.4f, 0.4f, 1.0f); }
    inline ImVec4 TextWhite() { return ImVec4(1.0f, 1.0f, 1.0f, 1.0f); }
    
    // Background Colors
    inline ImVec4 WindowBg() { return ImVec4(0.07f, 0.07f, 0.07f, 0.94f); }
    inline ImVec4 ChildBg() { return ImVec4(0.12f, 0.12f, 0.12f, 0.5f); }
    inline ImVec4 FrameBg() { return ImVec4(0.15f, 0.15f, 0.15f, 1.0f); }
    inline ImVec4 Border() { return ImVec4(0.2f, 0.2f, 0.2f, 0.5f); }
    inline ImVec4 Separator() { return ImVec4(0.25f, 0.25f, 0.25f, 1.0f); }
    
    // Button Colors
    inline ImVec4 Button() { return ImVec4(0.2f, 0.2f, 0.2f, 1.0f); }
    inline ImVec4 ButtonHovered() { return Primary(); }
    inline ImVec4 ButtonActive() { return PrimaryHover(); }
    
    // Tab Colors
    inline ImVec4 Tab() { return ImVec4(0.15f, 0.15f, 0.15f, 1.0f); }
    inline ImVec4 TabHovered() { return Primary(); }
    inline ImVec4 TabActive() { return Primary(); }
    inline ImVec4 TabUnfocused() { return ImVec4(0.1f, 0.1f, 0.1f, 1.0f); }
    inline ImVec4 TabUnfocusedActive() { return ImVec4(0.2f, 0.2f, 0.2f, 1.0f); }
    
    // Temperature Colors
    inline ImVec4 TempCool() { return ImVec4(0.0f, 0.8f, 1.0f, 1.0f); }
    inline ImVec4 TempWarm() { return ImVec4(1.0f, 0.6f, 0.0f, 1.0f); }
    inline ImVec4 TempHot() { return ImVec4(1.0f, 0.2f, 0.2f, 1.0f); }
    
    inline ImVec4 GetTempColor(float temp) {
        if (temp < 50.0f) return TempCool();
        if (temp < 75.0f) return TempWarm();
        return TempHot();
    }
    
    // Layout Constants (before DPI scaling)
    namespace Layout {
        constexpr float CardHeight = 90.0f;
        constexpr float SidebarWidth = 180.0f;
        constexpr float SensorsAreaHeight = 280.0f;
        constexpr float ControlPanelHeight = 270.0f;  // Increased to prevent button overlap
        constexpr float ButtonHeight = 35.0f;
        constexpr float SmallButtonHeight = 25.0f;    // For compact buttons like presets
        constexpr float LargeButtonHeight = 40.0f;
        constexpr float InputWidth = 200.0f;
    }
    
    // Style Constants
    namespace Style {
        constexpr float WindowRounding = 10.0f;
        constexpr float FrameRounding = 6.0f;
        constexpr float PopupRounding = 6.0f;
        constexpr float ScrollbarRounding = 12.0f;
        constexpr float GrabRounding = 6.0f;
        constexpr float ItemSpacingX = 12.0f;
        constexpr float ItemSpacingY = 10.0f;
        constexpr float WindowPaddingX = 15.0f;
        constexpr float WindowPaddingY = 15.0f;
    }
    
    // Window Flags for scroll policy (see .gemini/ui-design-spec.md)
    namespace WindowFlags {
        // For fixed content panels that should NEVER scroll
        constexpr ImGuiWindowFlags NoScroll = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
        
        // For scrollable content areas (logs, lists)
        constexpr ImGuiWindowFlags Scrollable = ImGuiWindowFlags_None;
    }
    
    // Apply full theme to ImGui style
    inline void ApplyTheme(ImGuiStyle& style, float dpiScale) {
        style.ScaleAllSizes(dpiScale);
        style.WindowRounding = Style::WindowRounding * dpiScale;
        style.FrameRounding = Style::FrameRounding * dpiScale;
        style.PopupRounding = Style::PopupRounding * dpiScale;
        style.ScrollbarRounding = Style::ScrollbarRounding * dpiScale;
        style.GrabRounding = Style::GrabRounding * dpiScale;
        style.ItemSpacing = ImVec2(Style::ItemSpacingX * dpiScale, Style::ItemSpacingY * dpiScale);
        style.WindowPadding = ImVec2(Style::WindowPaddingX * dpiScale, Style::WindowPaddingY * dpiScale);
        
        // Colors
        style.Colors[ImGuiCol_WindowBg] = WindowBg();
        style.Colors[ImGuiCol_ChildBg] = ChildBg();
        style.Colors[ImGuiCol_Border] = Border();
        style.Colors[ImGuiCol_FrameBg] = FrameBg();
        style.Colors[ImGuiCol_CheckMark] = Primary();
        style.Colors[ImGuiCol_SliderGrab] = Primary();
        style.Colors[ImGuiCol_SliderGrabActive] = PrimaryHover();
        style.Colors[ImGuiCol_Button] = Button();
        style.Colors[ImGuiCol_ButtonHovered] = ButtonHovered();
        style.Colors[ImGuiCol_ButtonActive] = ButtonActive();
        style.Colors[ImGuiCol_Header] = PrimaryTransparent(0.3f);
        style.Colors[ImGuiCol_HeaderHovered] = PrimaryTransparent(0.5f);
        style.Colors[ImGuiCol_HeaderActive] = Primary();
        style.Colors[ImGuiCol_Tab] = Tab();
        style.Colors[ImGuiCol_TabHovered] = TabHovered();
        style.Colors[ImGuiCol_TabActive] = TabActive();
        style.Colors[ImGuiCol_TabUnfocused] = TabUnfocused();
        style.Colors[ImGuiCol_TabUnfocusedActive] = TabUnfocusedActive();
        style.Colors[ImGuiCol_PlotHistogram] = Primary();
        style.Colors[ImGuiCol_Separator] = Separator();
    }
}
