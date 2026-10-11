#include "pch.h"
#include "Rage.h"
#include "NoSpread.h"
#include "core/features/combat/CombatShared.h"
#include "core/systems/Tracing.h"
#include "core/hooks/CreateMoveHook.h"
#include "core/features/combat/legit/triggerbot/UsercmdLayout.h"
#include "core/systems/schemas/Schemas.h"
#include "config/OffsetConfig.h"
#include "render/Renderer.h"
#include "../../../math/math.hpp"
#include "../../../math/hash.hpp"
#include "core/memory/PatternScan.h"
#include "core/memory/Proto.h"
#include <atomic>
#include <mutex>
#include <cmath>

namespace Rage {

    static volatile LONG g_enabled = 0;
    static volatile LONG g_teamCheck = 1;
    static volatile LONG g_hitboxes = 0x1F;
    static volatile LONG g_minDamage = 1;
    static volatile LONG g_silent = 1;
    static volatile LONG g_autoShoot = 1;
    static volatile LONG g_noSpread = 0;

    static std::atomic<bool> g_rageAttackWritten{ false };

    static uintptr_t g_getInaccuracyFn = 0;
    static uintptr_t g_updateAccuracyFn = 0;

    static constexpr uint32_t HB_HEAD = 1u << 0;
    static constexpr uint32_t HB_NECK = 1u << 1;
    static constexpr uint32_t HB_CHEST = 1u << 2;
    static constexpr uint32_t HB_STOMACH = 1u << 3;
    static constexpr uint32_t HB_PELVIS = 1u << 4;
    static constexpr uint32_t HB_ARMS = 1u << 5;
    static constexpr uint32_t HB_LEGS = 1u << 6;

    struct HitboxEntry {
        uint32_t flag;
        int boneIndex;
        int priority;
        int hitgroup;
    };

    static const HitboxEntry kHitboxes[] = {
        { HB_HEAD,    7,  100, 1 },
        { HB_NECK,    6,  90,  8 },
        { HB_CHEST,   3,  50,  2 },
        { HB_STOMACH, 2,  40,  3 },
        { HB_PELVIS,  1,  40,  3 },
        { HB_ARMS,    9,  20,  4 },
        { HB_ARMS,    13, 20,  5 },
        { HB_LEGS,    17, 10,  6 },
        { HB_LEGS,    20, 10,  7 },
    };

    struct InputHistoryEntryLayout {
        proto::has_bits m_has_bits;
        uint32_t m_cached_size;
        proto::msg_qangle* m_view_angles;
        void* m_cl_interp;
        void* m_sv_interp0;
        void* m_sv_interp1;
        void* m_player_interp;
        void* m_shoot_position;
        void* m_target_head_pos_check;
        void* m_target_abs_pos_check;
        void* m_target_abs_ang_check;
        int32_t m_render_tick_count;
        float m_render_tick_fraction;
        int32_t m_player_tick_count;
        float m_player_tick_fraction;
        int32_t m_frame_number;
        int32_t m_target_ent_index;
    };

    struct WeaponState {
        uintptr_t weapon = 0;
        uintptr_t vdata = 0;
        int itemDefIdx = 0;
        int numBullets = 1;
        int damage = 0;
        float recoilIndex = 0.0f;
        int shotsFired = 0;
        float inaccuracy = 0.0f;
        float spread = 0.0f;
        float armorRatio = 0.5f;
        float headshotMultiplier = 4.0f;
        uint32_t weaponType = 0;
        bool valid = false;
    };

    static WeaponState g_cachedWs{};
    static uint32_t g_cachedWsHandle = 0;

    static int g_accCacheTick = -1;
    static WeaponState g_accCacheWs{};

    static uint32_t g_tickBaseOff = 0;
    static uint32_t g_attrMgrOff = 0;
    static uint32_t g_attrItemOff = 0;
    static uint32_t g_itemDefIdxOff = 0;
    static uint32_t g_punchPredOff = 0;
    static uint32_t g_punchUnpredOff = 0;
    static uint32_t g_recoilIndexOff = 0;
    static uint32_t g_shotsFiredOff = 0;
    static uint32_t g_spreadOff = 0;

