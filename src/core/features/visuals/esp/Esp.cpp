#include "pch.h"
#include "Esp.h"
#include "../../combat/CombatShared.h"
#include "../../../../config/OffsetConfig.h"
#include "imgui.h"
#include <cfloat>
#include <cstdio>

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

    static volatile LONG g_healthEnabled = 0;
    static volatile LONG g_healthPosition = 0;
    static volatile LONG g_healthStyle = 0;
    static float g_healthColor[4] = { 0.659f, 0.333f, 0.969f, 1.00f };
    static float g_healthLowColor[4] = { 1.00f, 0.25f, 0.25f, 1.00f };
    static float g_healthHighColor[4] = { 0.30f, 1.00f, 0.30f, 1.00f };
    static volatile LONG g_healthShowValue = 1;
    static volatile LONG g_healthThickness = 3;

    static volatile LONG g_ammoEnabled = 0;
    static volatile LONG g_ammoPosition = 2;
    static float g_ammoColor[4] = { 1.00f, 1.00f, 1.00f, 1.00f };
    static volatile LONG g_ammoShowValue = 1;
    static volatile LONG g_ammoThickness = 3;

    static volatile LONG g_nameEnabled = 0;
    static volatile LONG g_namePosition = 0;
    static float g_nameColor[4] = { 1.00f, 1.00f, 1.00f, 1.00f };

    struct Bounds {
        float minX;
        float minY;
        float maxX;
        float maxY;
    };

    struct SideOffsets {
        float left;
        float right;
        float top;
        float bottom;
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

    static ImU32 ToU32(const float c[4]) {
        return ImGui::ColorConvertFloat4ToU32(ImVec4(c[0], c[1], c[2], c[3]));
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

    static void DrawBox(ImDrawList* dl, const Bounds& b, const CombatShared::Enemy& e) {
        const float* col = e.visible ? g_boxVisibleColor : g_boxOccludedColor;
        const ImU32 color = ToU32(col);

        if (g_boxFill != 0) {
            dl->AddRectFilled(
                ImVec2(b.minX + 1.0f, b.minY + 1.0f),
                ImVec2(b.maxX - 1.0f, b.maxY - 1.0f),
                ToU32(g_boxFillColor));
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

    static void DrawOutlinedText(ImDrawList* dl, float x, float y, const char* text, ImU32 color) {
        dl->AddText(ImVec2(x + 1.0f, y + 1.0f), IM_COL32(0, 0, 0, 220), text);
        dl->AddText(ImVec2(x - 1.0f, y + 1.0f), IM_COL32(0, 0, 0, 220), text);
        dl->AddText(ImVec2(x + 1.0f, y - 1.0f), IM_COL32(0, 0, 0, 220), text);
        dl->AddText(ImVec2(x - 1.0f, y - 1.0f), IM_COL32(0, 0, 0, 220), text);
        dl->AddText(ImVec2(x, y), color, text);
    }

    static void DrawHealthBar(ImDrawList* dl, const Bounds& b,
        const CombatShared::Enemy& e, SideOffsets& off)
    {
        if (g_healthEnabled == 0) return;
        if (e.health <= 0) return;

        const float thickness = (float)g_healthThickness;
        const float padding = 4.0f;
        const int pos = (int)g_healthPosition;

        int hp = e.health;
        if (hp > 100) hp = 100;
        const float frac = (float)hp / 100.0f;

        const ImU32 staticCol = ToU32(g_healthColor);
        const ImU32 lowCol = ToU32(g_healthLowColor);
        const ImU32 highCol = ToU32(g_healthHighColor);
        const bool gradient = (g_healthStyle == 1);

        float barX, barY, barW, barH;
        if (pos == 0) {
            barW = thickness;
            barH = b.maxY - b.minY;
            barX = b.minX - padding - barW - off.left;
            barY = b.minY;
            off.left += barW + padding;
        }
        else if (pos == 1) {
            barW = b.maxX - b.minX;
            barH = thickness;
            barX = b.minX;
            barY = b.minY - padding - barH - off.top;
            off.top += barH + padding;
        }
        else {
            barW = b.maxX - b.minX;
            barH = thickness;
            barX = b.minX;
            barY = b.maxY + padding + off.bottom;
            off.bottom += barH + padding;
        }

        dl->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + barW, barY + barH),
            IM_COL32(0, 0, 0, 140));

        if (pos == 0) {
            const float fillH = barH * frac;
            const float fy0 = barY + barH - fillH;
            const float fy1 = barY + barH;
            if (gradient) {
                dl->AddRectFilledMultiColor(
                    ImVec2(barX, fy0), ImVec2(barX + barW, fy1),
                    highCol, highCol, lowCol, lowCol);
            }
            else {
                dl->AddRectFilled(ImVec2(barX, fy0), ImVec2(barX + barW, fy1), staticCol);
            }
        }
        else {
            const float fillW = barW * frac;
            if (gradient) {
                dl->AddRectFilledMultiColor(
                    ImVec2(barX, barY), ImVec2(barX + fillW, barY + barH),
                    lowCol, highCol, highCol, lowCol);
            }
            else {
                dl->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + fillW, barY + barH), staticCol);
            }
        }

        if (g_healthShowValue != 0) {
            char buf[8];
            snprintf(buf, sizeof(buf), "%d", hp);
            const ImVec2 ts = ImGui::CalcTextSize(buf);
            float tx, ty;
            if (pos == 0) {
                const float fillH = barH * frac;
                tx = barX + barW + 2.0f;
                ty = barY + barH - fillH - ts.y;
            }
            else if (pos == 1) {
                const float fillW = barW * frac;
                tx = barX + fillW - ts.x - 4.0f;
                ty = barY - ts.y - 1.0f;
            }
            else {
                const float fillW = barW * frac;
                tx = barX + fillW - ts.x - 4.0f;
                ty = barY + barH + 1.0f;
            }
            DrawOutlinedText(dl, tx, ty, buf, IM_COL32(255, 255, 255, 255));
        }
    }

    static void DrawAmmoBar(ImDrawList* dl, const Bounds& b,
        const CombatShared::Enemy& e, SideOffsets& off)
    {
        if (g_ammoEnabled == 0) return;
        if (!e.hasWeapon || e.maxAmmo <= 0) return;

        const float thickness = (float)g_ammoThickness;
        const float padding = 4.0f;
        const int pos = (int)g_ammoPosition;

        int clip = e.ammo;
        if (clip < 0) clip = 0;
        if (clip > e.maxAmmo) clip = e.maxAmmo;
        const float frac = (float)clip / (float)e.maxAmmo;

        const ImU32 color = ToU32(g_ammoColor);

        float barX, barY, barW, barH;
        if (pos == 0) {
            barW = thickness;
            barH = b.maxY - b.minY;
            barX = b.minX - padding - barW - off.left;
            barY = b.minY;
            off.left += barW + padding;
        }
        else if (pos == 1) {
            barW = b.maxX - b.minX;
            barH = thickness;
            barX = b.minX;
            barY = b.minY - padding - barH - off.top;
            off.top += barH + padding;
        }
        else {
            barW = b.maxX - b.minX;
            barH = thickness;
            barX = b.minX;
            barY = b.maxY + padding + off.bottom;
            off.bottom += barH + padding;
        }

        dl->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + barW, barY + barH),
            IM_COL32(0, 0, 0, 140));

        if (pos == 0) {
            const float fillH = barH * frac;
            const float fy0 = barY + barH - fillH;
            const float fy1 = barY + barH;
            dl->AddRectFilled(ImVec2(barX, fy0), ImVec2(barX + barW, fy1), color);
        }
        else {
            const float fillW = barW * frac;
            dl->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + fillW, barY + barH), color);
        }

        if (g_ammoShowValue != 0) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%d", clip);
            const ImVec2 ts = ImGui::CalcTextSize(buf);
            float tx, ty;
            if (pos == 0) {
                const float fillH = barH * frac;
                tx = barX + barW + 2.0f;
                ty = barY + barH - fillH - ts.y;
            }
            else if (pos == 1) {
                const float fillW = barW * frac;
                tx = barX + fillW - ts.x - 4.0f;
                ty = barY - ts.y - 1.0f;
            }
            else {
                const float fillW = barW * frac;
                tx = barX + fillW - ts.x - 4.0f;
                ty = barY + barH + 1.0f;
            }
            DrawOutlinedText(dl, tx, ty, buf, IM_COL32(255, 255, 255, 255));
        }
    }

    static void DrawName(ImDrawList* dl, const Bounds& b,
        const CombatShared::Enemy& e, SideOffsets& off)
    {
        if (g_nameEnabled == 0) return;
        if (e.name.empty()) return;

        const int pos = (int)g_namePosition;

        const ImVec2 ts = ImGui::CalcTextSize(e.name.c_str());
        const float x = b.minX + ((b.maxX - b.minX) * 0.5f) - (ts.x * 0.5f);

        float y;
        if (pos == 0) {
            y = b.minY - 2.0f - ts.y - off.top;
            off.top += ts.y + 2.0f;
        }
        else {
            y = b.maxY + 2.0f + off.bottom;
            off.bottom += ts.y + 2.0f;
        }

        DrawOutlinedText(dl, x, y, e.name.c_str(), ToU32(g_nameColor));
    }

    void Render() {
        if (g_enabled == 0) return;

        uintptr_t localPawn = CombatShared::GetLocalPawn();
        if (!localPawn) return;

        const int localTeam = ReadTeam(localPawn);

        const auto& enemies = CombatShared::GetEnemiesCached();
        if (enemies.empty()) return;

        ImDrawList* dl = ImGui::GetBackgroundDrawList();
        if (!dl) return;

        for (const auto& e : enemies) {
            if (g_teamCheck != 0 && e.team == localTeam) continue;

            Bounds b;
            if (!ComputeBounds(e.pawn, e.origin, b)) continue;

            SideOffsets off{ 0.0f, 0.0f, 0.0f, 0.0f };

            if (g_boxEnabled != 0) DrawBox(dl, b, e);
            DrawHealthBar(dl, b, e, off);
            DrawAmmoBar(dl, b, e, off);
            DrawName(dl, b, e, off);
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

    bool IsHealthEnabled() { return g_healthEnabled != 0; }
    void SetHealthEnabled(bool enabled) { InterlockedExchange(&g_healthEnabled, enabled ? 1 : 0); }

    int GetHealthPosition() { return g_healthPosition; }
    void SetHealthPosition(int position) {
        if (position < 0) position = 0;
        if (position > 2) position = 2;
        InterlockedExchange(&g_healthPosition, position);
    }

    int GetHealthStyle() { return g_healthStyle; }
    void SetHealthStyle(int style) {
        if (style < 0) style = 0;
        if (style > 1) style = 1;
        InterlockedExchange(&g_healthStyle, style);
    }

    float* GetHealthColor() { return g_healthColor; }
    float* GetHealthLowColor() { return g_healthLowColor; }
    float* GetHealthHighColor() { return g_healthHighColor; }

    void SetHealthColor(const float color[4]) {
        g_healthColor[0] = color[0];
        g_healthColor[1] = color[1];
        g_healthColor[2] = color[2];
        g_healthColor[3] = color[3];
    }

    void SetHealthLowColor(const float color[4]) {
        g_healthLowColor[0] = color[0];
        g_healthLowColor[1] = color[1];
        g_healthLowColor[2] = color[2];
        g_healthLowColor[3] = color[3];
    }

    void SetHealthHighColor(const float color[4]) {
        g_healthHighColor[0] = color[0];
        g_healthHighColor[1] = color[1];
        g_healthHighColor[2] = color[2];
        g_healthHighColor[3] = color[3];
    }

    bool IsHealthShowValue() { return g_healthShowValue != 0; }
    void SetHealthShowValue(bool enabled) { InterlockedExchange(&g_healthShowValue, enabled ? 1 : 0); }

    float GetHealthThickness() { return (float)g_healthThickness; }
    void SetHealthThickness(float thickness) {
        if (thickness < 1.0f) thickness = 1.0f;
        if (thickness > 6.0f) thickness = 6.0f;
        InterlockedExchange(&g_healthThickness, (LONG)thickness);
    }

    bool IsAmmoEnabled() { return g_ammoEnabled != 0; }
    void SetAmmoEnabled(bool enabled) { InterlockedExchange(&g_ammoEnabled, enabled ? 1 : 0); }

    int GetAmmoPosition() { return g_ammoPosition; }
    void SetAmmoPosition(int position) {
        if (position < 0) position = 0;
        if (position > 2) position = 2;
        InterlockedExchange(&g_ammoPosition, position);
    }

    float* GetAmmoColor() { return g_ammoColor; }
    void SetAmmoColor(const float color[4]) {
        g_ammoColor[0] = color[0];
        g_ammoColor[1] = color[1];
        g_ammoColor[2] = color[2];
        g_ammoColor[3] = color[3];
    }

    bool IsAmmoShowValue() { return g_ammoShowValue != 0; }
    void SetAmmoShowValue(bool enabled) { InterlockedExchange(&g_ammoShowValue, enabled ? 1 : 0); }

    float GetAmmoThickness() { return (float)g_ammoThickness; }
    void SetAmmoThickness(float thickness) {
        if (thickness < 1.0f) thickness = 1.0f;
        if (thickness > 6.0f) thickness = 6.0f;
        InterlockedExchange(&g_ammoThickness, (LONG)thickness);
    }

    bool IsNameEnabled() { return g_nameEnabled != 0; }
    void SetNameEnabled(bool enabled) { InterlockedExchange(&g_nameEnabled, enabled ? 1 : 0); }

    int GetNamePosition() { return g_namePosition; }
    void SetNamePosition(int position) {
        if (position < 0) position = 0;
        if (position > 1) position = 1;
        InterlockedExchange(&g_namePosition, position);
    }

    float* GetNameColor() { return g_nameColor; }
    void SetNameColor(const float color[4]) {
        g_nameColor[0] = color[0];
        g_nameColor[1] = color[1];
        g_nameColor[2] = color[2];
        g_nameColor[3] = color[3];
    }

}