#include "pch.h"
#include "Tracing.h"
#include "../../config/OffsetConfig.h"
#include "../../core/memory/PatternScan.h"

namespace Tracing {

    static uintptr_t g_traceRay = 0;
    static uintptr_t g_traceRayEntity = 0;
    static uintptr_t g_traceFilterInit = 0;
    static uintptr_t g_traceHull = 0;
    static uintptr_t g_traceBullet = 0;
    static uintptr_t g_traceBulletDataInit = 0;
    static uintptr_t g_traceBulletFree = 0;
    static uintptr_t g_traceBulletUpdate = 0;

    static uintptr_t ResolveRelCall(uintptr_t instr) {
        int32_t rel = 0;
        __try { rel = *reinterpret_cast<int32_t*>(instr + 1); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
        return instr + 5 + static_cast<intptr_t>(rel);
    }

    static bool SafeReadU64(uintptr_t addr, uintptr_t* out) {
        __try { *out = *reinterpret_cast<uintptr_t*>(addr); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        return true;
    }

    static uintptr_t ResolveTraceRay() {
        const char* callerSig =
            "48 8B 0D ? ? ? ? 4C 8D 4B 0C 4C 8B C3 48 8D 55 50 0F 10 00 0F 11 45 50 F3 0F 10 48 0C "
            "48 8D 45 80 48 89 44 24 28 48 8D 44 24 30 0F 2F CE 48 89 44 24 20 0F 97 45 78";

        auto hits = Memory::FindAllPatterns("client.dll", callerSig);
        if (hits.empty()) return 0;

        uintptr_t gm = OffsetConfig::globals.dwGameTraceManager;

        for (auto h : hits) {
            int32_t rel = *reinterpret_cast<int32_t*>(h + 3);
            uintptr_t target = h + 7 + static_cast<intptr_t>(rel);

            uintptr_t value = 0;
            if (!SafeReadU64(target, &value)) continue;
            if (value != gm) continue;

            const uint8_t* p = reinterpret_cast<const uint8_t*>(h);
            if (p[56] != 0xE8) continue;

            int32_t callRel = *reinterpret_cast<const int32_t*>(p + 57);
            return (uintptr_t)(p + 61) + callRel;
        }

        return 0;
    }

    static uintptr_t ResolveTraceRayEntity() {
        const char* sigs[] = {
            "44 24 68 48 89 7C 24 20 E8 ? ? ? ? 48 8B 3D ? ? ? ?",
            "48 89 7C 24 20 E8 ? ? ? ? 48 8B 3D ? ? ? ?"
        };
        for (auto sig : sigs) {
            uintptr_t addr = Memory::FindPattern("client.dll", sig);
            if (addr) return ResolveRelCall(addr + 8);
        }
        return 0;
    }

    bool Initialize() {
        g_traceRay = ResolveTraceRay();
        g_traceRayEntity = ResolveTraceRayEntity();

        g_traceFilterInit = Memory::FindPattern("client.dll",
            "48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC ? 0F B6 41 ? 33 FF 24");

        uintptr_t hullCall = Memory::FindPattern("client.dll",
            "E8 ? ? ? ? 0F 2F 75 4C");
        if (hullCall) g_traceHull = ResolveRelCall(hullCall);

        g_traceBullet = Memory::FindPattern("client.dll",
            "40 53 57 41 56 48 83 EC 50 8B 84 24");

        g_traceBulletDataInit = Memory::FindPattern("client.dll",
            "48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 57 41 56 41 57 48 83 EC ? F2 0F 10 02");

        uintptr_t freeCall = Memory::FindPattern("client.dll",
            "E8 ? ? ? ? 3B DE 74 4E");
        if (freeCall) g_traceBulletFree = ResolveRelCall(freeCall);

        uintptr_t updateCall = Memory::FindPattern("client.dll",
            "E8 ? ? ? ? F3 44 0F 10 65 74");
        if (updateCall) g_traceBulletUpdate = ResolveRelCall(updateCall);

        printf("[Tracing] ready (ray=%d filter=%d bullet=%d bulletInit=%d)\n",
            g_traceRay != 0, g_traceFilterInit != 0,
            g_traceBullet != 0, g_traceBulletDataInit != 0);
        fflush(stdout);

        return g_traceRay != 0
            && g_traceFilterInit != 0
            && g_traceBullet != 0
            && g_traceBulletDataInit != 0;
    }

    void Shutdown() {
        g_traceRay = 0;
        g_traceRayEntity = 0;
        g_traceFilterInit = 0;
        g_traceHull = 0;
        g_traceBullet = 0;
        g_traceBulletDataInit = 0;
        g_traceBulletFree = 0;
        g_traceBulletUpdate = 0;
    }

    Filter MakeFilter(uintptr_t skipEntity, uintptr_t mask, uint8_t layer, int type) {
        Filter filter{};
        if (!g_traceFilterInit) return filter;
        __try {
            Memory::Call<void>(g_traceFilterInit,
                reinterpret_cast<uintptr_t>(&filter), skipEntity, mask,
                static_cast<uintptr_t>(layer), static_cast<uintptr_t>(type));
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
        return filter;
    }

    Filter MakeFilter(uintptr_t skipEntity, uintptr_t mask, uint8_t layer) {
        return MakeFilter(skipEntity, mask, layer, 7);
    }

    Result Trace(const math::vector3& start, const math::vector3& end, const Filter& filter) {
        Result result{};
        if (!g_traceRay) return result;

        uintptr_t manager = OffsetConfig::globals.dwGameTraceManager;
        if (!manager) return result;

        Ray ray{};

        __try {
            Memory::Call<bool>(g_traceRay, manager,
                reinterpret_cast<uintptr_t>(&ray),
                reinterpret_cast<uintptr_t>(&start),
                reinterpret_cast<uintptr_t>(&end),
                reinterpret_cast<uintptr_t>(&filter),
                reinterpret_cast<uintptr_t>(&result));
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
        return result;
    }

    Result TraceHull(const math::vector3& start, const math::vector3& end, const math::vector3& mins, const math::vector3& maxs, const Filter& filter) {
        Result result{};
        if (!g_traceRay) return result;

        uintptr_t manager = OffsetConfig::globals.dwGameTraceManager;
        if (!manager) return result;

        Ray ray{};
        ray.mins = mins;
        ray.maxs = maxs;
        ray.type = 2;

        __try {
            Memory::Call<bool>(g_traceRay, manager,
                reinterpret_cast<uintptr_t>(&ray),
                reinterpret_cast<uintptr_t>(&start),
                reinterpret_cast<uintptr_t>(&end),
                reinterpret_cast<uintptr_t>(&filter),
                reinterpret_cast<uintptr_t>(&result));
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
        return result;
    }

    Result TraceSphere(const math::vector3& start, const math::vector3& end, float radius, const Filter& filter) {
        Result result{};
        if (!g_traceRay) return result;

        uintptr_t manager = OffsetConfig::globals.dwGameTraceManager;
        if (!manager) return result;

        Ray ray{};
        *reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(&ray) + 12) = radius;
        ray.type = 1;

        __try {
            Memory::Call<bool>(g_traceRay, manager,
                reinterpret_cast<uintptr_t>(&ray),
                reinterpret_cast<uintptr_t>(&start),
                reinterpret_cast<uintptr_t>(&end),
                reinterpret_cast<uintptr_t>(&filter),
                reinterpret_cast<uintptr_t>(&result));
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
        return result;
    }

    Result TraceToEntity(const math::vector3& start, const math::vector3& end, uintptr_t targetEntity, const Filter& filter) {
        Result result{};
        if (!g_traceRayEntity) return result;

        uintptr_t manager = OffsetConfig::globals.dwGameTraceManager;
        if (!manager) return result;

        Ray ray{};

        __try {
            Memory::Call<bool>(g_traceRayEntity, manager,
                reinterpret_cast<uintptr_t>(&ray),
                reinterpret_cast<uintptr_t>(&start),
                reinterpret_cast<uintptr_t>(&end),
                targetEntity,
                reinterpret_cast<uintptr_t>(&filter),
                reinterpret_cast<uintptr_t>(&result));
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
        return result;
    }

    void SetupTrace(TraceData* traceData, const math::vector3& start, const math::vector3& delta, const Filter& filter, int penetrationCount, bool traceWorld) {
        if (!g_traceBulletDataInit || !traceData) return;
        __try {
            Memory::Call<void>(g_traceBulletDataInit,
                reinterpret_cast<uintptr_t>(traceData),
                reinterpret_cast<uintptr_t>(&start),
                reinterpret_cast<uintptr_t>(&delta),
                reinterpret_cast<uintptr_t>(&filter),
                static_cast<uintptr_t>(penetrationCount),
                static_cast<uintptr_t>(traceWorld ? 1 : 0));
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    void InitResult(Result* result) {
        if (!g_traceBulletFree || !result) return;
        __try {
            Memory::Call<void>(g_traceBulletFree, reinterpret_cast<uintptr_t>(result));
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    void FinalizeTrace(TraceData* traceData, Result* result, float unknown, void* unk) {
        if (!g_traceBulletUpdate || !traceData || !result) return;
        __try {
            Memory::Call<void>(g_traceBulletUpdate,
                reinterpret_cast<uintptr_t>(traceData),
                reinterpret_cast<uintptr_t>(result),
                unknown,
                reinterpret_cast<uintptr_t>(unk));
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    bool IsVisible(const math::vector3& start, const math::vector3& end, uintptr_t targetEntity, uintptr_t skipEntity, uintptr_t mask) {
        auto currentStart = start;
        auto skip = skipEntity;

        for (int i = 0; i < 3; i++) {
            auto filter = MakeFilter(skip, mask, 4);
            auto result = Trace(currentStart, end, filter);

            if (result.hitEntity == targetEntity || result.fraction > 0.97f) return true;
            if (!result.hitEntity) break;

            int health = 0;
            __try { health = *reinterpret_cast<int*>(result.hitEntity + OffsetConfig::fields.m_iHealth); }
            __except (EXCEPTION_EXECUTE_HANDLER) { break; }

            if (health > 0 && health <= 100) {
                skip = result.hitEntity;
                auto dir = (end - currentStart).normalized();
                currentStart = result.endPos + dir;
                continue;
            }

            break;
        }

        return false;
    }
}