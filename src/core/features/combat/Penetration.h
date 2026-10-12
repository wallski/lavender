#pragma once
#include <cstdint>
#include "../../math/math.hpp"
#include "Lagcomp.h"

namespace Penetration {

    struct WeaponData {
        float damage = 0.0f;
        float penetration = 0.0f;
        float rangeModifier = 0.0f;
        float range = 0.0f;
        float armorRatio = 0.0f;
        float headshotMultiplier = 0.0f;
    };

    struct Result {
        float damage = 0.0f;
        int hitbox = -1;
        int hitgroup = -1;
        bool penetrated = false;
    };

    void Prepare(uintptr_t weaponVdata, uintptr_t weapon);

    bool Run(const math::vector3& start,
        const math::vector3& end,
        uintptr_t targetPawn,
        uintptr_t localPawn,
        int localTeam,
        Lagcomp::Record* record,
        Result& out);

    float GetMaxDamage(int hitgroup, int targetArmor, bool hasHelmet, int targetTeam);

    bool RayVsCapsule(const math::vector3& rayOrigin,
        const math::vector3& rayDir,
        const math::vector3& capsuleA,
        const math::vector3& capsuleB,
        float radius,
        float& outFraction);
}