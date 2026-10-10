#include "pch.h"
#include "Triggerbot.h"
#include "../../CombatShared.h"
#include "../../../../input/InputHelper.h"
#include "../../../../../core/hooks/CreateMoveHook.h"
#include "../../../../../config/OffsetConfig.h"
#include "../../../../../render/Renderer.h"
#include "../../../../../core/systems/Tracing.h"
#include <cmath>
#include <atomic>

namespace Triggerbot {

    static volatile LONG g_enabled = 0;
    static volatile LONG g_key = VK_XBUTTON2;
    static volatile LONG g_teamCheck = 1;
    static volatile LONG g_hitboxes = 0x01;
    static volatile LONG g_delay_ms = 60;

    static std::atomic<bool> g_shouldFire{ false };
    static std::atomic<bool> g_externalFire{ false };
    static std::atomic<bool> g_attackCurrentlyWritten{ false };
    static ULONGLONG g_onTargetSince = 0;
    static ULONGLONG g_lastOnTarget = 0;
    static ULONGLONG g_nextFireAllowed = 0;

    static ExternalFireCheckFn g_externalFireCheck = nullptr;

    static constexpr ULONGLONG kOffTargetGraceMs = 300;
    static constexpr ULONGLONG kFireCooldownMs = 80;

    static constexpr uint32_t HB_HEAD = 1u << 0;
    static constexpr uint32_t HB_NECK = 1u << 1;
    static constexpr uint32_t HB_CHEST = 1u << 2;
    static constexpr uint32_t HB_STOMACH = 1u << 3;
    static constexpr uint32_t HB_PELVIS = 1u << 4;
    static constexpr uint32_t HB_ARMS = 1u << 5;
    static constexpr uint32_t HB_LEGS = 1u << 6;

    static void OnCreateMoveInternal(void* cmdPtr) {
        UsercmdLayout* cmd = reinterpret_cast<UsercmdLayout*>(cmdPtr);
        if (!cmd) return;

        bool wantFire = false;

        if (g_enabled != 0 && g_shouldFire.load()) {
            wantFire = true;
            g_shouldFire.store(false);
        }

        if (g_externalFire.load()) {
            wantFire = true;
            g_externalFire.store(false);
        }

        if (!wantFire && g_externalFireCheck && g_externalFireCheck()) {
            wantFire = true;
        }

        if (wantFire) {
            cmd->buttons.value |= 1ull;
            g_attackCurrentlyWritten.store(true);
        }
        else if (g_attackCurrentlyWritten.load()) {
            cmd->buttons.value &= ~1ull;
            g_attackCurrentlyWritten.store(false);
        }
    }

    bool Initialize() {
        CreateMoveHook::Register(&OnCreateMoveInternal);
        return true;
    }

    void Shutdown() {}

    void RequestExternalFire() {
        g_externalFire.store(true);
    }

    void SetExternalFireCheck(ExternalFireCheckFn fn) {
        g_externalFireCheck = fn;
    }

    bool IsEnabled() { return g_enabled != 0; }
    void SetEnabled(bool enabled) {
        InterlockedExchange(&g_enabled, enabled ? 1 : 0);
        if (!enabled) g_shouldFire.store(false);
    }

    int GetKey() { return g_key; }
    void SetKey(int key) { InterlockedExchange(&g_key, key); }

    bool GetTeamCheck() { return g_teamCheck != 0; }
    void SetTeamCheck(bool v) { InterlockedExchange(&g_teamCheck, v ? 1 : 0); }

    uint32_t GetHitboxes() { return (uint32_t)g_hitboxes; }
    void SetHitboxes(uint32_t mask) {
        if (mask == 0) mask = HB_HEAD;
        InterlockedExchange(&g_hitboxes, (LONG)mask);
    }

    int GetDelayMs() { return g_delay_ms; }
    void SetDelayMs(int ms) {
        if (ms < 0) ms = 0;
        if (ms > 2000) ms = 2000;
        InterlockedExchange(&g_delay_ms, ms);
    }

    static bool IsGameFocused() {
        HWND fg = GetForegroundWindow();
        if (!fg) return false;
        DWORD pid = 0;
        GetWindowThreadProcessId(fg, &pid);
        return pid == GetCurrentProcessId();
    }

