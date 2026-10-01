#include "pch.h"
#include "Widgets.h"
#include "Style.h"
#include "imgui_internal.h"
#include <unordered_map>

namespace Widgets {

    static std::unordered_map<ImGuiID, float> g_toggleAnim;

    static void DrawRowBg(ImVec2 pos, float width, float height, bool hovered) {
        if (!hovered) return;
        const float inset = 6.0f;
        ImGui::GetWindowDrawList()->AddRectFilled(
            ImVec2(pos.x + inset, pos.y + 1),
            ImVec2(pos.x + width - inset, pos.y + height - 1),
            ImGui::GetColorU32(ImVec4(1, 1, 1, 0.04f)), 5.0f);
    }

    static void DrawRowLabel(ImVec2 pos, float rowH, const char* label) {
        ImVec2 textSize = ImGui::CalcTextSize(label);
        ImGui::GetWindowDrawList()->AddText(
            ImVec2(pos.x + Style::RowInnerPad, pos.y + rowH * 0.5f - textSize.y * 0.5f),
            ImGui::GetColorU32(Style::Text), label);
    }

    bool Toggle(const char* label, bool* v) {
        ImGui::PushID(label);
        ImVec2 pos = ImGui::GetCursorScreenPos();
        float avail = ImGui::GetContentRegionAvail().x;
        float rowH = Style::RowHeight;

        ImGui::InvisibleButton("##row", ImVec2(avail, rowH));
        bool clicked = ImGui::IsItemClicked();
        bool hovered = ImGui::IsItemHovered();

        if (clicked) *v = !*v;

        DrawRowBg(pos, avail, rowH, hovered);
        DrawRowLabel(pos, rowH, label);

        const float trackW = 36.0f;
        const float trackH = 20.0f;
        ImVec2 trackPos(pos.x + avail - trackW - Style::RowInnerPad,
            pos.y + rowH * 0.5f - trackH * 0.5f);

        ImGuiID id = ImGui::GetID("##anim");
        float& anim = g_toggleAnim[id];
        float target = *v ? 1.0f : 0.0f;
        float dt = ImGui::GetIO().DeltaTime;
        float step = dt / 0.12f;
        anim += (target - anim) * ImMin(1.0f, step * 3.0f);
        if (fabsf(anim - target) < 0.002f) anim = target;

        ImVec4 offCol(0.19f, 0.19f, 0.23f, 1.0f);
        ImVec4 onCol = Style::Accent;
        ImVec4 trackCol(
            offCol.x + (onCol.x - offCol.x) * anim,
            offCol.y + (onCol.y - offCol.y) * anim,
            offCol.z + (onCol.z - offCol.z) * anim,
            1.0f);

        ImDrawList* d = ImGui::GetWindowDrawList();
        d->AddRectFilled(trackPos,
            ImVec2(trackPos.x + trackW, trackPos.y + trackH),
            ImGui::GetColorU32(trackCol), trackH * 0.5f);

        float knobR = 7.0f;
        float knobPad = 3.0f;
        float knobX = trackPos.x + knobR + knobPad
            + (trackW - (knobR + knobPad) * 2.0f) * anim;
        float knobY = trackPos.y + trackH * 0.5f;
        d->AddCircleFilled(ImVec2(knobX, knobY), knobR, IM_COL32(255, 255, 255, 255));

        ImGui::PopID();
        return clicked;
    }

    bool Keybind(const char* label, int* key) {
        ImGui::PushID(label);
        ImVec2 pos = ImGui::GetCursorScreenPos();
        float avail = ImGui::GetContentRegionAvail().x;
        float rowH = Style::RowHeight;

        static ImGuiID s_capturing = 0;
        ImGuiID myId = ImGui::GetID("##kb");

        ImGui::InvisibleButton("##row", ImVec2(avail, rowH));
        bool clicked = ImGui::IsItemClicked();
        bool hovered = ImGui::IsItemHovered();

        if (clicked) s_capturing = myId;

        if (s_capturing == myId) {
            for (int vk = 1; vk < 256; vk++) {
                if (GetAsyncKeyState(vk) & 0x8000) {
                    if (vk == VK_ESCAPE) {
                        s_capturing = 0;
                        break;
                    }
                    if (vk == VK_LBUTTON || vk == VK_RBUTTON || vk == VK_MBUTTON) continue;
                    *key = vk;
                    s_capturing = 0;
                    break;
                }
            }
        }

        DrawRowBg(pos, avail, rowH, hovered);
        DrawRowLabel(pos, rowH, label);

        char keyName[32] = "None";
        if (s_capturing == myId) {
            strcpy_s(keyName, "...");
        }
        else if (*key != 0) {
            switch (*key) {
            case VK_INSERT:  strcpy_s(keyName, "Insert");  break;
            case VK_DELETE:  strcpy_s(keyName, "Delete");  break;
            case VK_HOME:    strcpy_s(keyName, "Home");    break;
            case VK_END:     strcpy_s(keyName, "End");     break;
            case VK_SHIFT:   strcpy_s(keyName, "Shift");   break;
            case VK_MENU:    strcpy_s(keyName, "Alt");     break;
            case VK_CONTROL: strcpy_s(keyName, "Ctrl");    break;
            case VK_SPACE:   strcpy_s(keyName, "Space");   break;
            default: {
                UINT sc = MapVirtualKeyA(*key, MAPVK_VK_TO_VSC);
                if (!GetKeyNameTextA(sc << 16, keyName, sizeof(keyName)))
                    sprintf_s(keyName, "VK 0x%X", *key);
                break;
            }
            }
        }

        ImVec2 txtSize = ImGui::CalcTextSize(keyName);
        float boxW = txtSize.x + 24;
        float boxH = 22.0f;
        ImVec2 boxPos(pos.x + avail - boxW - Style::RowInnerPad,
            pos.y + rowH * 0.5f - boxH * 0.5f);
        ImVec2 boxEnd(boxPos.x + boxW, boxPos.y + boxH);

        ImDrawList* d = ImGui::GetWindowDrawList();
        bool active = (s_capturing == myId);
        ImU32 bg = active ? ImGui::GetColorU32(Style::Accent)
            : ImGui::GetColorU32(ImVec4(0.15f, 0.15f, 0.19f, 1.0f));
        d->AddRectFilled(boxPos, boxEnd, bg, 4.0f);
        d->AddText(ImVec2(boxPos.x + (boxW - txtSize.x) * 0.5f,
            boxPos.y + (boxH - txtSize.y) * 0.5f),
            ImGui::GetColorU32(Style::Text), keyName);

        ImGui::PopID();
        return clicked;
    }

