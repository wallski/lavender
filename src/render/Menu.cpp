#include "pch.h"
#include "Menu.h"
#include "Style.h"
#include "Widgets.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "../core/features/visuals/chams/Chams.h"
#include "../core/features/visuals/chams/Materials.h"
#include "../core/features/visuals/esp/Esp.h"
#include "../core/features/visuals/removals/Removals.h"
#include "../core/features/visuals/skybox/Skybox.h"
#include "../core/features/visuals/bombtimer/BombTimer.h"
#include "../core/features/combat/legit/triggerbot/Triggerbot.h"
#include "../core/features/combat/legit/aim/Aim.h"
#include "../core/features/combat/rage/Rage.h"
#include "../core/config/ConfigManager.h"

namespace Menu {

    static int g_activeTab = 0;
    static int g_legitSubTab = 0;
    static int g_visualsSubTab = 0;
    static int g_menuKey = VK_DELETE;
    static char g_configNameBuf[64] = "default";
    static std::string g_pendingDelete;

    static const char* kLegitSubTabs[] = { "Aim", "Trigger" };
    static const char* kVisualsSubTabs[] = { "Chams", "ESP", "Info" };

    static const char* kMaterialNames[] = {
        "None", "Matte", "Flat", "Glow", "Outlines", "Bloom"
    };

    static const char* kHitboxNames[] = {
        "Head", "Neck", "Chest", "Stomach", "Pelvis", "Arms", "Legs"
    };

    static const char* kBarPositionNames[] = { "Left", "Top", "Bottom" };
    static const char* kNamePositionNames[] = { "Above", "Below" };
    static const char* kHealthStyleNames[] = { "Static", "Gradient" };

    int GetMenuKey() { return g_menuKey; }
    void SetMenuKey(int key) { g_menuKey = key; }

    static void DrawHitboxDropdown(uint32_t* hitboxes, void (*setter)(uint32_t)) {
        constexpr float rowH = 30.0f;
        ImGui::PushID("##hitbox_dropdown");

        ImVec2 pos = ImGui::GetCursorScreenPos();
        float avail = ImGui::GetContentRegionAvail().x;

        ImGui::InvisibleButton("##row", ImVec2(avail, rowH));
        bool clicked = ImGui::IsItemClicked();
        bool hovered = ImGui::IsItemHovered();

        ImDrawList* d = ImGui::GetWindowDrawList();
        if (hovered) {
            d->AddRectFilled(pos, ImVec2(pos.x + avail, pos.y + rowH),
                ImGui::GetColorU32(ImVec4(1, 1, 1, 0.030f)), 4.0f);
        }

        ImVec2 labelSize = ImGui::CalcTextSize("Hitboxes");
        d->AddText(ImVec2(pos.x + Style::RowInnerPad, pos.y + rowH * 0.5f - labelSize.y * 0.5f),
            ImGui::GetColorU32(Style::Text), "Hitboxes");

        char preview[128] = "";
        for (int i = 0; i < 7; i++) {
            if (*hitboxes & (1u << i)) {
                if (preview[0]) strcat_s(preview, ", ");
                strcat_s(preview, kHitboxNames[i]);
            }
        }
        if (!preview[0]) strcpy_s(preview, "None");

        ImVec2 ts = ImGui::CalcTextSize(preview);
        d->AddText(ImVec2(pos.x + avail - ts.x - Style::RowInnerPad, pos.y + rowH * 0.5f - ts.y * 0.5f),
            ImGui::GetColorU32(Style::TextMuted), preview);

        if (clicked) ImGui::OpenPopup("##hb_popup");

        if (ImGui::BeginPopup("##hb_popup")) {
            for (int i = 0; i < 7; i++) {
                bool on = (*hitboxes & (1u << i)) != 0;
                if (ImGui::Checkbox(kHitboxNames[i], &on)) {
                    if (on) *hitboxes |= (1u << i);
                    else    *hitboxes &= ~(1u << i);
                    if (*hitboxes == 0) *hitboxes = 1u << i;
                    if (setter) setter(*hitboxes);
                }
            }
            ImGui::EndPopup();
        }

        ImGui::PopID();
    }

