#include "pch.h"
#include "NoSpread.h"
#include "core/memory/PatternScan.h"
#include <cmath>

namespace NoSpread {

    static uintptr_t g_getTickViewAngles = 0;
    static uintptr_t g_calculateSpread = 0;

    static uintptr_t ResolveRelCall(uintptr_t instr) {
        int32_t rel = 0;
        __try { rel = *reinterpret_cast<int32_t*>(instr + 1); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
        return instr + 5 + static_cast<intptr_t>(rel);
    }

    static uint32_t GetSeed(const math::vector3& angles, int tick) {
        if (!g_getTickViewAngles) return 0;
        using Fn = uint32_t(*)(void*, const math::vector3*, int);
        auto fn = reinterpret_cast<Fn>(g_getTickViewAngles);
        __try { return fn(nullptr, &angles, tick); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
    }

    static math::vector2 CalcSpread(int seed, float accuracy, float spread,
        float recoilIndex, int itemDefIdx, int numBullets)
    {
        math::vector2 out{};
        if (!g_calculateSpread) return out;
        using Fn = void(*)(int16_t, int, int, uint32_t, float, float, float, float*, float*);
        auto fn = reinterpret_cast<Fn>(g_calculateSpread);
        __try {
            fn((int16_t)itemDefIdx, numBullets, 0, (uint32_t)(seed + 1),
                accuracy, spread, recoilIndex, &out.x, &out.y);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
        return out;
    }

    bool Initialize() {
        uintptr_t tva = Memory::FindPattern("client.dll",
            "48 89 5C 24 08 57 48 81 EC F0 00 00 00 F3 0F 10 0A 48 8D 8C 24 10 01 00 00 41 8B D8 48 8B FA E8 ? ? ? ? F3 0F 10 4F 04");
        if (tva) g_getTickViewAngles = tva;

        uintptr_t cs = Memory::FindPattern("client.dll",
            "28 F3 44 0F 11 44 24 20 E8 ? ? ? ? 48 8D 85 B0 00 00 00");
        if (cs) g_calculateSpread = ResolveRelCall(cs + 8);

        printf("[NoSpread] tva=%d calc=%d\n",
            g_getTickViewAngles != 0, g_calculateSpread != 0);
        fflush(stdout);

        return g_getTickViewAngles != 0 && g_calculateSpread != 0;
    }

    void Shutdown() {
        g_getTickViewAngles = 0;
        g_calculateSpread = 0;
    }

    bool IsReady() {
        return g_getTickViewAngles != 0 && g_calculateSpread != 0;
    }

    math::vector3 Correct(const math::vector3& aimAngle, int tick,
        int itemDefIdx, int numBullets, float inaccuracy, float spread, float recoilIndex)
    {
        if (!g_getTickViewAngles || !g_calculateSpread) return aimAngle;
        if (inaccuracy < 0.001f && spread < 0.001f) return aimAngle;

        constexpr float kRadToDeg = 180.0f / 3.14159265f;

        for (int i = 0; i < 360; i++) {
            const math::vector3 testAngles{ (float)i, aimAngle.y, 0.0f };
            const uint32_t seed = GetSeed(testAngles, tick);
            if (!seed) continue;

            const math::vector2 spr = CalcSpread(seed,
                inaccuracy, spread, recoilIndex, itemDefIdx, numBullets);

            const float mag = std::sqrt(spr.x * spr.x + spr.y * spr.y);

            math::vector3 adjusted = aimAngle;
            if (mag > 0.0001f) {
                adjusted.x += std::atan(mag) * kRadToDeg;
            }
            adjusted.z = -std::atan2(spr.x, spr.y) * kRadToDeg;

            if (GetSeed(adjusted, tick) == seed) {
                return adjusted;
            }
        }

        return aimAngle;
    }
}