    static bool SafeReadPtr(uintptr_t addr, uintptr_t* out) {
        __try { *out = *reinterpret_cast<uintptr_t*>(addr); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        return true;
    }

    static bool SafeReadI32(uintptr_t addr, int32_t* out) {
        __try { *out = *reinterpret_cast<int32_t*>(addr); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        return true;
    }

    static bool SafeReadU16(uintptr_t addr, uint16_t* out) {
        __try { *out = *reinterpret_cast<uint16_t*>(addr); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        return true;
    }

    static bool SafeReadU32(uintptr_t addr, uint32_t* out) {
        __try { *out = *reinterpret_cast<uint32_t*>(addr); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        return true;
    }

    static bool SafeReadF32(uintptr_t addr, float* out) {
        __try { *out = *reinterpret_cast<float*>(addr); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        return true;
    }

    static int ReadTickBase(uintptr_t controller) {
        if (controller < 0x10000 || !g_tickBaseOff) return 0;
        int32_t tb = 0;
        __try { tb = *reinterpret_cast<int32_t*>(controller + g_tickBaseOff); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
        return tb;
    }

    static void InitializeSchemaOffsets() {
        g_tickBaseOff = SCHEMA("CBasePlayerController", "m_nTickBase"_hash);
        g_attrMgrOff = SCHEMA("C_EconEntity", "m_AttributeManager"_hash);
        g_attrItemOff = SCHEMA("C_AttributeContainer", "m_Item"_hash);
        g_itemDefIdxOff = SCHEMA("C_EconItemView", "m_iItemDefinitionIndex"_hash);
        g_punchPredOff = SCHEMA("CCSPlayer_AimPunchServices", "m_predictableBaseAngle"_hash);
        g_punchUnpredOff = SCHEMA("CCSPlayer_AimPunchServices", "m_unpredictableBaseAngle"_hash);
        g_recoilIndexOff = SCHEMA("C_CSWeaponBase", "m_flRecoilIndex"_hash);
        g_shotsFiredOff = SCHEMA("C_CSPlayerPawn", "m_iShotsFired"_hash);
        g_spreadOff = SCHEMA("CCSWeaponBaseVData", "m_flSpread"_hash);

        printf("[Rage] schema: tickBase=0x%X attrMgr=0x%X attrItem=0x%X itemDef=0x%X punchPred=0x%X punchUnpred=0x%X recoilIdx=0x%X shotsFired=0x%X spread=0x%X\n",
            g_tickBaseOff, g_attrMgrOff, g_attrItemOff, g_itemDefIdxOff,
            g_punchPredOff, g_punchUnpredOff, g_recoilIndexOff, g_shotsFiredOff, g_spreadOff);
        fflush(stdout);
    }

    static void InitializeAccuracyPatterns() {
        g_getInaccuracyFn = Memory::FindPattern("client.dll",
            "48 89 5C 24 ? 55 56 57 48 81 EC ? ? ? ? 44 0F 29 84 24");

        g_updateAccuracyFn = Memory::FindPattern("client.dll",
            "40 57 41 56 48 83 EC 68 48 8B F9 E8 ? ? ? ? 4C 8B F0 48 85 C0");

        printf("[Rage] accuracy: getInaccuracy=0x%llX updateAccuracy=0x%llX\n",
            (unsigned long long)g_getInaccuracyFn,
            (unsigned long long)g_updateAccuracyFn);
        fflush(stdout);
    }

    static float CallGetInaccuracy(uintptr_t weapon) {
        if (!g_getInaccuracyFn || weapon < 0x10000) return 0.0f;
        using Fn = float(*)(uintptr_t, float*, float*);
        auto f = reinterpret_cast<Fn>(g_getInaccuracyFn);
        __try { return f(weapon, nullptr, nullptr); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return 0.0f; }
    }

    static void CallUpdateAccuracy(uintptr_t weapon) {
        if (!g_updateAccuracyFn || weapon < 0x10000) return;
        using Fn = void(*)(uintptr_t);
        auto f = reinterpret_cast<Fn>(g_updateAccuracyFn);
        __try { f(weapon); }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    static bool IsGameFocused() {
        HWND fg = GetForegroundWindow();
        if (!fg) return false;
        DWORD pid = 0;
        GetWindowThreadProcessId(fg, &pid);
        return pid == GetCurrentProcessId();
    }

    static int ReadTeam(uintptr_t pawn) {
        __try { return *reinterpret_cast<int*>(pawn + OffsetConfig::fields.m_iTeamNum); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
    }

    static bool ReadViewAngles(Vec3& out) {
        uintptr_t va = OffsetConfig::globals.dwViewAngles;
        if (!va) return false;
        __try { out = *reinterpret_cast<Vec3*>(va); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        return true;
    }

    static void WriteClientViewAngles(const math::vector3& angles) {
        uintptr_t va = OffsetConfig::globals.dwViewAngles;
        if (!va) return;
        __try { *reinterpret_cast<Vec3*>(va) = Vec3(angles.x, angles.y, 0.0f); }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    static proto::base_usercmd_pb* GetProtobufBase(UsercmdLayout* cmd) {
        if (!cmd) return nullptr;
        uintptr_t wrapper = reinterpret_cast<uintptr_t>(cmd->pb.base);
        if (wrapper < 0x10000) return nullptr;
        return reinterpret_cast<proto::base_usercmd_pb*>(wrapper + 0x10);
    }

    static InputHistoryEntryLayout* GetInputHistoryEntry(UsercmdLayout* cmd, int index) {
        if (!cmd) return nullptr;
        auto& hist = cmd->pb.input_history;
        if (!hist.rep) return nullptr;
        if (index < 0 || index >= hist.current_size) return nullptr;
        void* raw = hist.rep->elements[index];
        if (!raw) return nullptr;
        uintptr_t addr = reinterpret_cast<uintptr_t>(raw);
        if (addr < 0x10000) return nullptr;
        return reinterpret_cast<InputHistoryEntryLayout*>(addr + 0x10);
    }

    static void ReadItemDefIdx(uintptr_t weapon, int* out) {
        *out = 0;
        if (weapon < 0x10000) return;
        if (!g_attrMgrOff || !g_attrItemOff || !g_itemDefIdxOff) return;

        const uintptr_t addr = weapon + g_attrMgrOff + g_attrItemOff + g_itemDefIdxOff;
        uint16_t v = 0;
        if (!SafeReadU16(addr, &v)) return;
        *out = (int)v;
    }

    static void BackupRestoreAccuracy(uintptr_t weapon, bool doUpdateAndRead,
        float* outInaccuracy)
    {
        *outInaccuracy = 0.0f;
        if (weapon < 0x10000) return;

        const uint32_t begin = SCHEMA("C_CSWeaponBase", "m_flTurningInaccuracyDelta"_hash);
        const uint32_t end = SCHEMA("C_CSWeaponBase", "m_flRecoilIndex"_hash);
        if (!begin || !end || end < begin) return;

        const size_t size = (size_t)(end - begin) + sizeof(float);
        if (size == 0 || size > 0x100) return;

        uint8_t backup[0x100] = {};
        __try {
            memcpy(backup, (const void*)(weapon + begin), size);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { return; }

        if (doUpdateAndRead) {
            CallUpdateAccuracy(weapon);
            *outInaccuracy = CallGetInaccuracy(weapon);
        }

        __try {
            memcpy((void*)(weapon + begin), backup, size);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    static uint32_t GetWeaponHandle(uintptr_t localPawn) {
        if (localPawn < 0x10000) return 0;
        uintptr_t wservices = 0;
        if (!SafeReadPtr(localPawn + OffsetConfig::fields.m_pWeaponServices, &wservices)) return 0;
        if (wservices < 0x10000) return 0;
        uint32_t h = 0;
        if (!SafeReadU32(wservices + OffsetConfig::fields.m_hActiveWeapon, &h)) return 0;
        return h;
    }

    static WeaponState BuildWeaponState(uintptr_t localPawn, uint32_t weaponHandle,
        bool updateAccuracy)
    {
        WeaponState ws{};
        if (localPawn < 0x10000 || !weaponHandle) return ws;

        uintptr_t weapon = CombatShared::GetEntityByHandle(weaponHandle);
        if (weapon < 0x10000) return ws;
        ws.weapon = weapon;

        uintptr_t vdata = 0;
        if (!SafeReadPtr(weapon + OffsetConfig::fields.m_nSubclassID + 0x8, &vdata)) return ws;
        if (vdata < 0x10000) return ws;
        ws.vdata = vdata;

        const uint32_t dmgOff = SCHEMA("CCSWeaponBaseVData", "m_nDamage"_hash);
        const uint32_t armorOff = SCHEMA("CCSWeaponBaseVData", "m_flArmorRatio"_hash);
        const uint32_t hsOff = SCHEMA("CCSWeaponBaseVData", "m_flHeadshotMultiplier"_hash);
        const uint32_t wtOff = SCHEMA("CCSWeaponBaseVData", "m_WeaponType"_hash);
        const uint32_t bulletsOff = SCHEMA("CCSWeaponBaseVData", "m_nNumBullets"_hash);

        if (dmgOff) SafeReadI32(vdata + dmgOff, &ws.damage);
        if (armorOff) SafeReadF32(vdata + armorOff, &ws.armorRatio);
        if (hsOff) SafeReadF32(vdata + hsOff, &ws.headshotMultiplier);
        if (wtOff) SafeReadU32(vdata + wtOff, &ws.weaponType);
        if (bulletsOff) SafeReadI32(vdata + bulletsOff, &ws.numBullets);
        if (g_recoilIndexOff) SafeReadF32(weapon + g_recoilIndexOff, &ws.recoilIndex);
        if (g_shotsFiredOff) SafeReadI32(localPawn + g_shotsFiredOff, &ws.shotsFired);
        if (g_spreadOff) SafeReadF32(vdata + g_spreadOff, &ws.spread);

        if (ws.damage <= 0) ws.damage = 30;
        if (ws.armorRatio <= 0.0f) ws.armorRatio = 0.5f;
        if (ws.headshotMultiplier <= 0.0f) ws.headshotMultiplier = 4.0f;
        if (ws.numBullets <= 0) ws.numBullets = 1;

        ReadItemDefIdx(weapon, &ws.itemDefIdx);

        if (updateAccuracy) {
            BackupRestoreAccuracy(weapon, true, &ws.inaccuracy);
        }
        else {
            ws.inaccuracy = CallGetInaccuracy(weapon);
        }

        ws.valid = true;
        return ws;
    }

    static WeaponState GetWeaponState(uintptr_t localPawn, bool forceRefresh) {
        const uint32_t handle = GetWeaponHandle(localPawn);
        if (!handle) return {};

        if (!forceRefresh && handle == g_cachedWsHandle && g_cachedWs.valid) {
            return g_cachedWs;
        }

        WeaponState ws = BuildWeaponState(localPawn, handle, false);
        g_cachedWs = ws;
        g_cachedWsHandle = handle;
        return ws;
    }

    static WeaponState GetWeaponStateWithAccuracy(uintptr_t localPawn, int tickBase) {
        if (tickBase > 0 && tickBase == g_accCacheTick && g_accCacheWs.valid) {
            return g_accCacheWs;
        }

        const uint32_t handle = GetWeaponHandle(localPawn);
        if (!handle) return {};

        WeaponState ws = BuildWeaponState(localPawn, handle, true);
        g_accCacheWs = ws;
        g_accCacheTick = tickBase;
        return ws;
    }

    static int EstimateDamage(const WeaponState& ws, int hitgroup) {
        if (!ws.valid || ws.damage <= 0) return 0;

        float dmg = (float)ws.damage;

        switch (hitgroup) {
        case 1: dmg *= ws.headshotMultiplier; break;
        case 3: dmg *= 1.25f; break;
        case 6:
        case 7: dmg *= 0.75f; break;
        default: break;
        }

        return (int)dmg;
    }

    struct ScanResult {
        uintptr_t pawn = 0;
        math::vector3 angle{};
        int hitgroup = 0;
        bool valid = false;
    };

    static ScanResult DoScan(uintptr_t localPawn, const WeaponState& ws,
        const Vec3& eye, const Vec3& curAngles)
    {
        ScanResult out{};

        const math::vector3 eyeM{ eye.x, eye.y, eye.z };
        const Vec3 curV{ curAngles.x, curAngles.y, 0.0f };

        const auto enemies = CombatShared::GetEnemiesCached();
        const int localTeam = ReadTeam(localPawn);
        const uint32_t mask = (uint32_t)g_hitboxes;
        const float maxFov = 180.0f;
        const int teamCheck = g_teamCheck;
        const int minDamage = g_minDamage;

        float bestFov = maxFov + 0.001f;
        int bestPriority = -1;

        for (const auto& e : enemies) {
            if (e.health <= 0) continue;
            if (teamCheck && e.team == localTeam) continue;
            if (e.headPos.Length() < 1.0f) continue;

            const size_t count = sizeof(kHitboxes) / sizeof(kHitboxes[0]);
            for (size_t i = 0; i < count; i++) {
                const auto& hb = kHitboxes[i];
                if (!(mask & hb.flag)) continue;
                if (hb.boneIndex < 0 || hb.boneIndex >= 32) continue;

                const Vec3& bone = e.bones[hb.boneIndex];
                if (bone.Length() < 1.0f) continue;

                const Vec3 aimV = CombatShared::CalculateAngle(
                    Vec3(eye.x, eye.y, eye.z), bone);
                const float fov = CombatShared::AngleDistance(curV, aimV);

                if (fov > maxFov) continue;

                if (minDamage > 1) {
                    if (!ws.valid) continue;
                    const int est = EstimateDamage(ws, hb.hitgroup);
                    if (est < minDamage) continue;
                }

                const math::vector3 boneM{ bone.x, bone.y, bone.z };

                auto filter = Tracing::MakeFilter(localPawn, 0x1c3003, 4);
                auto result = Tracing::Trace(eyeM, boneM, filter);

                if (result.fraction < 0.97f && result.hitEntity != e.pawn) continue;

                if (hb.priority > bestPriority ||
                    (hb.priority == bestPriority && fov < bestFov)) {
                    bestPriority = hb.priority;
                    bestFov = fov;
                    out.pawn = e.pawn;
                    out.angle = math::vector3{ aimV.x, aimV.y, 0.0f };
                    out.hitgroup = hb.hitgroup;
                    out.valid = true;
                }

                break;
            }
        }

        return out;
    }

    static float EffectiveRecoil(const WeaponState& ws) {
        if (ws.recoilIndex > 0.01f) return ws.recoilIndex;
        if (ws.shotsFired > 0) return (float)ws.shotsFired;
        return 0.0f;
    }

    static void OnCreateMoveInternal(void* cmdPtr) {
        UsercmdLayout* cmd = reinterpret_cast<UsercmdLayout*>(cmdPtr);
        if (!cmd) return;

        const bool wasAttacking = g_rageAttackWritten.load();

        if (g_enabled == 0 || Renderer::IsMenuOpen() || !IsGameFocused()) {
            if (wasAttacking) {
                cmd->buttons.value &= ~1ull;
                cmd->buttons.value_changed |= 1ull;
                g_rageAttackWritten.store(false);
            }
            return;
        }

        uintptr_t localPawn = CombatShared::GetLocalPawn();
        if (!localPawn) {
            if (wasAttacking) {
                cmd->buttons.value &= ~1ull;
                cmd->buttons.value_changed |= 1ull;
                g_rageAttackWritten.store(false);
            }
            return;
        }

        Vec3 eye;
        if (!CombatShared::GetEyePosition(localPawn, eye)) {
            if (wasAttacking) {
                cmd->buttons.value &= ~1ull;
                cmd->buttons.value_changed |= 1ull;
                g_rageAttackWritten.store(false);
            }
            return;
        }

        Vec3 curAngles;
        if (!ReadViewAngles(curAngles)) {
            if (wasAttacking) {
                cmd->buttons.value &= ~1ull;
                cmd->buttons.value_changed |= 1ull;
                g_rageAttackWritten.store(false);
            }
            return;
        }

        WeaponState ws = GetWeaponState(localPawn, false);

        ScanResult res = DoScan(localPawn, ws, eye, curAngles);

        const bool autoShoot = (g_autoShoot != 0);
        const bool wantFire = autoShoot && res.valid;

        const math::vector3 userAngleVec{ curAngles.x, curAngles.y, 0.0f };
        const bool silent = (g_silent != 0);
        math::vector3 writeAngle = res.valid ? res.angle : userAngleVec;

        if (wantFire && g_noSpread != 0) {
            const uintptr_t lc = CombatShared::GetLocalController();
            const int tickBase = ReadTickBase(lc);

            WeaponState wsAcc = GetWeaponStateWithAccuracy(localPawn, tickBase);

            if (wsAcc.valid && tickBase > 0) {
                const float recoil = EffectiveRecoil(wsAcc);

                if (recoil > 0.5f || wsAcc.inaccuracy > 0.001f || wsAcc.spread > 0.001f) {
                    const math::vector3 corrected = NoSpread::Correct(
                        writeAngle, tickBase + 1,
                        wsAcc.itemDefIdx, wsAcc.numBullets,
                        wsAcc.inaccuracy, wsAcc.spread, recoil);

                    if (corrected.x != 0.0f || corrected.y != 0.0f || corrected.z != 0.0f) {
                        writeAngle = corrected;
                    }
                }
            }
        }

        auto* base = GetProtobufBase(cmd);
        if (base && res.valid) {
            if (auto* va = base->mutable_viewangles()) {
                if (silent) {
                    va->set_x(userAngleVec.x);
                    va->set_y(userAngleVec.y);
                    va->set_z(0.0f);
                }
                else {
                    va->set_x(writeAngle.x);
                    va->set_y(writeAngle.y);
                    va->set_z(writeAngle.z);
                }
            }
        }

        if (res.valid) {
            const int historySize = cmd->pb.input_history.current_size;
            for (int i = 0; i < historySize; ++i) {
                auto* entry = GetInputHistoryEntry(cmd, i);
                if (!entry) continue;

                uintptr_t rawVa = reinterpret_cast<uintptr_t>(entry->m_view_angles);
                if (rawVa < 0x10000) continue;

                auto* va = reinterpret_cast<proto::msg_qangle*>(rawVa + 0x10);
                va->set_x(writeAngle.x);
                va->set_y(writeAngle.y);
                va->set_z(writeAngle.z);
            }

            if (!silent) {
                WriteClientViewAngles(writeAngle);
            }
        }

        if (wantFire) {
            cmd->buttons.value |= 1ull;
            cmd->buttons.value_changed |= 1ull;
            cmd->buttons.value_scroll |= 1ull;

            const int historySize = cmd->pb.input_history.current_size;
            if (historySize > 0) {
                cmd->pb.has_bits.bits |= (1u << 6);
                cmd->pb.attack1_start_history_index = historySize - 1;
            }

            g_rageAttackWritten.store(true);
        }
        else if (wasAttacking) {
            cmd->buttons.value &= ~1ull;
            cmd->buttons.value_changed |= 1ull;
            cmd->buttons.value_scroll |= 1ull;
            g_rageAttackWritten.store(false);
        }
    }

    void Initialize() {
        InitializeSchemaOffsets();
        InitializeAccuracyPatterns();
        NoSpread::Initialize();
        CreateMoveHook::Register(&OnCreateMoveInternal);
    }

    void Shutdown() {
        g_rageAttackWritten.store(false);
        NoSpread::Shutdown();
        g_getInaccuracyFn = 0;
        g_updateAccuracyFn = 0;
        g_accCacheTick = -1;
    }

    void Tick() {}

    bool IsEnabled() { return g_enabled != 0; }
    void SetEnabled(bool enabled) {
        InterlockedExchange(&g_enabled, enabled ? 1 : 0);
        if (!enabled) g_rageAttackWritten.store(false);
    }

    bool IsTeamCheck() { return g_teamCheck != 0; }
    void SetTeamCheck(bool enabled) { InterlockedExchange(&g_teamCheck, enabled ? 1 : 0); }

    uint32_t GetHitboxes() { return (uint32_t)g_hitboxes; }
    void SetHitboxes(uint32_t mask) {
        if (mask == 0) mask = HB_HEAD;
        InterlockedExchange(&g_hitboxes, (LONG)mask);
    }

    float GetFov() { return 180.0f; }
    void SetFov(float) {}

    int GetMinDamage() { return g_minDamage; }
    void SetMinDamage(int damage) {
        if (damage < 1) damage = 1;
        if (damage > 130) damage = 130;
        InterlockedExchange(&g_minDamage, damage);
    }

    bool IsSilent() { return g_silent != 0; }
    void SetSilent(bool enabled) { InterlockedExchange(&g_silent, enabled ? 1 : 0); }

    bool IsAutoShoot() { return g_autoShoot != 0; }
    void SetAutoShoot(bool enabled) { InterlockedExchange(&g_autoShoot, enabled ? 1 : 0); }

    bool IsNoSpread() { return g_noSpread != 0; }
    void SetNoSpread(bool enabled) { InterlockedExchange(&g_noSpread, enabled ? 1 : 0); }

    bool IsAutoScope() { return false; }
    void SetAutoScope(bool) {}
}