    static void DrawRageMainCard() {
        bool enabled = Rage::IsEnabled();
        bool autoShoot = Rage::IsAutoShoot();
        bool silent = Rage::IsSilent();
        bool teamCheck = Rage::IsTeamCheck();
        bool noSpread = Rage::IsNoSpread();
        int minDamage = Rage::GetMinDamage();

        Widgets::BeginCard("RAGEBOT", 6);

        if (Widgets::Toggle("Enabled", &enabled)) {
            Rage::SetEnabled(enabled);
        }
        if (Widgets::Toggle("Auto shoot", &autoShoot)) {
            Rage::SetAutoShoot(autoShoot);
        }
        if (Widgets::Toggle("Silent", &silent)) {
            Rage::SetSilent(silent);
        }
        if (Widgets::Toggle("No spread", &noSpread)) {
            Rage::SetNoSpread(noSpread);
        }
        if (Widgets::Toggle("Team check", &teamCheck)) {
            Rage::SetTeamCheck(teamCheck);
        }
        if (Widgets::SliderInt("Min damage", &minDamage, 1, 130)) {
            Rage::SetMinDamage(minDamage);
        }

        Widgets::EndCard();
    }

    static void DrawRageHitboxCard() {
        uint32_t hitboxes = Rage::GetHitboxes();

        Widgets::BeginCard("HITBOXES", 1);
        DrawHitboxDropdown(&hitboxes, Rage::SetHitboxes);
        Widgets::EndCard();
    }

