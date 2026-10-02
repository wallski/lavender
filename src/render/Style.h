#pragma once
#include "imgui.h"

namespace Style {
    extern ImVec4 Bg;
    extern ImVec4 Panel;
    extern ImVec4 PanelHover;
    extern ImVec4 Border;
    extern ImVec4 Text;
    extern ImVec4 TextMuted;
    extern ImVec4 TextDim;
    extern ImVec4 Accent;
    extern ImVec4 AccentDim;

    inline constexpr float WindowWidth = 800.0f;
    inline constexpr float WindowHeight = 520.0f;
    inline constexpr float SidebarWidth = 200.0f;
    inline constexpr float Padding = 12.0f;
    inline constexpr float RowInnerPad = 8.0f;
    inline constexpr float Rounding = 8.0f;
    inline constexpr float RoundingSm = 5.0f;
    inline constexpr float RowHeight = 28.0f;
    inline constexpr float FadeDuration = 0.15f;

    void Apply();
    void LoadFonts();
    void SetAccent(float r, float g, float b);
    ImFont* Regular();
    ImFont* Medium();
}