    bool ColorPicker(const char* label, float col[4]) {
        ImGui::PushID(label);
        ImVec2 pos = ImGui::GetCursorScreenPos();
        float avail = ImGui::GetContentRegionAvail().x;
        float rowH = Style::RowHeight;

        ImGui::InvisibleButton("##row", ImVec2(avail, rowH));
        bool clicked = ImGui::IsItemClicked();
        bool hovered = ImGui::IsItemHovered();

        DrawRowBg(pos, avail, rowH, hovered);
        DrawRowLabel(pos, rowH, label);

        float swW = 44.0f;
        float swH = 22.0f;
        ImVec2 swPos(pos.x + avail - swW - Style::RowInnerPad,
            pos.y + rowH * 0.5f - swH * 0.5f);
        ImVec2 swEnd(swPos.x + swW, swPos.y + swH);

        ImDrawList* d = ImGui::GetWindowDrawList();
        d->AddRectFilled(swPos, swEnd, ImGui::GetColorU32(ImVec4(col[0], col[1], col[2], col[3])), 4.0f);
        d->AddRect(swPos, swEnd, ImGui::GetColorU32(Style::Border), 4.0f);

        bool changed = false;
        if (clicked) ImGui::OpenPopup("##pick");
        if (ImGui::BeginPopup("##pick")) {
            changed = ImGui::ColorPicker4("##cp", col,
                ImGuiColorEditFlags_NoSidePreview |
                ImGuiColorEditFlags_NoSmallPreview |
                ImGuiColorEditFlags_AlphaBar);
            ImGui::EndPopup();
        }

        ImGui::PopID();
        return changed;
    }

    bool SidebarTab(const char* label, int* current, int index) {
        ImGui::PushID(index);
        ImVec2 pos = ImGui::GetCursorScreenPos();
        float avail = ImGui::GetContentRegionAvail().x;
        float rowH = 38.0f;

        ImGui::InvisibleButton("##tab", ImVec2(avail, rowH));
        bool clicked = ImGui::IsItemClicked();
        bool hovered = ImGui::IsItemHovered();
        bool active = (*current == index);

        if (clicked) *current = index;

        ImDrawList* d = ImGui::GetWindowDrawList();

        const float inset = 10.0f;
        if (active) {
            d->AddRectFilled(ImVec2(pos.x + inset, pos.y + 2),
                ImVec2(pos.x + avail - inset, pos.y + rowH - 2),
                ImGui::GetColorU32(ImVec4(1, 1, 1, 0.055f)), 6.0f);
            d->AddRectFilled(ImVec2(pos.x + inset + 2, pos.y + 10),
                ImVec2(pos.x + inset + 5, pos.y + rowH - 10),
                ImGui::GetColorU32(Style::Accent), 2.0f);
        }
        else if (hovered) {
            d->AddRectFilled(ImVec2(pos.x + inset, pos.y + 2),
                ImVec2(pos.x + avail - inset, pos.y + rowH - 2),
                ImGui::GetColorU32(ImVec4(1, 1, 1, 0.025f)), 6.0f);
        }

        ImU32 col = ImGui::GetColorU32(active ? Style::Text : Style::TextMuted);
        ImVec2 textSize = ImGui::CalcTextSize(label);
        d->AddText(ImVec2(pos.x + inset + 18, pos.y + rowH * 0.5f - textSize.y * 0.5f), col, label);

        ImGui::PopID();
        return clicked;
    }

    void SectionHeader(const char* text) {
        ImGui::Dummy(ImVec2(0, 8));
        ImGui::PushStyleColor(ImGuiCol_Text, Style::TextMuted);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + Style::RowInnerPad);
        ImGui::TextUnformatted(text);
        ImGui::PopStyleColor();

        ImVec2 p = ImGui::GetCursorScreenPos();
        float w = ImGui::GetContentRegionAvail().x;
        ImGui::GetWindowDrawList()->AddLine(
            ImVec2(p.x + Style::RowInnerPad, p.y + 6),
            ImVec2(p.x + w - Style::RowInnerPad, p.y + 6),
            ImGui::GetColorU32(Style::Border));
        ImGui::Dummy(ImVec2(0, 14));
    }

    void TextMuted(const char* text) {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + Style::RowInnerPad);
        ImGui::PushStyleColor(ImGuiCol_Text, Style::TextMuted);
        ImGui::TextUnformatted(text);
        ImGui::PopStyleColor();
    }
}