#include "pch.h"
#include "Penetration.h"
#include "CombatShared.h"
#include "../../systems/Tracing.h"
#include "../../systems/systems.hpp"
#include "../../systems/schemas/Schemas.h"
#include "../../cvar/CVar.h"
#include "../../../config/OffsetConfig.h"
#include "../../math/hash.hpp"
#include <cmath>
#include <cstring>
#include <algorithm>

namespace Penetration {

    static WeaponData g_weapon{};

    struct BulletTraceRecord {
        float enterFraction;
        float exitFraction;
        float damageApplied;
        int teamAtContact;
        uint16_t enterContactIx;
        uint16_t exitContactIx;
        uint8_t canPenetrate;
        uint8_t pad[3];
    };

    static_assert(sizeof(BulletTraceRecord) == 0x18);

    struct DamageScales {
        float ctHead;
        float tHead;
        float ctBody;
        float tBody;
    };

    static constexpr size_t kScratchSize = 0x8000;

    static uint8_t* AcquireScratch() {
        static uint8_t* s_scratch = nullptr;
        if (!s_scratch) {
            s_scratch = reinterpret_cast<uint8_t*>(
                VirtualAlloc(nullptr, kScratchSize,
                    MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        }
        return s_scratch;
    }

    static bool SafeReadU32(uintptr_t addr, uint32_t* out) {
        __try { *out = *reinterpret_cast<uint32_t*>(addr); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        return true;
    }

    static bool SafeReadI32(uintptr_t addr, int* out) {
        __try { *out = *reinterpret_cast<int*>(addr); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        return true;
    }

    static bool SafeReadU8(uintptr_t addr, uint8_t* out) {
        __try { *out = *reinterpret_cast<uint8_t*>(addr); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        return true;
    }

    void Prepare(uintptr_t weaponVdata, uintptr_t weapon) {
        g_weapon = {};
        if (!weaponVdata || !weapon) {
            return;
        }

        const uint32_t dmgOff = SCHEMA("CCSWeaponBaseVData", "m_nDamage"_hash);
        const uint32_t penOff = SCHEMA("CCSWeaponBaseVData", "m_flPenetration"_hash);
        const uint32_t rangeModOff = SCHEMA("CCSWeaponBaseVData", "m_flRangeModifier"_hash);
        const uint32_t rangeOff = SCHEMA("CCSWeaponBaseVData", "m_flRange"_hash);
        const uint32_t armorOff = SCHEMA("CCSWeaponBaseVData", "m_flArmorRatio"_hash);
        const uint32_t hsOff = SCHEMA("CCSWeaponBaseVData", "m_flHeadshotMultiplier"_hash);

        if (!dmgOff || !penOff || !rangeModOff || !rangeOff || !armorOff || !hsOff) {
            return;
        }

        int dmg = 0;
        SafeReadI32(weaponVdata + dmgOff, &dmg);
        g_weapon.damage = (float)dmg;

        __try { g_weapon.penetration = *reinterpret_cast<float*>(weaponVdata + penOff); }
        __except (EXCEPTION_EXECUTE_HANDLER) {}

        __try { g_weapon.rangeModifier = *reinterpret_cast<float*>(weaponVdata + rangeModOff); }
        __except (EXCEPTION_EXECUTE_HANDLER) {}

        __try { g_weapon.range = *reinterpret_cast<float*>(weaponVdata + rangeOff); }
        __except (EXCEPTION_EXECUTE_HANDLER) {}

        __try { g_weapon.armorRatio = *reinterpret_cast<float*>(weaponVdata + armorOff); }
        __except (EXCEPTION_EXECUTE_HANDLER) {}

        __try { g_weapon.headshotMultiplier = *reinterpret_cast<float*>(weaponVdata + hsOff); }
        __except (EXCEPTION_EXECUTE_HANDLER) {}

        if (!std::isfinite(g_weapon.damage) || g_weapon.damage < 1.0f || g_weapon.damage > 500.0f) g_weapon.damage = 30.0f;
        if (!std::isfinite(g_weapon.penetration) || g_weapon.penetration < 0.01f || g_weapon.penetration > 10.0f) g_weapon.penetration = 1.0f;
        if (!std::isfinite(g_weapon.rangeModifier) || g_weapon.rangeModifier < 0.5f || g_weapon.rangeModifier > 1.0f) g_weapon.rangeModifier = 0.98f;
        if (!std::isfinite(g_weapon.range) || g_weapon.range < 1.0f || g_weapon.range > 65536.0f) g_weapon.range = 8192.0f;
        if (!std::isfinite(g_weapon.armorRatio) || g_weapon.armorRatio < 0.1f || g_weapon.armorRatio > 3.0f) g_weapon.armorRatio = 0.5f;
        if (!std::isfinite(g_weapon.headshotMultiplier) || g_weapon.headshotMultiplier < 1.0f || g_weapon.headshotMultiplier > 20.0f) g_weapon.headshotMultiplier = 4.0f;
    }

    static void GetDamageScales(DamageScales& out) {
        out.ctHead = CVar::GetFloat("mp_damage_scale_ct_head", 1.0f);
        out.tHead = CVar::GetFloat("mp_damage_scale_t_head", 1.0f);
        out.ctBody = CVar::GetFloat("mp_damage_scale_ct_body", 1.0f);
        out.tBody = CVar::GetFloat("mp_damage_scale_t_body", 1.0f);
    }

    static void ScaleDamage(int hitgroup, int armor, bool hasHelmet, int team,
        float armorRatio, float headshotMultiplier, const DamageScales& scales, float& damage)
    {
        const bool isCt = (team == 3);
        const float headScale = isCt ? scales.ctHead : scales.tHead;
        const float bodyScale = isCt ? scales.ctBody : scales.tBody;

        switch (hitgroup) {
        case 1: damage *= headshotMultiplier * headScale; break;
        case 2:
        case 4:
        case 5:
        case 8: damage *= bodyScale; break;
        case 3: damage *= 1.25f * bodyScale; break;
        case 6:
        case 7: damage *= 0.75f * bodyScale; break;
        default: break;
        }

        const bool isHead = (hitgroup == 1);
        const bool isArmored = (hitgroup >= 1 && hitgroup <= 5) || (hitgroup == 8);

        if (armor <= 0 || !isArmored || (isHead && !hasHelmet)) {
            damage = std::floor(damage);
            return;
        }

        constexpr float armorBonus = 0.5f;
        const float armorRatioScaled = armorRatio * 0.5f;

        float damageToHealth = damage * armorRatioScaled;
        float damageToArmor = (damage - damageToHealth) * armorBonus;

        if (damageToArmor > (float)armor) {
            damageToHealth = damage - ((float)armor / armorBonus);
        }

        damage = std::floor(damageToHealth);
    }

    bool RayVsCapsule(const math::vector3& rayOrigin, const math::vector3& rayDir,
        const math::vector3& capsuleA, const math::vector3& capsuleB,
        float radius, float& outFraction)
    {
        const math::vector3 ab = capsuleB - capsuleA;
        const float abSq = ab.dot(ab);
        const math::vector3 oc = rayOrigin - capsuleA;
        const float dirSq = rayDir.dot(rayDir);

        if (dirSq < 1e-8f) return false;

        float bestT = 1.0f;
        bool hit = false;

        if (abSq > 1e-8f) {
            const float m = ab.dot(rayDir) / abSq;
            const float n = ab.dot(oc) / abSq;

            const math::vector3 dPerp = rayDir - ab * m;
            const math::vector3 ocPerp = oc - ab * n;

            const float a = dPerp.dot(dPerp);
            const float halfB = dPerp.dot(ocPerp);
            const float c = ocPerp.dot(ocPerp) - radius * radius;

            if (a > 1e-8f) {
                const float disc = halfB * halfB - a * c;
                if (disc >= 0.0f) {
                    const float sqrtDisc = std::sqrt(disc);
                    for (int r = 0; r < 2; r++) {
                        const float t = (-halfB + (r == 0 ? -sqrtDisc : sqrtDisc)) / a;
                        if (t < 0.0f || t >= bestT) continue;
                        const float s = m * t + n;
                        if (s >= 0.0f && s <= 1.0f) {
                            bestT = t;
                            hit = true;
                            break;
                        }
                    }
                }
            }
        }

        const math::vector3 caps[2] = { capsuleA, capsuleB };

        for (int i = 0; i < 2; i++) {
            const math::vector3 co = rayOrigin - caps[i];
            const float halfB = co.dot(rayDir);
            const float c = co.dot(co) - radius * radius;
            const float disc = halfB * halfB - dirSq * c;

            if (disc < 0.0f) continue;

            const float sqrtDisc = std::sqrt(disc);
            for (int r = 0; r < 2; r++) {
                const float t = (-halfB + (r == 0 ? -sqrtDisc : sqrtDisc)) / dirSq;
                if (t < 0.0f || t >= bestT) continue;

                if (abSq > 1e-8f) {
                    const math::vector3 hitPoint = rayOrigin + rayDir * t - caps[i];
                    const float sign = i == 0 ? -1.0f : 1.0f;
                    if (sign * ab.dot(hitPoint) < 0.0f) continue;
                }

                bestT = t;
                hit = true;
                break;
            }
        }

        if (hit) outFraction = bestT;
        return hit;
    }

    static int IdentifyHitbox(const math::vector3& start, const math::vector3& delta,
        Lagcomp::Record* record, const systems::hitboxes::set& hitboxes)
    {
        if (!record) return -1;

        int bestHitbox = -1;
        float closestFraction = 1.0f;

        for (const auto& hb : hitboxes) {
            if (hb.bone < 0 || hb.bone >= record->boneCount) continue;

            const BoneData& boneData = record->bones[hb.bone];
            const math::vector3 bonePos{ boneData.position.x, boneData.position.y, boneData.position.z };
            const math::quaternion boneRot{
                boneData.rotation.x, boneData.rotation.y, boneData.rotation.z, boneData.rotation.w };

            float fraction = 1.0f;
            bool intersects = false;

            if (hb.radius > 0.001f) {
                const math::vector3 capA = boneRot.rotate_vector(hb.mins) + bonePos;
                const math::vector3 capB = boneRot.rotate_vector(hb.maxs) + bonePos;
                intersects = RayVsCapsule(start, delta, capA, capB, hb.radius, fraction);
            }
            else {
                math::quaternion inverse = boneRot;
                inverse.x = -inverse.x;
                inverse.y = -inverse.y;
                inverse.z = -inverse.z;

                const math::vector3 localOrigin = inverse.rotate_vector(start - bonePos);
                const math::vector3 localDelta = inverse.rotate_vector(delta);

                float entry = 0.0f;
                float exit = 1.0f;

                const auto intersectAxis = [&](float origin, float d, float mn, float mx) -> bool {
                    if (std::fabs(d) < 1.0e-8f) {
                        return origin >= mn && origin <= mx;
                    }
                    float first = (mn - origin) / d;
                    float second = (mx - origin) / d;
                    if (first > second) std::swap(first, second);
                    entry = (std::max)(entry, first);
                    exit = (std::min)(exit, second);
                    return entry <= exit;
                    };

                intersects = intersectAxis(localOrigin.x, localDelta.x, hb.mins.x, hb.maxs.x)
                    && intersectAxis(localOrigin.y, localDelta.y, hb.mins.y, hb.maxs.y)
                    && intersectAxis(localOrigin.z, localDelta.z, hb.mins.z, hb.maxs.z);
                fraction = entry;
            }

            if (intersects && fraction < closestFraction) {
                closestFraction = fraction;
                bestHitbox = hb.index;
            }
        }

        return bestHitbox;
    }

    bool Run(const math::vector3& start, const math::vector3& end,
        uintptr_t targetPawn, uintptr_t localPawn, int localTeam,
        Lagcomp::Record* record, Result& out)
    {
        out = {};

        if (g_weapon.damage <= 0.0f) return false;
        if (g_weapon.penetration <= 0.0f) return false;

        const math::vector3 direction = (end - start).normalized();
        const math::vector3 traceDelta = direction * g_weapon.range;

        Tracing::Filter filter = Tracing::MakeFilter(localPawn, 0x1c300b, 3, 15);

        uint8_t* scratch = AcquireScratch();
        if (!scratch) {
            return false;
        }

        auto* trace = reinterpret_cast<Tracing::TraceData*>(scratch);

        *trace = Tracing::TraceData{};
        trace->arrayPointer = &trace->elements;
        trace->hitArrayPointer = &trace->hitElements;

        Tracing::SetupTrace(trace, start, traceDelta, filter, 4, true);

        if (trace->numHits <= 0) return false;

        Tracing::TraceBullet(trace, g_weapon.damage, g_weapon.penetration,
            g_weapon.rangeModifier, 4, localTeam, 0);

        const int numHits = trace->numHits;

        if (numHits <= 0 || numHits > 64) return false;

        const uintptr_t hitArray = reinterpret_cast<uintptr_t>(trace->hitArrayPointer);
        const uintptr_t surfaceArray = reinterpret_cast<uintptr_t>(trace->arrayPointer);
        if (hitArray < 0x10000 || surfaceArray < 0x10000) return false;

        int actualHitbox = -1;
        if (record) {
            uintptr_t gsn = 0;
            __try { gsn = *reinterpret_cast<uintptr_t*>(targetPawn + OffsetConfig::fields.m_pGameSceneNode); }
            __except (EXCEPTION_EXECUTE_HANDLER) { gsn = 0; }

            if (gsn >= 0x10000) {
                auto hbSet = systems::g_hitboxes.query(gsn);
                if (hbSet.count > 0) {
                    actualHitbox = IdentifyHitbox(start, traceDelta, record, hbSet);
                }
            }
        }

        int targetArmor = 0;
        SafeReadI32(targetPawn + SCHEMA("C_CSPlayerPawn", "m_ArmorValue"_hash), &targetArmor);

        int targetTeam = 0;
        SafeReadI32(targetPawn + OffsetConfig::fields.m_iTeamNum, &targetTeam);

        bool hasHelmet = false;
        if (targetArmor > 0) {
            uintptr_t services = 0;
            __try { services = *reinterpret_cast<uintptr_t*>(targetPawn + SCHEMA("C_BasePlayerPawn", "m_pItemServices"_hash)); }
            __except (EXCEPTION_EXECUTE_HANDLER) { services = 0; }

            if (services >= 0x10000) {
                uint8_t helmet = 0;
                SafeReadU8(services + SCHEMA("CCSPlayer_ItemServices", "m_bHasHelmet"_hash), &helmet);
                hasHelmet = (helmet != 0);
            }
        }

        DamageScales scales{};
        GetDamageScales(scales);

        bool penetrated = false;
        bool found = false;

        __try {
            for (int i = 0; i < numHits; ++i) {
                auto* hit = reinterpret_cast<BulletTraceRecord*>(hitArray + i * sizeof(BulletTraceRecord));
                const float damage = hit->damageApplied;

                if (!std::isfinite(damage) || damage <= 0.0f) break;

                if ((hit->canPenetrate & 1) != 0) {
                    penetrated = true;
                    if (hit->exitFraction == 1.0f) break;
                    continue;
                }

                const uintptr_t traceHolder = surfaceArray
                    + sizeof(Tracing::TraceArrayElement) * (hit->enterContactIx & 0x7fff);

                uint32_t hitHandle = 0;
                SafeReadU32(traceHolder + 0x2c, &hitHandle);

                if (!hitHandle || hitHandle == 0xffffffff) continue;

                const uintptr_t hitEntity = CombatShared::GetEntityByHandle(hitHandle);
                if (hitEntity != targetPawn) continue;

                if (actualHitbox < 0) continue;

                out.hitbox = actualHitbox;
                out.hitgroup = systems::g_hitboxes.hitgroup_from_hitbox(actualHitbox);
                out.penetrated = penetrated;
                out.damage = damage;

                ScaleDamage(out.hitgroup, targetArmor, hasHelmet, targetTeam,
                    g_weapon.armorRatio, g_weapon.headshotMultiplier, scales, out.damage);

                found = true;
                break;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            found = false;
        }

        if (!found) {
            out = {};
            return false;
        }
        return true;
    }

    float GetMaxDamage(int hitgroup, int targetArmor, bool hasHelmet, int targetTeam) {
        if (g_weapon.damage <= 0.0f) return 0.0f;

        DamageScales scales{};
        GetDamageScales(scales);

        float damage = g_weapon.damage;
        ScaleDamage(hitgroup, targetArmor, hasHelmet, targetTeam,
            g_weapon.armorRatio, g_weapon.headshotMultiplier, scales, damage);
        return damage;
    }
}