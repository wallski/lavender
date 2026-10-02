#pragma once
#include "imgui.h"

namespace Widgets {
    bool Toggle(const char* label, bool* v);
    bool Keybind(const char* label, int* key);
    bool ColorPicker(const char* label, float col[4]);
    bool SliderFloat(const char* label, float* v, float min, float max, const char* fmt = "%.2f");
    bool SliderInt(const char* label, int* v, int min, int max);
    bool SidebarItem(const char* title, const char* subtitle, int* current, int index);
    bool SidebarTab(const char* label, int* current, int index);
    bool TopTab(const char* label, int* current, int index);
    void SectionHeader(const char* text);
    void TextMuted(const char* text);

    void BeginCard(const char* title, int rows);
    void EndCard();
}