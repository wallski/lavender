#include "pch.h"
#include "Backtrack.h"
#include "../../combat/CombatShared.h"
#include "../../combat/Lagcomp.h"
#include "../../../../config/OffsetConfig.h"
#include "imgui.h"
#include <cmath>

namespace Backtrack {

    static volatile LONG g_enabled = 0;
    static volatile LONG g_teamCheck = 1;
    static float g_visibleColor[4] = { 0.659f, 0.333f, 0.969f, 1.00f };
    static float g_occludedColor[4] = { 1.000f, 0.400f, 0.400f, 1.00f };
    static float g_thickness = 1.5f;

    static constexpr int kMaxRenderRecords = 8;

    static const int kPairs[][2] = {
        { 7, 6 },
        { 6, 5 },
        { 5, 4 },
        { 4, 3 },
        { 3, 2 },
        { 2, 1 },
        { 6, 8 },
        { 8, 9 },
        { 9, 10 },
        { 6, 12 },
        { 12, 13 },
        { 13, 14 },
        { 1, 16 },
        { 16, 17 },
        { 17, 18 },
        { 1, 19 },
        { 19, 20 },
        { 20, 21 },
    };

    static int ReadTeam(uintptr_t pawn) {
        __try { return *reinterpret_cast<int*>(pawn + OffsetConfig::fields.m_iTeamNum); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
    }

    static ImU32 ToU32(const float c[4]) {
        return ImGui::ColorConvertFloat4ToU32(ImVec4(c[0], c[1], c[2], c[3]));
    }

    static ImU32 FadeU32(ImU32 base, float alphaMul) {
        ImVec4 c = ImGui::ColorConvertU32ToFloat4(base);
        c.w *= alphaMul;
        return ImGui::ColorConvertFloat4ToU32(c);
    }

    void Render() {
        if (g_enabled == 0) return;

        uintptr_t localPawn = CombatShared::GetLocalPawn();
        if (!localPawn) return;

        const auto enemies = CombatShared::GetEnemiesCached();
        if (enemies.empty()) return;

        ImDrawList* dl = ImGui::GetBackgroundDrawList();
        if (!dl) return;

        Vec3 localEye;
        const bool haveEye = CombatShared::GetEyePosition(localPawn, localEye);

        const float thick = g_thickness;
        const ImU32 visCol = ToU32(g_visibleColor);
        const ImU32 occCol = ToU32(g_occludedColor);

        const int localTeam = ReadTeam(localPawn);
        const bool teamCheck = (g_teamCheck != 0);

        for (const auto& e : enemies) {
            if (e.health <= 0) continue;
            if (!e.pawn) continue;
            if (teamCheck && e.team == localTeam) continue;

            const auto snapshots = Lagcomp::GetRenderSnapshots(e.pawn);
            if (snapshots.empty()) continue;

            bool visible = false;
            if (haveEye && e.headPos.Length() > 1.0f) {
                visible = CombatShared::IsVisible(localEye, e.headPos, localPawn, e.pawn);
            }

            const ImU32 baseCol = visible ? visCol : occCol;

            const int total = (int)snapshots.size();
            int limit = total < kMaxRenderRecords ? total : kMaxRenderRecords;
            if (limit <= 0) continue;

            for (int ri = 0; ri < limit; ri++) {
                const auto& snap = snapshots[ri];

                const float t = (limit > 1)
                    ? (float)ri / (float)(limit - 1)
                    : 0.0f;
                const float alphaMul = 1.0f - t * 0.68f;
                const ImU32 faded = FadeU32(baseCol, alphaMul);

                for (auto& pair : kPairs) {
                    const int a = pair[0];
                    const int b = pair[1];
                    if (a >= snap.boneCount || b >= snap.boneCount) continue;
                    if (snap.bonePositions[a].Length() < 1.0f) continue;
                    if (snap.bonePositions[b].Length() < 1.0f) continue;

                    Vec3 sa, sb;
                    if (!CombatShared::WorldToScreen(snap.bonePositions[a], sa)) continue;
                    if (!CombatShared::WorldToScreen(snap.bonePositions[b], sb)) continue;

                    dl->AddLine(ImVec2(sa.x, sa.y), ImVec2(sb.x, sb.y), faded, thick);
                }
            }
        }
    }

    bool IsEnabled() { return g_enabled != 0; }
    void SetEnabled(bool enabled) { InterlockedExchange(&g_enabled, enabled ? 1 : 0); }

    bool IsTeamCheck() { return g_teamCheck != 0; }
    void SetTeamCheck(bool enabled) { InterlockedExchange(&g_teamCheck, enabled ? 1 : 0); }

    float* GetVisibleColor() { return g_visibleColor; }
    float* GetOccludedColor() { return g_occludedColor; }

    void SetVisibleColor(const float color[4]) {
        g_visibleColor[0] = color[0];
        g_visibleColor[1] = color[1];
        g_visibleColor[2] = color[2];
        g_visibleColor[3] = color[3];
    }

    void SetOccludedColor(const float color[4]) {
        g_occludedColor[0] = color[0];
        g_occludedColor[1] = color[1];
        g_occludedColor[2] = color[2];
        g_occludedColor[3] = color[3];
    }

    float GetThickness() { return g_thickness; }
    void SetThickness(float thickness) {
        if (thickness < 0.5f) thickness = 0.5f;
        if (thickness > 5.0f) thickness = 5.0f;
        g_thickness = thickness;
    }
}