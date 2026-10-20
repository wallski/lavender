#include "pch.h"
#include "Rage.h"
#include "NoSpread.h"
#include "../Penetration.h"
#include "core/features/combat/CombatShared.h"
#include "core/features/combat/Lagcomp.h"
#include "core/systems/Tracing.h"
#include "core/systems/systems.hpp"
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

    struct WeaponSettings {
        volatile LONG enabled = 0;
        volatile LONG autoShoot = 1;
        volatile LONG silent = 1;
        volatile LONG noSpread = 0;
        volatile LONG autowall = 0;
        volatile LONG teamCheck = 1;
        volatile LONG backtrack = 1;
        volatile LONG minDamage = 1;
        volatile LONG hitboxes = 0x1F;
        volatile LONG pointscale = 50;
    };

    static WeaponSettings g_weapons[(int)Group::Count];

    static std::atomic<bool> g_rageAttackWritten{ false };
    static uintptr_t g_getInaccuracyFn = 0;
    static uintptr_t g_updateAccuracyFn = 0;

    static constexpr int kMaxAutowallCallsPerTick = 20;
    static int g_autowallCallsThisTick = 0;

    static constexpr uint32_t HB_HEAD = 1u << 0;
    static constexpr uint32_t HB_NECK = 1u << 1;
    static constexpr uint32_t HB_CHEST = 1u << 2;
    static constexpr uint32_t HB_STOMACH = 1u << 3;
    static constexpr uint32_t HB_PELVIS = 1u << 4;
    static constexpr uint32_t HB_ARMS = 1u << 5;
    static constexpr uint32_t HB_LEGS = 1u << 6;

    struct HitboxScan {
        uint32_t flag;
        int hitboxIndex;
        int priority;
    };

    static const HitboxScan kScanOrder[] = {
        { HB_HEAD,    0,  100 },
        { HB_NECK,    1,  90  },
        { HB_CHEST,   4,  50  },
        { HB_CHEST,   5,  50  },
        { HB_CHEST,   6,  50  },
        { HB_STOMACH, 3,  40  },
        { HB_PELVIS,  2,  40  },
        { HB_LEGS,    7,  10  },
        { HB_LEGS,    8,  10  },
        { HB_LEGS,    9,  10  },
        { HB_LEGS,    10, 10  },
        { HB_LEGS,    11, 10  },
        { HB_LEGS,    12, 10  },
        { HB_ARMS,    13, 20  },
        { HB_ARMS,    14, 20  },
        { HB_ARMS,    15, 20  },
        { HB_ARMS,    16, 20  },
        { HB_ARMS,    17, 20  },
        { HB_ARMS,    18, 20  },
    };

    static constexpr size_t kScanOrderCount = sizeof(kScanOrder) / sizeof(kScanOrder[0]);
    static constexpr int kMaxViewBones = 32;
    static constexpr int kMaxViews = 16;

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
    static uint32_t g_recoilIndexOff = 0;
    static uint32_t g_shotsFiredOff = 0;
    static uint32_t g_spreadOff = 0;
    static uint32_t g_nextPrimaryTickOff = 0;

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

    static bool SafeReadU8(uintptr_t addr, uint8_t* out) {
        __try { *out = *reinterpret_cast<uint8_t*>(addr); }
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

    static Group GroupFromWeaponType(uint32_t wt) {
        switch (wt) {
        case 1: return Group::Pistol;
        case 2: return Group::SMG;
        case 3: return Group::Rifle;
        case 4: return Group::Shotgun;
        case 5: return Group::Sniper;
        case 6: return Group::LMG;
        default: return Group::Rifle;
        }
    }

    static void InitializeSchemaOffsets() {
        g_tickBaseOff = SCHEMA("CBasePlayerController", "m_nTickBase"_hash);
        g_attrMgrOff = SCHEMA("C_EconEntity", "m_AttributeManager"_hash);
        g_attrItemOff = SCHEMA("C_AttributeContainer", "m_Item"_hash);
        g_itemDefIdxOff = SCHEMA("C_EconItemView", "m_iItemDefinitionIndex"_hash);
        g_recoilIndexOff = SCHEMA("C_CSWeaponBase", "m_flRecoilIndex"_hash);
        g_shotsFiredOff = SCHEMA("C_CSPlayerPawn", "m_iShotsFired"_hash);
        g_spreadOff = SCHEMA("CCSWeaponBaseVData", "m_flSpread"_hash);
        g_nextPrimaryTickOff = SCHEMA("C_BasePlayerWeapon", "m_nNextPrimaryAttackTick"_hash);

        printf("[Rage] schema: tickBase=0x%X attrMgr=0x%X attrItem=0x%X itemDef=0x%X recoilIdx=0x%X shotsFired=0x%X spread=0x%X nextPrimary=0x%X wt=0x%X bullets=0x%X\n",
            g_tickBaseOff, g_attrMgrOff, g_attrItemOff, g_itemDefIdxOff,
            g_recoilIndexOff, g_shotsFiredOff, g_spreadOff, g_nextPrimaryTickOff,
            SCHEMA("CCSWeaponBaseVData", "m_WeaponType"_hash),
            SCHEMA("CCSWeaponBaseVData", "m_nNumBullets"_hash));
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

    static int ReadArmor(uintptr_t pawn) {
        int a = 0;
        __try { a = *reinterpret_cast<int*>(pawn + SCHEMA("C_CSPlayerPawn", "m_ArmorValue"_hash)); }
        __except (EXCEPTION_EXECUTE_HANDLER) { a = 0; }
        return a;
    }

    static bool ReadHelmet(uintptr_t pawn, int armor) {
        if (armor <= 0) return false;
        uintptr_t services = 0;
        __try { services = *reinterpret_cast<uintptr_t*>(pawn + SCHEMA("C_BasePlayerPawn", "m_pItemServices"_hash)); }
        __except (EXCEPTION_EXECUTE_HANDLER) { services = 0; }
        if (services < 0x10000) return false;
        uint8_t helmet = 0;
        SafeReadU8(services + SCHEMA("CCSPlayer_ItemServices", "m_bHasHelmet"_hash), &helmet);
        return helmet != 0;
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

    static float EstimateDamage(int baseDamage, int hitgroup) {
        float dmg = (float)baseDamage;
        switch (hitgroup) {
        case 1: dmg *= 4.0f; break;
        case 3: dmg *= 1.25f; break;
        case 6:
        case 7: dmg *= 0.75f; break;
        default: break;
        }
        return dmg;
    }

    static bool RayVsCapsuleLocal(const math::vector3& ray_origin, const math::vector3& ray_dir,
        const math::vector3& capsule_a, const math::vector3& capsule_b,
        float radius, float& out_fraction)
    {
        return Penetration::RayVsCapsule(ray_origin, ray_dir, capsule_a, capsule_b, radius, out_fraction);
    }

    static math::vector3 MathVec(const Vec3& v) {
        return math::vector3{ v.x, v.y, v.z };
    }

    static math::quaternion MathQuat(const Quat& q) {
        math::quaternion out{};
        out.x = q.x;
        out.y = q.y;
        out.z = q.z;
        out.w = q.w;
        return out;
    }

    static std::vector<math::vector3> GenerateMultipoints(
        const systems::hitboxes::entry& hitbox,
        const math::vector3& center,
        const math::quaternion& bone_rot,
        float pointscale,
        const math::vector3& shoot_pos,
        float inaccuracy,
        float spread)
    {
        std::vector<math::vector3> out;

        float scale = pointscale / 100.0f;
        if (scale < 0.0f) scale = 0.0f;
        if (scale > 1.0f) scale = 1.0f;
        if (scale <= 0.01f) return out;

        const math::vector3 hb_mid = (hitbox.mins + hitbox.maxs) * 0.5f;
        const math::vector3 capsule_a = center + bone_rot.rotate_vector(hitbox.mins - hb_mid);
        const math::vector3 capsule_b = center + bone_rot.rotate_vector(hitbox.maxs - hb_mid);

        if (hitbox.radius > 0.001f) {
            const float cone = (inaccuracy + spread > 0.0f) ? (inaccuracy + spread) : 0.0f;
            const float cone_radius = std::tanf(cone) * (center - shoot_pos).length();
            const float automatic_scale = 0.9f - cone_radius / hitbox.radius;
            const float clamped = automatic_scale < 0.0f ? 0.0f : (automatic_scale > 1.0f ? 1.0f : automatic_scale);
            if (clamped < scale) scale = clamped;
            if (scale <= 0.01f) return out;
        }

        const math::vector3 shoot_dir = (center - shoot_pos).normalized();
        const math::vector3 ang = math::helpers::vector_to_angle(shoot_dir);

        math::vector3 left{}, up{};
        math::helpers::angle_vectors_left(ang, nullptr, &left, &up);
        const math::vector3 right = math::vector3{ -left.x, -left.y, -left.z };

        const auto surface_point = [&](const math::vector3& direction) -> math::vector3 {
            const math::vector3 dir = direction.normalized();

            if (hitbox.radius > 0.001f) {
                const float reach = (capsule_b - capsule_a).length() + hitbox.radius * 2.0f + 1.0f;
                const math::vector3 origin = center + dir * reach;
                const math::vector3 delta = dir * (reach * -2.0f);
                float fraction = 1.0f;
                if (RayVsCapsuleLocal(origin, delta, capsule_a, capsule_b, hitbox.radius, fraction)) {
                    return origin + delta * fraction;
                }
            }
            else {
                math::quaternion inverse = bone_rot;
                inverse.x = -inverse.x;
                inverse.y = -inverse.y;
                inverse.z = -inverse.z;
                const math::vector3 local_dir = inverse.rotate_vector(dir);
                const math::vector3 extents = (hitbox.maxs - hitbox.mins) * 0.5f;
                float distance = 8192.0f;

                if (std::fabs(local_dir.x) > 1.0e-6f) distance = (std::min)(distance, std::fabs(extents.x / local_dir.x));
                if (std::fabs(local_dir.y) > 1.0e-6f) distance = (std::min)(distance, std::fabs(extents.y / local_dir.y));
                if (std::fabs(local_dir.z) > 1.0e-6f) distance = (std::min)(distance, std::fabs(extents.z / local_dir.z));

                if (distance < 8192.0f) {
                    return center + dir * distance;
                }
            }

            return center;
            };

        const auto scaled_surface = [&](const math::vector3& direction) {
            const math::vector3 surface = surface_point(direction);
            return center + (surface - center) * scale;
            };

        switch (hitbox.index) {
        case 0:
            out.reserve(4);
            out.push_back(scaled_surface(right));
            out.push_back(scaled_surface(-right));
            out.push_back(scaled_surface(up));
            out.push_back(scaled_surface(-up));
            break;
        case 2:
        case 3:
            out.reserve(2);
            out.push_back(scaled_surface(right));
            out.push_back(scaled_surface(-right));
            break;
        case 4:
        case 5:
        case 6:
            out.reserve(3);
            out.push_back(scaled_surface(right));
            out.push_back(scaled_surface(-right));
            if (hitbox.index == 6) out.push_back(scaled_surface(up));
            break;
        case 7:
        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
            out.reserve(2);
            out.push_back(capsule_a);
            out.push_back(capsule_b);
            break;
        case 13:
        case 14:
        case 15:
        case 16:
        case 17:
        case 18:
            out.reserve(1);
            out.push_back(capsule_b);
            break;
        default:
            out.reserve(2);
            out.push_back(scaled_surface(right));
            out.push_back(scaled_surface(-right));
            break;
        }

        return out;
    }

    static bool RayHitsSphere(const math::vector3& origin, const math::vector3& dir,
        const math::vector3& center, float radius)
    {
        const math::vector3 oc = origin - center;
        const float b = oc.dot(dir);
        const float c = oc.dot(oc) - radius * radius;
        const float disc = b * b - c;
        return disc >= 0.0f;
    }

    static float CalculateHitchance(const Vec3& shootPos, const math::vector3& aimAngle,
        const Vec3& bonePos, float radius,
        float inaccuracy, float spread, float recoilIndex,
        int itemDefIdx, int numBullets)
    {
        const float total = inaccuracy + spread;
        if (total < 0.0005f) return 1.0f;
        if (!NoSpread::IsReady()) return 1.0f;

        constexpr int kSamples = 64;

        math::vector3 forward{}, left{}, up{};
        math::helpers::angle_vectors_left(aimAngle, &forward, &left, &up);

        const math::vector3 origin{ shootPos.x, shootPos.y, shootPos.z };
        const math::vector3 center{ bonePos.x, bonePos.y, bonePos.z };

        int hits = 0;
        for (int i = 0; i < kSamples; i++) {
            const math::vector2 spr = NoSpread::CalculateSpread(
                (uint32_t)i, itemDefIdx, numBullets, inaccuracy, spread, recoilIndex);

            const math::vector3 dir = (forward + left * spr.x + up * spr.y).normalized();
            if (RayHitsSphere(origin, dir, center, radius)) hits++;
        }

        return (float)hits / (float)kSamples;
    }

    struct ScanHit {
        uintptr_t pawn = 0;
        int health = 0;
        int team = 0;
        int boneIndex = -1;
        int hitboxIndex = -1;
        int hitgroup = 0;
        uint32_t hitboxFlag = 0;
        Vec3 bonePos{};
        math::vector3 aimAngle{};
        float fov = 0.0f;
        float damage = 0.0f;
        float hitchance = 0.0f;
        float score = 0.0f;
        int priority = 0;
        int tick = 0;
        Lagcomp::Record* record = nullptr;
        bool isCenter = false;
        bool penetrated = false;
        bool valid = false;
    };

    struct ScanView {
        BoneData bones[kMaxViewBones];
        int count;
        int tick;
        Lagcomp::Record* record;
    };

    static void BuildViews(const CombatShared::Enemy& e, bool backtrackAllowed,
        ScanView* views, int& outCount)
    {
        outCount = 0;

        if (backtrackAllowed) {
            auto records = Lagcomp::GetValidRecords(e.pawn);
            if (!records.empty()) {
                for (auto* r : records) {
                    if (outCount >= kMaxViews) break;

                    const int n = (r->boneCount < kMaxViewBones) ? r->boneCount : kMaxViewBones;
                    for (int i = 0; i < n; i++) {
                        views[outCount].bones[i] = r->bones[i];
                    }
                    for (int i = n; i < kMaxViewBones; i++) {
                        views[outCount].bones[i] = BoneData{};
                    }

                    views[outCount].count = n;
                    views[outCount].tick = r->tick;
                    views[outCount].record = r;
                    outCount++;
                }
                return;
            }
        }

        if (e.headPos.Length() < 1.0f) return;

        for (int i = 0; i < kMaxViewBones; i++) {
            views[0].bones[i].position = e.bones[i];
            views[0].bones[i].rotation = Quat{ 0.0f, 0.0f, 0.0f, 1.0f };
        }
        views[0].count = kMaxViewBones;
        views[0].tick = 0;
        views[0].record = nullptr;
        outCount = 1;
    }

    static const systems::hitboxes::entry* FindHitbox(const systems::hitboxes::set& set, int index) {
        for (const auto& hb : set) {
            if (hb.index == index) return &hb;
        }
        return nullptr;
    }

    static void ScanEnemy(uintptr_t localPawn, const WeaponState& ws,
        const Vec3& eye, const Vec3& curAngles, const WeaponSettings& cfg,
        const CombatShared::Enemy& e, std::vector<ScanHit>& out)
    {
        const int localTeam = ReadTeam(localPawn);
        if ((int)cfg.teamCheck && e.team == localTeam) return;

        const uint32_t mask = (uint32_t)cfg.hitboxes;
        const int minDamage = (int)cfg.minDamage;
        const float maxFov = 180.0f;
        const float pointscale = (float)cfg.pointscale;
        const bool autowallEnabled = (cfg.autowall != 0);

        const Vec3 curV{ curAngles.x, curAngles.y, 0.0f };
        const math::vector3 eyeM{ eye.x, eye.y, eye.z };

        const int targetArmor = ReadArmor(e.pawn);
        const bool hasHelmet = ReadHelmet(e.pawn, targetArmor);
        const int targetTeam = e.team;

        uintptr_t gsn = 0;
        if (!SafeReadPtr(e.pawn + OffsetConfig::fields.m_pGameSceneNode, &gsn)) return;
        if (gsn < 0x10000) return;

        auto hbSet = systems::g_hitboxes.query(gsn);

        if (hbSet.count <= 0) {
            if ((mask & HB_HEAD) && e.headPos.Length() > 1.0f) {
                const Vec3 ptV = e.headPos;
                const Vec3 aimV = CombatShared::CalculateAngle(eye, ptV);
                const float fov = CombatShared::AngleDistance(curV, aimV);
                if (fov <= maxFov) {
                    auto filter = Tracing::MakeFilter(localPawn, 0x1c3003, 4);
                    auto result = Tracing::Trace(eyeM, MathVec(ptV), filter);
                    const bool visible = (result.fraction >= 0.97f) || (result.hitEntity == e.pawn);
                    if (visible) {
                        float finalDmg = Penetration::GetMaxDamage(1, targetArmor, hasHelmet, targetTeam);
                        if (finalDmg >= minDamage) {
                            ScanHit h{};
                            h.pawn = e.pawn;
                            h.health = e.health;
                            h.team = e.team;
                            h.boneIndex = 7;
                            h.hitboxIndex = 0;
                            h.hitgroup = 1;
                            h.hitboxFlag = HB_HEAD;
                            h.bonePos = ptV;
                            h.aimAngle = math::vector3{ aimV.x, aimV.y, 0.0f };
                            h.fov = fov;
                            h.damage = finalDmg;
                            h.priority = 100;
                            h.tick = 0;
                            h.record = nullptr;
                            h.isCenter = true;
                            h.penetrated = false;
                            h.valid = true;
                            out.push_back(h);
                        }
                    }
                }
            }
            return;
        }

        ScanView views[kMaxViews];
        int viewCount = 0;
        BuildViews(e, cfg.backtrack != 0, views, viewCount);
        if (viewCount == 0) return;

        for (int vi = 0; vi < viewCount; vi++) {
            const ScanView& v = views[vi];
            bool anyDirectHit = false;

            std::array<bool, 20> center_sufficient{};

            for (size_t si = 0; si < kScanOrderCount; si++) {
                if (anyDirectHit) break;

                const HitboxScan& scan = kScanOrder[si];
                if (!(mask & scan.flag)) continue;

                const systems::hitboxes::entry* hb = FindHitbox(hbSet, scan.hitboxIndex);
                if (!hb) continue;
                if (hb->bone < 0 || hb->bone >= v.count) continue;

                const BoneData& bone = v.bones[hb->bone];
                if (bone.position.Length() < 1.0f) continue;

                const math::quaternion boneRot = MathQuat(bone.rotation);
                const math::vector3 bonePosV = MathVec(bone.position);

                const math::vector3 hb_mid = (hb->mins + hb->maxs) * 0.5f;
                const math::vector3 center = boneRot.rotate_vector(hb_mid) + bonePosV;

                const int hitgroup = systems::g_hitboxes.hitgroup_from_hitbox(hb->index);

                std::vector<math::vector3> points;
                points.reserve(5);
                points.push_back(center);

                if (pointscale > 0.5f) {
                    auto mps = GenerateMultipoints(*hb, center, boneRot, pointscale, eyeM,
                        ws.inaccuracy, ws.spread);
                    for (const auto& mp : mps) {
                        bool dup = false;
                        for (const auto& p : points) {
                            if ((p - mp).length_sqr() < 0.01f) { dup = true; break; }
                        }
                        if (!dup) points.push_back(mp);
                    }
                }

                const bool centerSufficient = (hb->index >= 0 && hb->index < 20)
                    ? center_sufficient[hb->index] : false;

                for (size_t pi = 0; pi < points.size(); pi++) {
                    const bool isCenter = (pi == 0);

                    if (!isCenter && centerSufficient) continue;

                    const math::vector3& pt = points[pi];
                    const Vec3 ptV{ pt.x, pt.y, pt.z };

                    const Vec3 aimV = CombatShared::CalculateAngle(eye, ptV);
                    const float fov = CombatShared::AngleDistance(curV, aimV);
                    if (fov > maxFov) continue;

                    float finalDmg = 0.0f;
                    int finalHitgroup = hitgroup;
                    bool penetrated = false;
                    bool gotHit = false;

                    auto filter = Tracing::MakeFilter(localPawn, 0x1c3003, 4);
                    auto result = Tracing::Trace(eyeM, pt, filter);

                    const bool visible =
                        (result.fraction >= 0.97f) ||
                        (result.hitEntity == e.pawn);

                    if (visible) {
                        finalDmg = Penetration::GetMaxDamage(hitgroup, targetArmor, hasHelmet, targetTeam);
                        gotHit = true;
                    }
                    else if (autowallEnabled && isCenter && vi == 0 && g_autowallCallsThisTick < kMaxAutowallCallsPerTick) {
                        g_autowallCallsThisTick++;

                        Penetration::Result penResult{};
                        bool awOk = Penetration::Run(eyeM, pt, e.pawn, localPawn, localTeam, v.record, penResult);

                        if (awOk) {
                            const bool isHeadHitbox = (hb->index == 0);

                            if (isHeadHitbox && penResult.hitgroup != 1) {
                                continue;
                            }

                            finalDmg = penResult.damage;
                            finalHitgroup = penResult.hitgroup;
                            penetrated = penResult.penetrated;
                            gotHit = true;
                        }
                    }

                    if (!gotHit) continue;
                    if (finalDmg < (float)minDamage) continue;

                    ScanHit h{};
                    h.pawn = e.pawn;
                    h.health = e.health;
                    h.team = e.team;
                    h.boneIndex = hb->bone;
                    h.hitboxIndex = hb->index;
                    h.hitgroup = finalHitgroup;
                    h.hitboxFlag = scan.flag;
                    h.bonePos = ptV;
                    h.aimAngle = math::vector3{ aimV.x, aimV.y, 0.0f };
                    h.fov = fov;
                    h.damage = finalDmg;
                    h.priority = scan.priority;
                    h.tick = v.tick;
                    h.record = v.record;
                    h.isCenter = isCenter;
                    h.penetrated = penetrated;
                    h.valid = true;

                    out.push_back(h);
                    anyDirectHit = true;

                    if (isCenter && hb->index >= 0 && hb->index < 20) {
                        center_sufficient[hb->index] = !penetrated || finalDmg >= (float)e.health;
                    }

                    break;
                }
            }

            if (anyDirectHit) break;
        }
    }

    static ScanHit SelectBest(const WeaponState& ws, const Vec3& eye, const std::vector<ScanHit>& hits) {
        ScanHit best{};

        for (const auto& h : hits) {
            if (!h.valid) continue;

            const bool lethal = h.damage >= (float)h.health;
            float score = 0.0f;

            score += lethal ? 1000000.0f : 0.0f;
            score += h.damage * 20.0f;
            score += (float)h.priority * 50.0f;
            score += h.isCenter ? 100.0f : 0.0f;
            score += h.penetrated ? 0.0f : 250.0f;
            score -= h.fov * 1.0f;

            bool isBetter = !best.valid || score > best.score;
            if (best.valid && std::fabs(score - best.score) < 0.01f) {
                if (h.tick != best.tick) {
                    isBetter = h.tick > best.tick;
                }
                else if (h.priority != best.priority) {
                    isBetter = h.priority > best.priority;
                }
                else if (h.isCenter != best.isCenter) {
                    isBetter = h.isCenter;
                }
                else {
                    isBetter = h.fov < best.fov;
                }
            }

            if (isBetter) {
                best = h;
                best.score = score;
                best.hitchance = 1.0f;

                if (ws.valid && (ws.inaccuracy > 0.0005f || ws.spread > 0.0005f)) {
                    const float radius = (h.hitboxIndex == 0) ? 4.0f : 6.0f;
                    best.hitchance = CalculateHitchance(
                        eye, h.aimAngle, h.bonePos, radius,
                        ws.inaccuracy, ws.spread, ws.recoilIndex,
                        ws.itemDefIdx, ws.numBullets);
                }
            }
        }

        return best;
    }

    static void OnCreateMoveInternal(void* cmdPtr) {
        UsercmdLayout* cmd = reinterpret_cast<UsercmdLayout*>(cmdPtr);
        if (!cmd) return;

        const bool wasAttacking = g_rageAttackWritten.load();

        g_autowallCallsThisTick = 0;

        if (Renderer::IsMenuOpen() || !IsGameFocused()) {
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

        if (!ws.valid) {
            if (wasAttacking) {
                cmd->buttons.value &= ~1ull;
                cmd->buttons.value_changed |= 1ull;
                g_rageAttackWritten.store(false);
            }
            return;
        }

        const Group group = GroupFromWeaponType(ws.weaponType);
        const WeaponSettings& cfg = g_weapons[(int)group];

        if (cfg.enabled == 0) {
            if (wasAttacking) {
                cmd->buttons.value &= ~1ull;
                cmd->buttons.value_changed |= 1ull;
                g_rageAttackWritten.store(false);
            }
            return;
        }

        Penetration::Prepare(ws.vdata, ws.weapon);

        std::vector<ScanHit> hits;
        const auto enemies = CombatShared::GetEnemiesCached();
        hits.reserve(enemies.size() * 8);

        for (const auto& e : enemies) {
            if (e.health <= 0) continue;
            if (!e.pawn) continue;
            ScanEnemy(localPawn, ws, eye, curAngles, cfg, e, hits);
        }

        ScanHit best = SelectBest(ws, eye, hits);

        const bool autoShoot = (cfg.autoShoot != 0);
        const bool wantFire = autoShoot && best.valid;

        const uintptr_t lc = CombatShared::GetLocalController();
        const int tickBase = ReadTickBase(lc);

        int nextPrimaryTick = 0;
        if (g_nextPrimaryTickOff && ws.weapon > 0x10000) {
            SafeReadI32(ws.weapon + g_nextPrimaryTickOff, &nextPrimaryTick);
        }

        const bool weaponReady = (tickBase > 0) && (tickBase >= nextPrimaryTick);
        const bool canFire = wantFire && weaponReady;

        const math::vector3 userAngleVec{ curAngles.x, curAngles.y, 0.0f };
        const bool silent = (cfg.silent != 0);
        math::vector3 writeAngle = best.valid ? best.aimAngle : userAngleVec;

        if (canFire && cfg.noSpread != 0) {
            WeaponState wsAcc = GetWeaponStateWithAccuracy(localPawn, tickBase);

            if (wsAcc.valid && tickBase > 0) {
                const float recoil = wsAcc.recoilIndex > 0.01f ? wsAcc.recoilIndex : (float)wsAcc.shotsFired;

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
        if (base && best.valid) {
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

        if (best.valid) {
            const int32_t renderTick = best.record ? (best.tick + 1) : 0;

            const int historySize = cmd->pb.input_history.current_size;
            for (int i = 0; i < historySize; ++i) {
                auto* entry = GetInputHistoryEntry(cmd, i);
                if (!entry) continue;

                uintptr_t rawVa = reinterpret_cast<uintptr_t>(entry->m_view_angles);
                if (rawVa >= 0x10000) {
                    auto* va = reinterpret_cast<proto::msg_qangle*>(rawVa + 0x10);
                    va->set_x(writeAngle.x);
                    va->set_y(writeAngle.y);
                    va->set_z(writeAngle.z);
                }

                if (renderTick > 0) {
                    entry->m_render_tick_count = renderTick;
                    entry->m_render_tick_fraction = 0.0f;
                }
            }

            if (!silent) {
                WriteClientViewAngles(writeAngle);
            }
        }

        if (canFire) {
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

    bool IsEnabled(Group g) { return g_weapons[(int)g].enabled != 0; }
    void SetEnabled(Group g, bool enabled) {
        InterlockedExchange(&g_weapons[(int)g].enabled, enabled ? 1 : 0);
        if (!enabled) g_rageAttackWritten.store(false);
    }

    bool IsAutoShoot(Group g) { return g_weapons[(int)g].autoShoot != 0; }
    void SetAutoShoot(Group g, bool enabled) { InterlockedExchange(&g_weapons[(int)g].autoShoot, enabled ? 1 : 0); }

    bool IsSilent(Group g) { return g_weapons[(int)g].silent != 0; }
    void SetSilent(Group g, bool enabled) { InterlockedExchange(&g_weapons[(int)g].silent, enabled ? 1 : 0); }

    bool IsNoSpread(Group g) { return g_weapons[(int)g].noSpread != 0; }
    void SetNoSpread(Group g, bool enabled) { InterlockedExchange(&g_weapons[(int)g].noSpread, enabled ? 1 : 0); }

    bool IsAutowall(Group g) { return g_weapons[(int)g].autowall != 0; }
    void SetAutowall(Group g, bool enabled) { InterlockedExchange(&g_weapons[(int)g].autowall, enabled ? 1 : 0); }

    bool IsTeamCheck(Group g) { return g_weapons[(int)g].teamCheck != 0; }
    void SetTeamCheck(Group g, bool enabled) { InterlockedExchange(&g_weapons[(int)g].teamCheck, enabled ? 1 : 0); }

    bool IsBacktrack(Group g) { return g_weapons[(int)g].backtrack != 0; }
    void SetBacktrack(Group g, bool enabled) { InterlockedExchange(&g_weapons[(int)g].backtrack, enabled ? 1 : 0); }

    int GetPointscale(Group g) { return (int)g_weapons[(int)g].pointscale; }
    void SetPointscale(Group g, int scale) {
        if (scale < 0) scale = 0;
        if (scale > 100) scale = 100;
        InterlockedExchange(&g_weapons[(int)g].pointscale, scale);
    }

    int GetMinDamage(Group g) { return (int)g_weapons[(int)g].minDamage; }
    void SetMinDamage(Group g, int damage) {
        if (damage < 1) damage = 1;
        if (damage > 130) damage = 130;
        InterlockedExchange(&g_weapons[(int)g].minDamage, damage);
    }

    uint32_t GetHitboxes(Group g) { return (uint32_t)g_weapons[(int)g].hitboxes; }
    void SetHitboxes(Group g, uint32_t mask) {
        if (mask == 0) mask = HB_HEAD;
        InterlockedExchange(&g_weapons[(int)g].hitboxes, (LONG)mask);
    }
}