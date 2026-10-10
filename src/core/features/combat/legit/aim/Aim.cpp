#include "pch.h"
#include "Aim.h"
#include "../../CombatShared.h"
#include "../triggerbot/Triggerbot.h"
#include "../../../../../config/OffsetConfig.h"
#include "../../../../../render/Renderer.h"
#include "../../../../../core/systems/Tracing.h"
#include <cmath>
#include <atomic>

namespace Aim {

    static volatile LONG g_enabled = 0;
    static volatile LONG g_key = VK_XBUTTON1;
    static volatile LONG g_teamCheck = 1;
    static volatile LONG g_hitboxes = 0x01;
    static float g_fov = 5.0f;
    static volatile LONG g_smooth = 3;
    static volatile LONG g_onAccuracy = 0;
    static float g_accuracySpeed = 5.0f;

    static std::atomic<bool> g_onTargetNow{ false };
    static ULONGLONG g_nextFire = 0;
    static constexpr ULONGLONG kFireCooldownMs = 75;
    static constexpr float kTightAngleDeg = 1.5f;

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

    static bool IsClearToBone(uintptr_t localPawn, uintptr_t targetPawn, const Vec3& bonePos) {
        Vec3 eye;
        if (!CombatShared::GetEyePosition(localPawn, eye)) return false;

        const math::vector3 from{ eye.x, eye.y, eye.z };
        const math::vector3 to{ bonePos.x, bonePos.y, bonePos.z };

        auto filter = Tracing::MakeFilter(localPawn, 0x1c3003, 4);
        auto result = Tracing::Trace(from, to, filter);

        if (result.fraction >= 0.97f) return true;
        if (result.hitEntity == targetPawn) return true;
        return false;
    }

    static bool IsOnGroundAndSlow(uintptr_t localPawn, float threshold) {
        uint32_t flags = 0;
        __try {
            flags = *reinterpret_cast<uint32_t*>(localPawn + OffsetConfig::fields.m_fFlags);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }

        if ((flags & 1) == 0) return false;

        Vec3 vel{};
        __try {
            vel = *reinterpret_cast<Vec3*>(localPawn + OffsetConfig::fields.m_vecAbsVelocity);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }

        const float hSpeed = std::sqrt(vel.x * vel.x + vel.y * vel.y);
        return hSpeed <= threshold;
    }

    static bool OnCreateMoveFireCheck() {
        if (g_enabled == 0 || g_onAccuracy == 0) return false;
        if (!g_onTargetNow.load()) return false;
        if (Renderer::IsMenuOpen()) return false;
        if ((GetAsyncKeyState(g_key) & 0x8000) == 0) return false;

        uintptr_t localPawn = CombatShared::GetLocalPawn();
        if (!localPawn) return false;

        if (!IsOnGroundAndSlow(localPawn, g_accuracySpeed)) return false;

        ULONGLONG now = GetTickCount64();
        if (now < g_nextFire) return false;
        g_nextFire = now + kFireCooldownMs;

        return true;
    }

    void Initialize() {
        Triggerbot::SetExternalFireCheck(&OnCreateMoveFireCheck);
    }

    void Shutdown() {
        Triggerbot::SetExternalFireCheck(nullptr);
    }

    void Tick() {
        if (g_enabled == 0) {
            g_onTargetNow.store(false);
            return;
        }
        if (Renderer::IsMenuOpen()) {
            g_onTargetNow.store(false);
            return;
        }
        if (!IsGameFocused()) {
            g_onTargetNow.store(false);
            return;
        }

        if ((GetAsyncKeyState(g_key) & 0x8000) == 0) {
            g_onTargetNow.store(false);
            return;
        }

        uintptr_t localPawn = CombatShared::GetLocalPawn();
        if (!localPawn) {
            g_onTargetNow.store(false);
            return;
        }

        Vec3 localEye;
        if (!CombatShared::GetEyePosition(localPawn, localEye)) return;

        Vec3 currentAngles;
        if (!ReadViewAngles(currentAngles)) return;

        const auto& enemies = CombatShared::GetEnemiesCached();
        if (enemies.empty()) {
            g_onTargetNow.store(false);
            return;
        }

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

        if (!bestEnemy) {
            g_onTargetNow.store(false);
            return;
        }

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

        Vec3 checkDelta = targetAngles - finalAngles;
        CombatShared::NormalizeAngles(checkDelta);
        const float checkDist = std::sqrt(checkDelta.x * checkDelta.x + checkDelta.y * checkDelta.y);

        if (checkDist > kTightAngleDeg) {
            g_onTargetNow.store(false);
            return;
        }

        if (!IsClearToBone(localPawn, bestEnemy->pawn, bestBone)) {
            g_onTargetNow.store(false);
            return;
        }

        g_onTargetNow.store(true);
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

    bool IsOnAccuracy() { return g_onAccuracy != 0; }
    void SetOnAccuracy(bool enabled) { InterlockedExchange(&g_onAccuracy, enabled ? 1 : 0); }

    float GetAccuracySpeed() { return g_accuracySpeed; }
    void SetAccuracySpeed(float speed) {
        if (speed < 0.5f) speed = 0.5f;
        if (speed > 50.0f) speed = 50.0f;
        g_accuracySpeed = speed;
    }
}