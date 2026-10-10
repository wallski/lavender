#pragma once
#include <cstdint>
#include <cstddef>
#include "../math/math.hpp"

namespace Tracing {

    struct Filter {
        uintptr_t vtable;
        uintptr_t mask;
        int64_t v1[2];
        int skipHandles[4];
        int16_t collisions[2];
        int16_t v2;
        uint8_t layer;
        uint8_t flags;
        uint8_t v5;
        uint8_t v6;
        uint8_t pad0[6];
        char v7;
    };

    struct Ray {
        math::vector3 mins;
        math::vector3 maxs;
        uint8_t pad0[0x10];
        uint8_t type;
        uint8_t pad1[7];
    };

    struct Result {
        void* surface;
        uintptr_t hitEntity;
        void* hitboxData;
        uint8_t pad0[0x38];
        uint32_t contents;
        uint8_t pad1[0x24];
        math::vector3 startPos;
        math::vector3 endPos;
        math::vector3 normal;
        math::vector3 position;
        uint8_t pad2[0x4];
        float fraction;
        uint8_t pad3[0x6];
        bool allSolid;
        uint8_t pad4[0x4d];
    };

    struct TraceArrayElement {
        uint8_t pad0[0x38];
    };

    struct TraceData {
        int unknown1;
        float unknown2;
        void* arrayPointer;
        int unknown3;
        int unknown4;
        TraceArrayElement elements[0x80];
        uint8_t pad0[0x8];
        int numHits;
        int unknown5;
        void* hitArrayPointer;
        int hitCapacity;
        int hitFlags;
        uint8_t hitElements[0xc0];
        math::vector3 start;
        math::vector3 direction;
        float fraction;
        bool unknown6;
        uint8_t pad1[0x4b];
    };

    static_assert(offsetof(TraceData, elements) == 0x18);
    static_assert(offsetof(TraceData, numHits) == 0x1c20);
    static_assert(offsetof(TraceData, hitArrayPointer) == 0x1c28);
    static_assert(offsetof(TraceData, start) == 0x1cf8);
    static_assert(sizeof(TraceData) == 0x1d60);

    bool Initialize();
    void Shutdown();

    Result Trace(const math::vector3& start, const math::vector3& end, const Filter& filter);
    Result TraceHull(const math::vector3& start, const math::vector3& end, const math::vector3& mins, const math::vector3& maxs, const Filter& filter);
    Result TraceSphere(const math::vector3& start, const math::vector3& end, float radius, const Filter& filter);
    Result TraceToEntity(const math::vector3& start, const math::vector3& end, uintptr_t targetEntity, const Filter& filter);

    Filter MakeFilter(uintptr_t skipEntity, uintptr_t mask, uint8_t layer, int type);
    Filter MakeFilter(uintptr_t skipEntity, uintptr_t mask, uint8_t layer);

    void SetupTrace(TraceData* traceData, const math::vector3& start, const math::vector3& delta, const Filter& filter, int penetrationCount, bool traceWorld);
    void InitResult(Result* result);
    void FinalizeTrace(TraceData* traceData, Result* result, float unknown, void* unk);

    bool IsVisible(const math::vector3& start, const math::vector3& end, uintptr_t targetEntity, uintptr_t skipEntity, uintptr_t mask = 0x1c3003);
}