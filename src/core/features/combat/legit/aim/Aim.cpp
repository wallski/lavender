#include "pch.h"
#include "Aim.h"
#include "../../CombatShared.h"
#include "../../../../../config/OffsetConfig.h"
#include "../../../../../render/Renderer.h"
#include <cmath>

namespace Aim {

    static volatile LONG g_enabled = 0;
    static volatile LONG g_key = VK_XBUTTON1;
    static volatile LONG g_teamCheck = 1;
    static volatile LONG g_hitboxes = 0x01;
    static float g_fov = 5.0f;
    static volatile LONG g_smooth = 3;

    static constexpr uint32_t HB_HEAD = 1u << 0;
    static constexpr uint32_t HB_NECK = 1u << 1;
    static constexpr uint32_t HB_CHEST = 1u << 2;
    static constexpr uint32_t HB_STOMACH = 1u << 3;
    static constexpr uint32_t HB_PELVIS = 1u << 4;

    static bool IsGameFocused() {
        HWND fg = GetForegroundWindow();
        if (!fg) return false;
        DWORD pid = 0;
        GetWindowThreadProcessId(fg, &pid);
        return pid == GetCurrentProcessId();
    }

    static bool ReadViewAngles(Vec3& out) {
        if (!OffsetConfig::globals.dwViewAngles) return false;
        __try {
            out = *reinterpret_cast<Vec3*>(OffsetConfig::globals.dwViewAngles);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
        return std::isfinite(out.x) && std::isfinite(out.y);
    }

    static void WriteViewAngles(const Vec3& angles) {
        if (!OffsetConfig::globals.dwViewAngles) return;
        __try {
            *reinterpret_cast<Vec3*>(OffsetConfig::globals.dwViewAngles) = angles;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    static int ReadLocalTeam(uintptr_t pawn) {
        __try {
            return *reinterpret_cast<int*>(pawn + OffsetConfig::fields.m_iTeamNum);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return 0;
        }
    }

    static bool PickBone(const CombatShared::Enemy& e, uint32_t mask, Vec3& out) {
        if ((mask & HB_HEAD) && e.headPos.Length() > 1.0f) { out = e.headPos; return true; }
        if ((mask & HB_NECK) && e.neckPos.Length() > 1.0f) { out = e.neckPos; return true; }
        if ((mask & HB_CHEST) && e.chestPos.Length() > 1.0f) { out = e.chestPos; return true; }
        if ((mask & HB_STOMACH) && e.stomachPos.Length() > 1.0f) { out = e.stomachPos; return true; }
        if ((mask & HB_PELVIS) && e.pelvisPos.Length() > 1.0f) { out = e.pelvisPos; return true; }
        return false;
    }

    void Initialize() {}
    void Shutdown() {}

    void Tick() {
        if (g_enabled == 0) return;
        if (Renderer::IsMenuOpen()) return;
        if (!IsGameFocused()) return;

        if ((GetAsyncKeyState(g_key) & 0x8000) == 0) return;

        uintptr_t localPawn = CombatShared::GetLocalPawn();
        if (!localPawn) return;

        Vec3 localEye;
        if (!CombatShared::GetEyePosition(localPawn, localEye)) return;

        Vec3 currentAngles;
        if (!ReadViewAngles(currentAngles)) return;

        const auto& enemies = CombatShared::GetEnemiesCached();
        if (enemies.empty()) return;

        const uint32_t mask = (uint32_t)g_hitboxes;
        const float maxFov = g_fov;
        const int teamCheck = g_teamCheck;
        const int localTeam = ReadLocalTeam(localPawn);

        const CombatShared::Enemy* bestEnemy = nullptr;
        Vec3 bestBone;
        float bestFov = maxFov + 0.001f;

        for (const auto& e : enemies) {
            if (e.health <= 0) continue;
            if (teamCheck && e.team == localTeam) continue;

            Vec3 bone;
            if (!PickBone(e, mask, bone)) continue;

            const Vec3 aimAngles = CombatShared::CalculateAngle(localEye, bone);
            const float fov = CombatShared::AngleDistance(currentAngles, aimAngles);
            if (fov >= bestFov) continue;

            bestFov = fov;
            bestEnemy = &e;
            bestBone = bone;
        }

        if (!bestEnemy) return;

        const Vec3 targetAngles = CombatShared::CalculateAngle(localEye, bestBone);

        const int smooth = g_smooth;
        Vec3 finalAngles = targetAngles;
        if (smooth > 1) {
            Vec3 delta = targetAngles - currentAngles;
            CombatShared::NormalizeAngles(delta);
            const float factor = 1.0f / (float)smooth;
            finalAngles.x = currentAngles.x + delta.x * factor;
            finalAngles.y = currentAngles.y + delta.y * factor;
            finalAngles.z = 0.0f;
            CombatShared::NormalizeAngles(finalAngles);
        }

        WriteViewAngles(finalAngles);
    }

    bool IsEnabled() { return g_enabled != 0; }
    void SetEnabled(bool enabled) { InterlockedExchange(&g_enabled, enabled ? 1 : 0); }

    int GetKey() { return g_key; }
    void SetKey(int key) { InterlockedExchange(&g_key, key); }

    bool IsTeamCheck() { return g_teamCheck != 0; }
    void SetTeamCheck(bool enabled) { InterlockedExchange(&g_teamCheck, enabled ? 1 : 0); }

    float GetFov() { return g_fov; }
    void SetFov(float fov) {
        if (fov < 0.1f) fov = 0.1f;
        if (fov > 180.0f) fov = 180.0f;
        g_fov = fov;
    }

    int GetSmooth() { return g_smooth; }
    void SetSmooth(int smooth) {
        if (smooth < 1) smooth = 1;
        if (smooth > 60) smooth = 60;
        InterlockedExchange(&g_smooth, smooth);
    }

    uint32_t GetHitboxes() { return (uint32_t)g_hitboxes; }
    void SetHitboxes(uint32_t mask) {
        if (mask == 0) mask = HB_HEAD;
        InterlockedExchange(&g_hitboxes, (LONG)mask);
    }
}