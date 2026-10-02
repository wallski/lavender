#pragma once
#include "imgui.h"

namespace Widgets {
    bool Toggle(const char* label, bool* v);
    bool Keybind(const char* label, int* key);
    bool ColorPicker(const char* label, float col[4]);
    bool SidebarItem(const char* title, const char* subtitle, int* current, int index);
    bool SidebarTab(const char* label, int* current, int index);
    bool TopTab(const char* label, int* current, int index);
    void SectionHeader(const char* text);
    void TextMuted(const char* text);

    void BeginCard(const char* title, int rows);
    void EndCard();
}