    static int ReadLocalTeam(uintptr_t pawn) {
        int t = 0;
        __try { t = *reinterpret_cast<int*>(pawn + OffsetConfig::fields.m_iTeamNum); }
        __except (EXCEPTION_EXECUTE_HANDLER) { t = 0; }
        return t;
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

    static bool CheckBone(uintptr_t localPawn, uintptr_t targetPawn,
        const Vec3& bonePos, float cx, float cy, float radius)
    {
        Vec3 screen;
        if (!CombatShared::WorldToScreen(bonePos, screen)) return false;

        const float dx = screen.x - cx;
        const float dy = screen.y - cy;
        if ((dx * dx + dy * dy) >= radius * radius) return false;

        return IsClearToBone(localPawn, targetPawn, bonePos);
    }

    static bool CheckEnemy(uintptr_t localPawn, const CombatShared::Enemy& e,
        uint32_t hb, float cx, float cy)
    {
        constexpr float rHead = 8.0f;
        constexpr float rChest = 14.0f;
        constexpr float rStomach = 12.0f;
        constexpr float rPelvis = 12.0f;
        constexpr float rLimb = 10.0f;

        if ((hb & HB_HEAD) && CheckBone(localPawn, e.pawn, e.headPos, cx, cy, rHead)) return true;
        if ((hb & HB_NECK) && CheckBone(localPawn, e.pawn, e.neckPos, cx, cy, rHead)) return true;

        if (hb & HB_CHEST) {
            if (CheckBone(localPawn, e.pawn, e.chestPos, cx, cy, rChest)) return true;
            if (CheckBone(localPawn, e.pawn, e.stomachPos, cx, cy, rChest)) return true;
        }

        if ((hb & HB_STOMACH) && CheckBone(localPawn, e.pawn, e.stomachPos, cx, cy, rStomach)) return true;
        if ((hb & HB_PELVIS) && CheckBone(localPawn, e.pawn, e.pelvisPos, cx, cy, rPelvis)) return true;

        if (hb & HB_ARMS) {
            if (CheckBone(localPawn, e.pawn, e.leftArmPos, cx, cy, rLimb)) return true;
            if (CheckBone(localPawn, e.pawn, e.rightArmPos, cx, cy, rLimb)) return true;
        }

        if (hb & HB_LEGS) {
            if (CheckBone(localPawn, e.pawn, e.leftLegPos, cx, cy, rLimb)) return true;
            if (CheckBone(localPawn, e.pawn, e.rightLegPos, cx, cy, rLimb)) return true;
        }

        return false;
    }

    void Tick() {
        if (g_enabled == 0) {
            g_shouldFire.store(false);
            g_onTargetSince = 0;
            g_lastOnTarget = 0;
            return;
        }
        if (Renderer::IsMenuOpen()) return;
        if (!IsGameFocused()) return;

        uintptr_t localPawn = CombatShared::GetLocalPawn();
        if (!localPawn) {
            g_shouldFire.store(false);
            g_onTargetSince = 0;
            g_lastOnTarget = 0;
            return;
        }

        if ((GetAsyncKeyState(g_key) & 0x8000) == 0) {
            g_shouldFire.store(false);
            g_onTargetSince = 0;
            g_lastOnTarget = 0;
            return;
        }

        ULONGLONG now = GetTickCount64();

        RECT rect;
        HWND hwnd = GetForegroundWindow();
        if (!hwnd || !GetClientRect(hwnd, &rect)) return;

        float screenW = (float)(rect.right - rect.left);
        float screenH = (float)(rect.bottom - rect.top);
        float cx = screenW * 0.5f;
        float cy = screenH * 0.5f;

        const uint32_t hb = (uint32_t)g_hitboxes;
        const int teamCheck = g_teamCheck;
        const int localTeam = ReadLocalTeam(localPawn);
        const auto& enemies = CombatShared::GetEnemiesCached();

        bool hasTarget = false;
        for (const auto& e : enemies) {
            if (teamCheck != 0 && e.team == localTeam) continue;
            if (CheckEnemy(localPawn, e, hb, cx, cy)) {
                hasTarget = true;
                break;
            }
        }

        if (hasTarget) {
            if (g_onTargetSince == 0) g_onTargetSince = now;
            g_lastOnTarget = now;
        }
        else {
            if (g_lastOnTarget == 0 || (now - g_lastOnTarget) > kOffTargetGraceMs) {
                g_onTargetSince = 0;
                g_lastOnTarget = 0;
                return;
            }
        }

        if (now - g_onTargetSince < (ULONGLONG)g_delay_ms) return;
        if (now < g_nextFireAllowed) return;

        g_shouldFire.store(true);
        g_nextFireAllowed = now + kFireCooldownMs;
    }
}