    static void DrawRageTab() {
        const float totalW = ImGui::GetContentRegionAvail().x;
        const float gap = 10.0f;
        const float colW = (totalW - gap) * 0.5f;

        ImGui::BeginChild("##rage_col_left", ImVec2(colW, 0), false,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        DrawRageMainCard();
        ImGui::EndChild();

        ImGui::SameLine(0, gap);

        ImGui::BeginChild("##rage_col_right", ImVec2(colW, 0), false,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        DrawRageHitboxCard();
        ImGui::EndChild();
    }

    static void DrawAimCard() {
        bool enabled = Aim::IsEnabled();
        int key = Aim::GetKey();
        bool teamCheck = Aim::IsTeamCheck();
        float fov = Aim::GetFov();
        int smooth = Aim::GetSmooth();
        bool onAccuracy = Aim::IsOnAccuracy();
        float accuracySpeed = Aim::GetAccuracySpeed();

        int rows = 6;
        if (onAccuracy) rows += 1;

        Widgets::BeginCard("AIMBOT", rows);

        if (Widgets::Toggle("Enabled", &enabled)) {
            Aim::SetEnabled(enabled);
        }
        if (Widgets::Keybind("Aim key", &key)) {
            Aim::SetKey(key);
        }
        if (Widgets::Toggle("Team check", &teamCheck)) {
            Aim::SetTeamCheck(teamCheck);
        }
        if (Widgets::SliderFloat("FOV", &fov, 0.5f, 180.0f, "%.1f")) {
            Aim::SetFov(fov);
        }
        if (Widgets::SliderInt("Smooth", &smooth, 1, 30)) {
            Aim::SetSmooth(smooth);
        }
        if (Widgets::Toggle("On accuracy", &onAccuracy)) {
            Aim::SetOnAccuracy(onAccuracy);
        }
        if (onAccuracy) {
            if (Widgets::SliderFloat("Max speed", &accuracySpeed, 0.5f, 30.0f, "%.1f")) {
                Aim::SetAccuracySpeed(accuracySpeed);
            }
        }

        Widgets::EndCard();
    }

    static void DrawAimHitboxCard() {
        uint32_t hitboxes = Aim::GetHitboxes();

        Widgets::BeginCard("HITBOXES", 1);
        DrawHitboxDropdown(&hitboxes, Aim::SetHitboxes);
        Widgets::EndCard();
    }

    static void DrawTriggerCard() {
        bool enabled = Triggerbot::IsEnabled();
        int key = Triggerbot::GetKey();
        bool teamCheck = Triggerbot::GetTeamCheck();

        Widgets::BeginCard("TRIGGERBOT", 3);

        if (Widgets::Toggle("Enabled", &enabled)) {
            Triggerbot::SetEnabled(enabled);
        }
        if (Widgets::Keybind("Trigger key", &key)) {
            Triggerbot::SetKey(key);
        }
        if (Widgets::Toggle("Team check", &teamCheck)) {
            Triggerbot::SetTeamCheck(teamCheck);
        }

        Widgets::EndCard();
    }

    static void DrawTriggerHitboxCard() {
        uint32_t hitboxes = Triggerbot::GetHitboxes();

        Widgets::BeginCard("HITBOXES", 1);
        DrawHitboxDropdown(&hitboxes, Triggerbot::SetHitboxes);
        Widgets::EndCard();
    }

    static void DrawTriggerTimingCard() {
        int delay = Triggerbot::GetDelayMs();

        Widgets::BeginCard("TIMING", 2);
        if (Widgets::SliderInt("On-target ms", &delay, 0, 1000)) {
            Triggerbot::SetDelayMs(delay);
        }
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + Style::RowInnerPad);
        ImGui::SetNextItemWidth(120);
        if (ImGui::InputInt("##delay_input", &delay, 10, 100)) {
            Triggerbot::SetDelayMs(delay);
        }
        Widgets::EndCard();
    }

    static void DrawAimSubTab() {
        const float totalW = ImGui::GetContentRegionAvail().x;
        const float gap = 10.0f;
        const float colW = (totalW - gap) * 0.5f;

        ImGui::BeginChild("##aim_col_left", ImVec2(colW, 0), false,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        DrawAimCard();
        ImGui::EndChild();

        ImGui::SameLine(0, gap);

        ImGui::BeginChild("##aim_col_right", ImVec2(colW, 0), false,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        DrawAimHitboxCard();
        ImGui::EndChild();
    }

    static void DrawTriggerSubTab() {
        const float totalW = ImGui::GetContentRegionAvail().x;
        const float gap = 10.0f;
        const float colW = (totalW - gap) * 0.5f;

        ImGui::BeginChild("##trigger_col_left", ImVec2(colW, 0), false,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        DrawTriggerCard();
        ImGui::EndChild();

        ImGui::SameLine(0, gap);

        ImGui::BeginChild("##trigger_col_right", ImVec2(colW, 0), false,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        DrawTriggerHitboxCard();
        DrawTriggerTimingCard();
        ImGui::EndChild();
    }

    static void DrawSubTabBar(const char* const* names, int count, int* current) {
        ImGui::Dummy(ImVec2(0, 2));
        for (int i = 0; i < count; i++) {
            Widgets::TopTab(names[i], current, i);
        }
        ImGui::NewLine();

        const ImVec2 p = ImGui::GetCursorScreenPos();
        const float w = ImGui::GetContentRegionAvail().x;
        ImGui::GetWindowDrawList()->AddLine(
            ImVec2(p.x, p.y - 6.0f),
            ImVec2(p.x + w, p.y - 6.0f),
            ImGui::GetColorU32(Style::Border), 1.0f);

        ImGui::Dummy(ImVec2(0, 8));
    }

    static void DrawLegitTab() {
        DrawSubTabBar(kLegitSubTabs, IM_ARRAYSIZE(kLegitSubTabs), &g_legitSubTab);

        switch (g_legitSubTab) {
        case 0: DrawAimSubTab();     break;
        case 1: DrawTriggerSubTab(); break;
        }
    }

    static void DrawChamsCard() {
        bool enabled = Chams::IsEnabled();
        float* cur = Chams::GetColor();
        float color[4] = { cur[0], cur[1], cur[2], cur[3] };
        int mat = (int)Chams::GetMaterial();
        bool occluded = Chams::IsOccluded();
        bool chamsTeamCheck = Chams::IsTeamCheck();

        Widgets::BeginCard("CHAMS", 5);

        if (Widgets::Toggle("Enabled", &enabled)) {
            Chams::SetEnabled(enabled);
        }
        if (Widgets::ColorPicker("Color", color)) {
            Chams::SetColor(color);
        }
        if (ImGui::Combo("Material", &mat, kMaterialNames, IM_ARRAYSIZE(kMaterialNames))) {
            Chams::SetMaterial((Materials::Id)mat);
        }
        if (Widgets::Toggle("Occluded", &occluded)) {
            Chams::SetOccluded(occluded);
        }
        if (Widgets::Toggle("Team check", &chamsTeamCheck)) {
            Chams::SetTeamCheck(chamsTeamCheck);
        }

        Widgets::EndCard();
    }

    static void DrawEspCard() {
        bool espEnabled = Esp::IsEnabled();
        bool espTeamCheck = Esp::IsTeamCheck();

        Widgets::BeginCard("GENERAL", 2);
        if (Widgets::Toggle("Enabled", &espEnabled)) {
            Esp::SetEnabled(espEnabled);
        }
        if (Widgets::Toggle("Team check", &espTeamCheck)) {
            Esp::SetTeamCheck(espTeamCheck);
        }
        Widgets::EndCard();
    }

    static void DrawSkeletonCard() {
        bool enabled = Esp::IsSkeletonEnabled();
        float color[4];
        float occludedColor[4];
        memcpy(color, Esp::GetSkeletonColor(), sizeof(color));
        memcpy(occludedColor, Esp::GetSkeletonOccludedColor(), sizeof(occludedColor));
        float thickness = Esp::GetSkeletonThickness();
        bool debug = Esp::IsSkeletonDebug();

        int rows = 3;
        if (enabled) rows += 2;

        Widgets::BeginCard("SKELETON", rows);

        if (Widgets::Toggle("Enabled", &enabled)) {
            Esp::SetSkeletonEnabled(enabled);
        }
        if (Widgets::ColorPicker("Visible color", color)) {
            Esp::SetSkeletonColor(color);
        }
        if (Widgets::ColorPicker("Occluded color", occludedColor)) {
            Esp::SetSkeletonOccludedColor(occludedColor);
        }
        if (enabled) {
            if (Widgets::SliderFloat("Thickness", &thickness, 0.5f, 5.0f, "%.1f")) {
                Esp::SetSkeletonThickness(thickness);
            }
            if (Widgets::Toggle("Debug bones", &debug)) {
                Esp::SetSkeletonDebug(debug);
            }
        }

        Widgets::EndCard();
    }

    static void DrawEspBoxCard() {
        bool boxEnabled = Esp::IsBoxEnabled();
        int boxStyle = Esp::GetBoxStyle();
        bool boxFill = Esp::IsBoxFillEnabled();
        bool boxOutline = Esp::IsBoxOutlineEnabled();
        float cornerLen = Esp::GetBoxCornerLength();
        float visibleColor[4];
        float occludedColor[4];
        float fillColor[4];
        memcpy(visibleColor, Esp::GetBoxVisibleColor(), sizeof(visibleColor));
        memcpy(occludedColor, Esp::GetBoxOccludedColor(), sizeof(occludedColor));
        memcpy(fillColor, Esp::GetBoxFillColor(), sizeof(fillColor));

        int rows = 6;
        if (boxStyle == 1) rows++;
        if (boxFill) rows++;

        Widgets::BeginCard("BOX", rows);

        if (Widgets::Toggle("Box", &boxEnabled)) {
            Esp::SetBoxEnabled(boxEnabled);
        }

        const char* styles[] = { "Full", "Corner" };
        if (ImGui::Combo("Style", &boxStyle, styles, IM_ARRAYSIZE(styles))) {
            Esp::SetBoxStyle(boxStyle);
        }

        if (Widgets::Toggle("Outline", &boxOutline)) {
            Esp::SetBoxOutlineEnabled(boxOutline);
        }

        if (boxStyle == 1) {
            if (Widgets::SliderFloat("Corner length", &cornerLen, 1.0f, 30.0f, "%.0f")) {
                Esp::SetBoxCornerLength(cornerLen);
            }
        }

        if (Widgets::Toggle("Fill", &boxFill)) {
            Esp::SetBoxFillEnabled(boxFill);
        }

        if (boxFill) {
            if (Widgets::ColorPicker("Fill color", fillColor)) {
                Esp::SetBoxFillColor(fillColor);
            }
        }

        if (Widgets::ColorPicker("Visible color", visibleColor)) {
            Esp::SetBoxVisibleColor(visibleColor);
        }
        if (Widgets::ColorPicker("Occluded color", occludedColor)) {
            Esp::SetBoxOccludedColor(occludedColor);
        }

        Widgets::EndCard();
    }

    static void DrawEspSubTab() {
        const float totalW = ImGui::GetContentRegionAvail().x;
        const float gap = 10.0f;
        const float colW = (totalW - gap) * 0.5f;

        ImGui::BeginChild("##esp_col_left", ImVec2(colW, 0), false,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        DrawEspCard();
        DrawSkeletonCard();
        ImGui::EndChild();

        ImGui::SameLine(0, gap);

        ImGui::BeginChild("##esp_col_right", ImVec2(colW, 0), false,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        DrawEspBoxCard();
        ImGui::EndChild();
    }

    static void DrawEspHealthCard() {
        bool enabled = Esp::IsHealthEnabled();
        int position = Esp::GetHealthPosition();
        int style = Esp::GetHealthStyle();
        bool showValue = Esp::IsHealthShowValue();
        float thickness = Esp::GetHealthThickness();

        float color[4];
        float lowColor[4];
        float highColor[4];
        memcpy(color, Esp::GetHealthColor(), sizeof(color));
        memcpy(lowColor, Esp::GetHealthLowColor(), sizeof(lowColor));
        memcpy(highColor, Esp::GetHealthHighColor(), sizeof(highColor));

        int rows = 4;
        rows += (style == 0) ? 1 : 2;
        rows += 1;

        Widgets::BeginCard("HEALTH", rows);

        if (Widgets::Toggle("Enabled", &enabled)) {
            Esp::SetHealthEnabled(enabled);
        }
        if (ImGui::Combo("Position", &position, kBarPositionNames, IM_ARRAYSIZE(kBarPositionNames))) {
            Esp::SetHealthPosition(position);
        }
        if (ImGui::Combo("Style", &style, kHealthStyleNames, IM_ARRAYSIZE(kHealthStyleNames))) {
            Esp::SetHealthStyle(style);
        }
        if (style == 0) {
            if (Widgets::ColorPicker("Color", color)) {
                Esp::SetHealthColor(color);
            }
        }
        else {
            if (Widgets::ColorPicker("Low color", lowColor)) {
                Esp::SetHealthLowColor(lowColor);
            }
            if (Widgets::ColorPicker("High color", highColor)) {
                Esp::SetHealthHighColor(highColor);
            }
        }
        if (Widgets::Toggle("Show value", &showValue)) {
            Esp::SetHealthShowValue(showValue);
        }
        if (Widgets::SliderFloat("Thickness", &thickness, 1.0f, 6.0f, "%.0f")) {
            Esp::SetHealthThickness(thickness);
        }

        Widgets::EndCard();
    }

    static void DrawEspAmmoCard() {
        bool enabled = Esp::IsAmmoEnabled();
        int position = Esp::GetAmmoPosition();
        bool showValue = Esp::IsAmmoShowValue();
        float thickness = Esp::GetAmmoThickness();

        float color[4];
        memcpy(color, Esp::GetAmmoColor(), sizeof(color));

        Widgets::BeginCard("AMMO", 5);

        if (Widgets::Toggle("Enabled", &enabled)) {
            Esp::SetAmmoEnabled(enabled);
        }
        if (ImGui::Combo("Position", &position, kBarPositionNames, IM_ARRAYSIZE(kBarPositionNames))) {
            Esp::SetAmmoPosition(position);
        }
        if (Widgets::ColorPicker("Color", color)) {
            Esp::SetAmmoColor(color);
        }
        if (Widgets::Toggle("Show value", &showValue)) {
            Esp::SetAmmoShowValue(showValue);
        }
        if (Widgets::SliderFloat("Thickness", &thickness, 1.0f, 6.0f, "%.0f")) {
            Esp::SetAmmoThickness(thickness);
        }

        Widgets::EndCard();
    }

    static void DrawEspNameCard() {
        bool enabled = Esp::IsNameEnabled();
        int position = Esp::GetNamePosition();

        float color[4];
        float occludedColor[4];
        memcpy(color, Esp::GetNameColor(), sizeof(color));
        memcpy(occludedColor, Esp::GetNameOccludedColor(), sizeof(occludedColor));

        Widgets::BeginCard("NAME", 4);

        if (Widgets::Toggle("Enabled", &enabled)) {
            Esp::SetNameEnabled(enabled);
        }
        if (ImGui::Combo("Position", &position, kNamePositionNames, IM_ARRAYSIZE(kNamePositionNames))) {
            Esp::SetNamePosition(position);
        }
        if (Widgets::ColorPicker("Visible color", color)) {
            Esp::SetNameColor(color);
        }
        if (Widgets::ColorPicker("Occluded color", occludedColor)) {
            Esp::SetNameOccludedColor(occludedColor);
        }

        Widgets::EndCard();
    }

    static void DrawInfoSubTab() {
        const float totalW = ImGui::GetContentRegionAvail().x;
        const float gap = 10.0f;
        const float colW = (totalW - gap) * 0.5f;

        ImGui::BeginChild("##info_col_left", ImVec2(colW, 0), false,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        DrawEspHealthCard();
        ImGui::EndChild();

        ImGui::SameLine(0, gap);

        ImGui::BeginChild("##info_col_right", ImVec2(colW, 0), false,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        DrawEspAmmoCard();
        DrawEspNameCard();
        ImGui::EndChild();
    }

    static void DrawVisualsTab() {
        DrawSubTabBar(kVisualsSubTabs, IM_ARRAYSIZE(kVisualsSubTabs), &g_visualsSubTab);

        switch (g_visualsSubTab) {
        case 0: DrawChamsCard();    break;
        case 1: DrawEspSubTab();    break;
        case 2: DrawInfoSubTab();   break;
        }
    }

    static void DrawWorldTab() {
        bool enabled = Skybox::IsEnabled();
        int sel = Skybox::GetSelectedIndex();
        const auto& entries = Skybox::GetEntries();

        std::vector<const char*> names;
        names.reserve(entries.size());
        for (const auto& e : entries) names.push_back(e.name.c_str());

        Widgets::BeginCard("SKYBOX", 2);

        if (Widgets::Toggle("Custom skybox", &enabled)) {
            Skybox::SetEnabled(enabled);
        }

        if (!names.empty()) {
            if (ImGui::Combo("Sky", &sel, names.data(), (int)names.size())) {
                Skybox::SetSelectedIndex(sel);
            }
        }

        Widgets::EndCard();
    }

    static void DrawFovCircleCard() {
        bool enabled = Esp::IsFovCircleEnabled();
        float color[4];
        memcpy(color, Esp::GetFovCircleColor(), sizeof(color));
        float fov = Aim::GetFov();

        Widgets::BeginCard("FOV CIRCLE", 3);
        if (Widgets::Toggle("Enabled", &enabled)) {
            Esp::SetFovCircleEnabled(enabled);
        }
        {
            char fovText[32];
            snprintf(fovText, sizeof(fovText), "%.1f (aim fov)", fov);
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + Style::RowInnerPad);
            ImGui::PushStyleColor(ImGuiCol_Text, Style::TextMuted);
            ImGui::TextUnformatted(fovText);
            ImGui::PopStyleColor();
        }
        if (Widgets::ColorPicker("Color", color)) {
            Esp::SetFovCircleColor(color);
        }
        Widgets::EndCard();
    }

    static void DrawBombTimerCard() {
        bool enabled = BombTimer::IsEnabled();
        bool defuse = BombTimer::IsDefuseEnabled();
        bool site = BombTimer::IsSiteEnabled();
        float scale = BombTimer::GetScale();
        float color[4];
        float defuseColor[4];
        memcpy(color, BombTimer::GetColor(), sizeof(color));
        memcpy(defuseColor, BombTimer::GetDefuseColor(), sizeof(defuseColor));

        int rows = 5;
        if (defuse) rows += 1;

        Widgets::BeginCard("BOMB TIMER", rows);

        if (Widgets::Toggle("Enabled", &enabled)) {
            BombTimer::SetEnabled(enabled);
        }
        if (Widgets::Toggle("Defuse timer", &defuse)) {
            BombTimer::SetDefuseEnabled(defuse);
        }
        if (Widgets::Toggle("Show site", &site)) {
            BombTimer::SetSiteEnabled(site);
        }
        if (Widgets::SliderFloat("Text size", &scale, 0.5f, 3.0f, "%.2f")) {
            BombTimer::SetScale(scale);
        }
        if (Widgets::ColorPicker("Color", color)) {
            BombTimer::SetColor(color);
        }
        if (defuse) {
            if (Widgets::ColorPicker("Defuse color", defuseColor)) {
                BombTimer::SetDefuseColor(defuseColor);
            }
        }

        Widgets::EndCard();
    }

    static void DrawMiscTab() {
        const float totalW = ImGui::GetContentRegionAvail().x;
        const float gap = 10.0f;
        const float colW = (totalW - gap) * 0.5f;

        ImGui::BeginChild("##misc_col_left", ImVec2(colW, 0), false,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        float current[4] = { Style::Accent.x, Style::Accent.y, Style::Accent.z, 1.0f };

        Widgets::BeginCard("MENU", 2);
        Widgets::Keybind("Menu key", &g_menuKey);
        if (Widgets::ColorPicker("Accent color", current)) {
            Style::SetAccent(current[0], current[1], current[2]);
            Style::Apply();
        }
        Widgets::EndCard();

        bool flash = Removals::IsFlashEnabled();
        bool smoke = Removals::IsSmokeEnabled();

        Widgets::BeginCard("REMOVALS", 2);
        if (Widgets::Toggle("Remove flash", &flash)) {
            Removals::SetFlashEnabled(flash);
        }
        if (Widgets::Toggle("Remove smoke", &smoke)) {
            Removals::SetSmokeEnabled(smoke);
        }
        Widgets::EndCard();

        ImGui::EndChild();

        ImGui::SameLine(0, gap);

        ImGui::BeginChild("##misc_col_right", ImVec2(colW, 0), false,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        DrawBombTimerCard();
        DrawFovCircleCard();

        ImGui::EndChild();
    }

    static void DrawConfigTab() {
        const float sideMargin = 20.0f;
        const float totalW = ImGui::GetContentRegionAvail().x;
        const float innerW = totalW - sideMargin * 2;

        ImGui::Dummy(ImVec2(0, 8));

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + sideMargin);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(14, 12));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.10f, 0.10f, 0.12f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.12f, 0.12f, 0.145f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.13f, 0.13f, 0.155f, 1.0f));
        ImGui::SetNextItemWidth(innerW);
        ImGui::InputTextWithHint("##cfg_name", "config name...", g_configNameBuf, sizeof(g_configNameBuf));
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar(2);

        ImGui::Dummy(ImVec2(0, 8));

        const float btnW = (innerW - 8) * 0.5f;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + sideMargin);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 12));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8);

        const ImVec4 accent = Style::Accent;
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(accent.x, accent.y, accent.z, 0.75f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(accent.x, accent.y, accent.z, 0.95f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(accent.x * 0.85f, accent.y * 0.85f, accent.z * 0.85f, 1.0f));
        if (ImGui::Button("Save", ImVec2(btnW, 0))) {
            if (g_configNameBuf[0]) ConfigManager::Save(g_configNameBuf);
        }
        ImGui::PopStyleColor(3);

        ImGui::SameLine(0, 8);

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.13f, 0.13f, 0.15f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.22f, 0.10f, 0.10f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.35f, 0.13f, 0.13f, 1.0f));
        if (ImGui::Button("Delete", ImVec2(btnW, 0))) {
            if (g_configNameBuf[0] && ConfigManager::Exists(g_configNameBuf)) {
                g_pendingDelete = g_configNameBuf;
                ImGui::OpenPopup("Delete Config");
            }
        }
        ImGui::PopStyleColor(3);

        ImGui::PopStyleVar(2);

        if (ImGui::BeginPopupModal("Delete Config", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Delete \"%s\"?", g_pendingDelete.c_str());
            ImGui::Dummy(ImVec2(0, 8));

            if (ImGui::Button("Yes", ImVec2(80, 0))) {
                ConfigManager::Delete(g_pendingDelete);
                g_pendingDelete.clear();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("No", ImVec2(80, 0))) {
                g_pendingDelete.clear();
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::Dummy(ImVec2(0, 18));
        Widgets::SectionHeader("Saved");

        const auto configs = ConfigManager::List();
        const std::string current = ConfigManager::GetCurrentName();

        if (configs.empty()) {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + sideMargin);
            Widgets::TextMuted("no configs saved yet");
        }
        else {
            for (const auto& name : configs) {
                ImGui::PushID(name.c_str());

                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + sideMargin);

                const ImVec2 startPos = ImGui::GetCursorScreenPos();
                ImGui::InvisibleButton("##row", ImVec2(innerW, 40.0f));
                const bool clicked = ImGui::IsItemClicked();
                const bool hovered = ImGui::IsItemHovered();
                const bool isCurrent = (name == current);

                ImDrawList* d = ImGui::GetWindowDrawList();
                const ImVec2 bgMin = startPos;
                const ImVec2 bgMax(startPos.x + innerW, startPos.y + 40.0f);

                ImU32 bg;
                if (isCurrent) {
                    bg = ImGui::GetColorU32(ImVec4(accent.x, accent.y, accent.z, 0.14f));
                }
                else if (hovered) {
                    bg = ImGui::GetColorU32(ImVec4(1, 1, 1, 0.05f));
                }
                else {
                    bg = ImGui::GetColorU32(ImVec4(1, 1, 1, 0.028f));
                }
                d->AddRectFilled(bgMin, bgMax, bg, 8.0f);

                if (isCurrent) {
                    d->AddRectFilled(
                        ImVec2(bgMin.x, bgMin.y + 10),
                        ImVec2(bgMin.x + 3, bgMax.y - 10),
                        ImGui::GetColorU32(accent), 1.5f);
                }

                const ImVec2 ts = ImGui::CalcTextSize(name.c_str());
                const ImU32 textCol = isCurrent
                    ? ImGui::GetColorU32(Style::Text)
                    : ImGui::GetColorU32(ImVec4(Style::Text.x, Style::Text.y, Style::Text.z, 0.85f));
                d->AddText(
                    ImVec2(bgMin.x + 18, bgMin.y + 20.0f - ts.y * 0.5f),
                    textCol, name.c_str());

                if (clicked) {
                    ConfigManager::Load(name);
                    strncpy_s(g_configNameBuf, name.c_str(), sizeof(g_configNameBuf) - 1);
                }

                ImGui::Dummy(ImVec2(0, 6));
                ImGui::PopID();
            }
        }
    }

    void Draw(float alpha) {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);

        ImGuiIO& io = ImGui::GetIO();
        ImGui::SetNextWindowPos(
            ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
            ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(
            ImVec2(Style::WindowWidth, Style::WindowHeight),
            ImGuiCond_FirstUseEver);

        ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoSavedSettings;

        ImGui::Begin("##lavender_root", nullptr, flags);

        ImGui::BeginChild("##sidebar", ImVec2(Style::SidebarWidth, 0), false);
        {
            ImGui::Dummy(ImVec2(0, 6));
            ImGui::PushFont(Style::Medium());
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 14);
            ImGui::PushStyleColor(ImGuiCol_Text, Style::Accent);
            ImGui::TextUnformatted("LAVENDER");
            ImGui::PopStyleColor();
            ImGui::PopFont();
            ImGui::Dummy(ImVec2(0, 16));

            const char* titles[] = { "Rage", "Legit", "Visuals", "World", "Misc", "Config" };
            const char* subtitles[] = { "combat ragebot", "aim assist", "visual effects", "world effects", "miscellaneous", "settings" };

            for (int i = 0; i < 6; i++) {
                Widgets::SidebarItem(titles[i], subtitles[i], &g_activeTab, i);
            }
        }
        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_ChildBg, Style::Panel);
        ImGui::BeginChild("##content", ImVec2(0, 0), false);
        {
            ImGui::Dummy(ImVec2(0, 6));
            switch (g_activeTab) {
            case 0: DrawRageTab();    break;
            case 1: DrawLegitTab();   break;
            case 2: DrawVisualsTab(); break;
            case 3: DrawWorldTab();   break;
            case 4: DrawMiscTab();    break;
            case 5: DrawConfigTab();  break;
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();

        ImGui::End();
        ImGui::PopStyleVar();
    }

}