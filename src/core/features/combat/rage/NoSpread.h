#pragma once
#include <cstdint>
#include "../../../math/math.hpp"

namespace NoSpread {

    bool Initialize();
    void Shutdown();

    bool IsReady();

    math::vector3 Correct(const math::vector3& aimAngle,
        int tick,
        int itemDefIdx,
        int numBullets,
        float inaccuracy,
        float spread,
        float recoilIndex);

    uint32_t GetSeed(const math::vector3& angles, int tick);

    math::vector2 CalculateSpread(uint32_t seed,
        int itemDefIdx,
        int numBullets,
        float inaccuracy,
        float spread,
        float recoilIndex);
}