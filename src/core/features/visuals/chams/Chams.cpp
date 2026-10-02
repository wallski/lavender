#include "pch.h"
#include "Chams.h"

static volatile LONG g_enabled = 0;
static float g_color[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
static uintptr_t g_entityListGlobal = 0;
static uintptr_t g_localPawnGlobal = 0;
static volatile LONG64 g_tintCount = 0;
static volatile LONG64 g_scannedCount = 0;

namespace Chams {

    static uintptr_t __TryReadPtr(uintptr_t addr) {
        __try { return *reinterpret_cast<uintptr_t*>(addr); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
    }

    static uint32_t __TryReadU32(uintptr_t addr) {
        __try { return *reinterpret_cast<uint32_t*>(addr); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
    }

    static void __TryWriteU32(uintptr_t addr, uint32_t value) {
        __try { *reinterpret_cast<uint32_t*>(addr) = value; }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    static void __TryGetSchemaName(uintptr_t entity, char* outBuf, size_t outSize) {
        outBuf[0] = '\0';
        __try {
            uintptr_t identity = *reinterpret_cast<uintptr_t*>(entity + 0x10);
            if (!identity) return;
            uintptr_t entityClass = *reinterpret_cast<uintptr_t*>(identity + 0x8);
            if (!entityClass) return;
            uintptr_t classInfo = *reinterpret_cast<uintptr_t*>(entityClass + 0x58);
            if (!classInfo) return;
            const char* namePtr = *reinterpret_cast<const char**>(classInfo + 0x8);
            if (!namePtr) return;
            size_t i = 0;
            while (i < outSize - 1 && namePtr[i] != '\0') { outBuf[i] = namePtr[i]; i++; }
            outBuf[i] = '\0';
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            strncpy_s(outBuf, outSize, "(faulted)", outSize - 1);
        }
    }

    static uintptr_t LookupEntity(uint32_t handle, uintptr_t entityListGlobal) {
        if (!handle || handle == 0xffffffff) return 0;
        uintptr_t entityList = __TryReadPtr(entityListGlobal);
        if (!entityList) return 0;
        uintptr_t listEntry = __TryReadPtr(entityList + ((handle & 0x7fff) >> 9) * 8 + 0x10);
        if (!listEntry || listEntry == 0xffffffffffffffffULL) return 0;
        uintptr_t entity = __TryReadPtr(listEntry + (handle & 0x1ff) * 112);
        if (!entity || entity == 0xffffffffffffffffULL || entity < 0x10000) return 0;
        return entity;
    }

    void SetEntityListGlobal(uintptr_t addr) { g_entityListGlobal = addr; }
    void SetLocalPawnGlobal(uintptr_t addr) { g_localPawnGlobal = addr; }

    bool IsEnabled() { return g_enabled != 0; }
    void SetEnabled(bool enabled) { InterlockedExchange(&g_enabled, enabled ? 1 : 0); }

    float* GetColor() { return g_color; }

    void SetColor(const float color[4]) {
        g_color[0] = color[0];
        g_color[1] = color[1];
        g_color[2] = color[2];
        g_color[3] = color[3];
    }

    int64_t GetTintCount() { return g_tintCount; }
    int64_t GetScannedCount() { return g_scannedCount; }

    void OnSortCall(void* array, uint32_t count) {
        if (g_enabled == 0) return;
        uintptr_t arr = (uintptr_t)array;
        if (arr < 0x10000) return;

        uint8_t r = (uint8_t)(g_color[0] * 255.0f);
        uint8_t g = (uint8_t)(g_color[1] * 255.0f);
        uint8_t b = (uint8_t)(g_color[2] * 255.0f);
        uint8_t a = (uint8_t)(g_color[3] * 255.0f);
        uint32_t packed = (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16) | ((uint32_t)a << 24);

        uintptr_t localPawn = __TryReadPtr(g_localPawnGlobal);
        uint32_t maxCheck = count > 300 ? 300 : count;

        for (uint32_t i = 0; i < maxCheck; i++) {
            uintptr_t prim = arr + (uintptr_t)i * 0x70;
            InterlockedIncrement64(&g_scannedCount);

            uintptr_t sceneObj = __TryReadPtr(prim + 0x18);
            if (!sceneObj) continue;

            uint32_t handle = __TryReadU32(sceneObj + 0xc0);
            uintptr_t ent = LookupEntity(handle, g_entityListGlobal);
            if (!ent) continue;
            if (ent == localPawn) continue;

            char name[128];
            __TryGetSchemaName(ent, name, sizeof(name));
            if (name[0] != 'C') continue;
            if (strcmp(name, "C_CSPlayerPawn") != 0) continue;

            __TryWriteU32(prim + 0x50, packed);
            InterlockedIncrement64(&g_tintCount);
        }
    }
}