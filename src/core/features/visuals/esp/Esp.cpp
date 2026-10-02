#include "pch.h"
#include "Esp.h"
#include "../../combat/CombatShared.h"
#include "../../../../config/OffsetConfig.h"
#include "imgui.h"
#include <cfloat>

namespace Esp {

    static volatile LONG g_enabled = 1;
    static volatile LONG g_teamCheck = 1;

    static volatile LONG g_boxEnabled = 1;
    static volatile LONG g_boxStyle = 1;
    static volatile LONG g_boxFill = 0;
    static volatile LONG g_boxOutline = 1;
    static volatile LONG g_boxCornerLength = 8;

    static float g_boxVisibleColor[4] = { 0.659f, 0.333f, 0.969f, 1.00f };
    static float g_boxOccludedColor[4] = { 1.000f, 0.400f, 0.400f, 1.00f };
    static float g_boxFillColor[4] = { 0.659f, 0.333f, 0.969f, 0.25f };

    struct Bounds {
        float minX;
        float minY;
        float maxX;
        float maxY;
    };

    static int ReadTeam(uintptr_t entity) {
        __try {
            return *reinterpret_cast<int*>(entity + OffsetConfig::fields.m_iTeamNum);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return 0;
        }
    }

    static bool ReadCollisionBounds(uintptr_t pawn, float* mins, float* maxs) {
        __try {
            const uintptr_t collision = *reinterpret_cast<uintptr_t*>(
                pawn + OffsetConfig::fields.m_pCollision);
            if (collision < 0x10000) return false;

            mins[0] = *reinterpret_cast<float*>(collision + OffsetConfig::fields.m_vecMins + 0);
            mins[1] = *reinterpret_cast<float*>(collision + OffsetConfig::fields.m_vecMins + 4);
            mins[2] = *reinterpret_cast<float*>(collision + OffsetConfig::fields.m_vecMins + 8);
            maxs[0] = *reinterpret_cast<float*>(collision + OffsetConfig::fields.m_vecMaxs + 0);
            maxs[1] = *reinterpret_cast<float*>(collision + OffsetConfig::fields.m_vecMaxs + 4);
            maxs[2] = *reinterpret_cast<float*>(collision + OffsetConfig::fields.m_vecMaxs + 8);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    static bool ComputeBounds(uintptr_t pawn, const Vec3& origin, Bounds& out) {
        if (pawn < 0x10000) return false;

        float mins[3];
        float maxs[3];
        if (!ReadCollisionBounds(pawn, mins, maxs)) return false;
        if (maxs[2] - mins[2] < 1.0f) return false;

        const Vec3 corners[8] = {
            { origin.x + mins[0], origin.y + mins[1], origin.z + mins[2] },
            { origin.x + maxs[0], origin.y + mins[1], origin.z + mins[2] },
            { origin.x + mins[0], origin.y + maxs[1], origin.z + mins[2] },
            { origin.x + maxs[0], origin.y + maxs[1], origin.z + mins[2] },
            { origin.x + mins[0], origin.y + mins[1], origin.z + maxs[2] },
            { origin.x + maxs[0], origin.y + mins[1], origin.z + maxs[2] },
            { origin.x + mins[0], origin.y + maxs[1], origin.z + maxs[2] },
            { origin.x + maxs[0], origin.y + maxs[1], origin.z + maxs[2] },
        };

        float minX = FLT_MAX;
        float minY = FLT_MAX;
        float maxX = -FLT_MAX;
        float maxY = -FLT_MAX;
        int valid = 0;

        for (int i = 0; i < 8; i++) {
            Vec3 screen;
            if (CombatShared::WorldToScreen(corners[i], screen)) {
                if (screen.x < minX) minX = screen.x;
                if (screen.y < minY) minY = screen.y;
                if (screen.x > maxX) maxX = screen.x;
                if (screen.y > maxY) maxY = screen.y;
                valid++;
            }
        }

        if (valid < 6) return false;
        if (maxX - minX < 1.0f || maxY - minY < 1.0f) return false;
        if (maxX - minX > 2000.0f || maxY - minY > 2000.0f) return false;

        out.minX = minX;
        out.minY = minY;
        out.maxX = maxX;
        out.maxY = maxY;
        return true;
    }

    static void DrawFullBox(ImDrawList* dl, const Bounds& b, ImU32 color, float thickness) {
        dl->AddRect(ImVec2(b.minX, b.minY), ImVec2(b.maxX, b.maxY), color, 0.0f, 0, thickness);
    }

    static void DrawCornerSegment(ImDrawList* dl,
        float ax, float ay, float bx, float by,
        ImU32 color, ImU32 outlineColor, bool outlined, float thickness)
    {
        if (outlined) {
            dl->AddLine(ImVec2(ax, ay), ImVec2(bx, by), outlineColor, thickness + 2.0f);
        }
        dl->AddLine(ImVec2(ax, ay), ImVec2(bx, by), color, thickness);
    }

    static void DrawCornerBox(ImDrawList* dl, const Bounds& b,
        ImU32 color, ImU32 outlineColor, bool outlined,
        float cornerLen, float thickness)
    {
        const float x0 = b.minX;
        const float y0 = b.minY;
        const float x1 = b.maxX;
        const float y1 = b.maxY;
        const float w = x1 - x0;
        const float h = y1 - y0;
        const float cl = (std::min)(cornerLen, (std::min)(w, h) * 0.4f);

        DrawCornerSegment(dl, x0, y0, x0 + cl, y0, color, outlineColor, outlined, thickness);
        DrawCornerSegment(dl, x0, y0, x0, y0 + cl, color, outlineColor, outlined, thickness);

        DrawCornerSegment(dl, x1 - cl, y0, x1, y0, color, outlineColor, outlined, thickness);
        DrawCornerSegment(dl, x1, y0, x1, y0 + cl, color, outlineColor, outlined, thickness);

        DrawCornerSegment(dl, x0, y1 - cl, x0, y1, color, outlineColor, outlined, thickness);
        DrawCornerSegment(dl, x0, y1, x0 + cl, y1, color, outlineColor, outlined, thickness);

        DrawCornerSegment(dl, x1 - cl, y1, x1, y1, color, outlineColor, outlined, thickness);
        DrawCornerSegment(dl, x1, y1 - cl, x1, y1, color, outlineColor, outlined, thickness);
    }

    void Render() {
        if (g_enabled == 0) return;
        if (g_boxEnabled == 0) return;

        uintptr_t localPawn = CombatShared::GetLocalPawn();
        if (!localPawn) return;

        const int localTeam = ReadTeam(localPawn);

        auto enemies = CombatShared::GetEnemies();
        if (enemies.empty()) return;

        ImDrawList* dl = ImGui::GetBackgroundDrawList();
        if (!dl) return;

        for (const auto& e : enemies) {
            if (g_teamCheck != 0 && e.team == localTeam) continue;

            Bounds b;
            if (!ComputeBounds(e.pawn, e.origin, b)) continue;

            const float* col = e.visible ? g_boxVisibleColor : g_boxOccludedColor;
            const ImU32 color = IM_COL32(
                (int)(col[0] * 255.0f),
                (int)(col[1] * 255.0f),
                (int)(col[2] * 255.0f),
                (int)(col[3] * 255.0f));

            if (g_boxFill != 0) {
                const ImU32 fill = IM_COL32(
                    (int)(g_boxFillColor[0] * 255.0f),
                    (int)(g_boxFillColor[1] * 255.0f),
                    (int)(g_boxFillColor[2] * 255.0f),
                    (int)(g_boxFillColor[3] * 255.0f));
                dl->AddRectFilled(
                    ImVec2(b.minX + 1.0f, b.minY + 1.0f),
                    ImVec2(b.maxX - 1.0f, b.maxY - 1.0f),
                    fill);
            }

            const ImU32 outlineCol = IM_COL32(0, 0, 0, 200);
            const bool outlined = (g_boxOutline != 0);

            if (g_boxStyle == 0) {
                if (outlined) {
                    dl->AddRect(
                        ImVec2(b.minX - 1.0f, b.minY - 1.0f),
                        ImVec2(b.maxX + 1.0f, b.maxY + 1.0f),
                        outlineCol, 0.0f, 0, 3.0f);
                }
                DrawFullBox(dl, b, color, 1.0f);
            }
            else {
                DrawCornerBox(dl, b, color, outlineCol, outlined,
                    (float)g_boxCornerLength, 1.0f);
            }
        }
    }

    void Shutdown() {}

    bool IsEnabled() { return g_enabled != 0; }
    void SetEnabled(bool enabled) { InterlockedExchange(&g_enabled, enabled ? 1 : 0); }

    bool IsTeamCheck() { return g_teamCheck != 0; }
    void SetTeamCheck(bool enabled) { InterlockedExchange(&g_teamCheck, enabled ? 1 : 0); }

    bool IsBoxEnabled() { return g_boxEnabled != 0; }
    void SetBoxEnabled(bool enabled) { InterlockedExchange(&g_boxEnabled, enabled ? 1 : 0); }

    int GetBoxStyle() { return g_boxStyle; }
    void SetBoxStyle(int style) {
        if (style < 0) style = 0;
        if (style > 1) style = 1;
        InterlockedExchange(&g_boxStyle, style);
    }

    bool IsBoxFillEnabled() { return g_boxFill != 0; }
    void SetBoxFillEnabled(bool enabled) { InterlockedExchange(&g_boxFill, enabled ? 1 : 0); }

    bool IsBoxOutlineEnabled() { return g_boxOutline != 0; }
    void SetBoxOutlineEnabled(bool enabled) { InterlockedExchange(&g_boxOutline, enabled ? 1 : 0); }

    float GetBoxCornerLength() { return (float)g_boxCornerLength; }
    void SetBoxCornerLength(float length) {
        if (length < 1.0f) length = 1.0f;
        if (length > 30.0f) length = 30.0f;
        InterlockedExchange(&g_boxCornerLength, (LONG)length);
    }

    float* GetBoxVisibleColor() { return g_boxVisibleColor; }
    float* GetBoxFillColor() { return g_boxFillColor; }

    void SetBoxVisibleColor(const float color[4]) {
        g_boxVisibleColor[0] = color[0];
        g_boxVisibleColor[1] = color[1];
        g_boxVisibleColor[2] = color[2];
        g_boxVisibleColor[3] = color[3];
    }

    void SetBoxFillColor(const float color[4]) {
        g_boxFillColor[0] = color[0];
        g_boxFillColor[1] = color[1];
        g_boxFillColor[2] = color[2];
        g_boxFillColor[3] = color[3];
    }

}