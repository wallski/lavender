#include "pch.h"
#include "BombTimer.h"
#include "../../../../config/OffsetConfig.h"
#include "../../../../config/GeneratedOffsets.h"
#include "../../../memory/PatternScan.h"
#include "../../combat/CombatShared.h"
#include "imgui.h"
#include <cstdio>

namespace BombTimer {

    static volatile LONG g_enabled = 0;
    static volatile LONG g_defuseEnabled = 1;
    static volatile LONG g_siteEnabled = 1;
    static float g_scale = 1.0f;
    static float g_color[4] = { 1.0f, 0.25f, 0.25f, 1.0f };
    static float g_defuseColor[4] = { 0.25f, 0.85f, 0.35f, 1.0f };

    static uintptr_t g_c4GlobalAddr = 0;

    static constexpr uint32_t kCurTimeOffset = 0x00;

    struct State {
        bool valid;
        bool beingDefused;
        int site;
        float timeRemaining;
        float defuseRemaining;
        Vec3 worldPos;
    };

    static State g_state = {};

    static bool SafeReadU8(uintptr_t addr, uint8_t* out) {
        *out = 0;
        __try { *out = *reinterpret_cast<uint8_t*>(addr); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        return true;
    }

    static bool SafeReadI32(uintptr_t addr, int32_t* out) {
        *out = 0;
        __try { *out = *reinterpret_cast<int32_t*>(addr); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        return true;
    }

    static bool SafeReadF32(uintptr_t addr, float* out) {
        *out = 0.0f;
        __try { *out = *reinterpret_cast<float*>(addr); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        return true;
    }

    static bool SafeReadPtr(uintptr_t addr, uintptr_t* out) {
        *out = 0;
        __try { *out = *reinterpret_cast<uintptr_t*>(addr); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        return true;
    }

    static bool SafeReadVec3(uintptr_t addr, Vec3* out) {
        *out = Vec3();
        __try { *out = *reinterpret_cast<Vec3*>(addr); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        return true;
    }

    static bool ResolveC4Global() {
        const char* sig = "48 8B 1D ? ? ? ? 48 8B D3 4C 8B 81";

        uintptr_t match = Memory::FindPattern("client.dll", sig);
        if (!match) return false;

        int32_t rel = 0;
        if (!SafeReadI32(match + 3, &rel)) return false;

        uintptr_t globalAddr = match + 7 + (intptr_t)rel;

        g_c4GlobalAddr = globalAddr;
        return true;
    }

    void Initialize() {
        g_state = {};
        ResolveC4Global();
    }

    void Shutdown() {
        g_c4GlobalAddr = 0;
        g_state = {};
    }

    bool IsEnabled() { return g_enabled != 0; }
    void SetEnabled(bool enabled) { InterlockedExchange(&g_enabled, enabled ? 1 : 0); }

    bool IsDefuseEnabled() { return g_defuseEnabled != 0; }
    void SetDefuseEnabled(bool enabled) { InterlockedExchange(&g_defuseEnabled, enabled ? 1 : 0); }

    bool IsSiteEnabled() { return g_siteEnabled != 0; }
    void SetSiteEnabled(bool enabled) { InterlockedExchange(&g_siteEnabled, enabled ? 1 : 0); }

    float GetScale() { return g_scale; }
    void SetScale(float scale) {
        if (scale < 0.5f) scale = 0.5f;
        if (scale > 3.0f) scale = 3.0f;
        g_scale = scale;
    }

    float* GetColor() { return g_color; }
    void SetColor(const float color[4]) {
        g_color[0] = color[0];
        g_color[1] = color[1];
        g_color[2] = color[2];
        g_color[3] = color[3];
    }

    float* GetDefuseColor() { return g_defuseColor; }
    void SetDefuseColor(const float color[4]) {
        g_defuseColor[0] = color[0];
        g_defuseColor[1] = color[1];
        g_defuseColor[2] = color[2];
        g_defuseColor[3] = color[3];
    }

    static float ReadCurtime() {
        uintptr_t globalVars = 0;
        if (!SafeReadPtr(OffsetConfig::globals.dwGlobalVars, &globalVars)) return 0.0f;
        if (globalVars < 0x10000) return 0.0f;

        float curtime = 0.0f;
        SafeReadF32(globalVars + kCurTimeOffset, &curtime);
        return curtime;
    }

    void Update() {
        g_state.valid = false;

        if (g_enabled == 0) return;
        if (g_c4GlobalAddr == 0) return;

        uintptr_t localPawn = CombatShared::GetLocalPawn();
        if (!localPawn) return;

        uintptr_t c4Entity = 0;
        if (!SafeReadPtr(g_c4GlobalAddr, &c4Entity)) return;
        if (c4Entity < 0x10000) return;

        uint8_t ticking = 0;
        if (!SafeReadU8(c4Entity + GeneratedOffsets::C4::m_bBombTicking, &ticking)) return;
        if (ticking == 0) return;

        uint8_t exploded = 0, defused = 0;
        SafeReadU8(c4Entity + GeneratedOffsets::C4::m_bHasExploded, &exploded);
        SafeReadU8(c4Entity + GeneratedOffsets::C4::m_bBombDefused, &defused);
        if (exploded != 0 || defused != 0) return;

        float c4Blow = 0.0f;
        if (!SafeReadF32(c4Entity + GeneratedOffsets::C4::m_flC4Blow, &c4Blow)) return;

        float curtime = ReadCurtime();
        if (curtime <= 0.0f) return;

        float timeRemaining = c4Blow - curtime;
        if (timeRemaining <= 0.0f) return;

        int32_t site = 0;
        SafeReadI32(c4Entity + GeneratedOffsets::C4::m_nBombSite, &site);

        uint8_t beingDefused = 0;
        SafeReadU8(c4Entity + GeneratedOffsets::C4::m_bBeingDefused, &beingDefused);

        float defuseRemaining = 0.0f;
        if (beingDefused != 0) {
            float defuseCountDown = 0.0f;
            if (SafeReadF32(c4Entity + GeneratedOffsets::C4::m_flDefuseCountDown, &defuseCountDown)) {
                defuseRemaining = defuseCountDown - curtime;
            }
        }

        Vec3 worldPos;
        uintptr_t sceneNode = 0;
        if (SafeReadPtr(c4Entity + OffsetConfig::fields.m_pGameSceneNode, &sceneNode) &&
            sceneNode >= 0x10000) {
            SafeReadVec3(sceneNode + OffsetConfig::fields.m_vecAbsOrigin, &worldPos);
            worldPos.z += 8.0f;
        }
        else {
            worldPos = Vec3();
        }

        g_state.valid = true;
        g_state.beingDefused = (beingDefused != 0);
        g_state.site = site;
        g_state.timeRemaining = timeRemaining;
        g_state.defuseRemaining = defuseRemaining;
        g_state.worldPos = worldPos;
    }

    static void DrawOutlinedText(ImDrawList* dl, ImFont* font, float size,
        ImVec2 pos, ImU32 color, const char* text)
    {
        ImU32 outline = IM_COL32(0, 0, 0, 220);
        dl->AddText(font, size, ImVec2(pos.x + 1.0f, pos.y + 1.0f), outline, text);
        dl->AddText(font, size, ImVec2(pos.x - 1.0f, pos.y + 1.0f), outline, text);
        dl->AddText(font, size, ImVec2(pos.x + 1.0f, pos.y - 1.0f), outline, text);
        dl->AddText(font, size, ImVec2(pos.x - 1.0f, pos.y - 1.0f), outline, text);
        dl->AddText(font, size, pos, color, text);
    }

    void Render() {
        if (g_enabled == 0) return;
        if (!g_state.valid) return;

        uintptr_t localPawn = CombatShared::GetLocalPawn();
        if (!localPawn) return;

        Vec3 screen;
        if (!CombatShared::WorldToScreen(g_state.worldPos, screen)) return;
        if (screen.x < -100.0f || screen.y < -100.0f) return;

        ImDrawList* dl = ImGui::GetBackgroundDrawList();
        if (!dl) return;

        ImFont* font = ImGui::GetFont();
        const float timerSize = 18.0f * g_scale;
        const float subSize = 14.0f * g_scale;

        char timer[32];
        snprintf(timer, sizeof(timer), "%.2f", g_state.timeRemaining);

        bool willSucceed = g_state.beingDefused && (g_state.defuseRemaining < g_state.timeRemaining);
        const float* activeColor = willSucceed ? g_defuseColor : g_color;
        ImU32 timerColor = ImGui::ColorConvertFloat4ToU32(
            ImVec4(activeColor[0], activeColor[1], activeColor[2], activeColor[3]));

        ImVec2 timerTextSize = font->CalcTextSizeA(timerSize, FLT_MAX, 0.0f, timer);
        float tx = screen.x - timerTextSize.x * 0.5f;
        float ty = screen.y - timerTextSize.y;
        DrawOutlinedText(dl, font, timerSize, ImVec2(tx, ty), timerColor, timer);

        float nextY = ty + timerTextSize.y + 1.0f * g_scale;

        if (g_siteEnabled != 0) {
            char site[4];
            site[0] = (g_state.site == 1) ? 'B' : 'A';
            site[1] = '\0';

            ImU32 siteColor = IM_COL32(255, 255, 255, 255);
            ImVec2 sSize = font->CalcTextSizeA(subSize, FLT_MAX, 0.0f, site);
            float sx = screen.x - sSize.x * 0.5f;
            DrawOutlinedText(dl, font, subSize, ImVec2(sx, nextY), siteColor, site);
            nextY += sSize.y + 1.0f * g_scale;
        }

        if (g_defuseEnabled != 0 && g_state.beingDefused) {
            char defuseText[32];
            snprintf(defuseText, sizeof(defuseText), "DEFUSE %.2f", g_state.defuseRemaining);

            ImU32 defuseColor = willSucceed
                ? ImGui::ColorConvertFloat4ToU32(ImVec4(g_defuseColor[0], g_defuseColor[1], g_defuseColor[2], 1.0f))
                : ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 0.3f, 0.3f, 1.0f));

            ImVec2 dSize = font->CalcTextSizeA(subSize, FLT_MAX, 0.0f, defuseText);
            float dx = screen.x - dSize.x * 0.5f;
            DrawOutlinedText(dl, font, subSize, ImVec2(dx, nextY), defuseColor, defuseText);
        }
    }
}