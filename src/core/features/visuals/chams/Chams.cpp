#include "pch.h"
#include "Chams.h"
#include "../../../../config/OffsetConfig.h"
#include "../../../cvar/CVar.h"

static volatile LONG g_enabled = 0;
static volatile LONG g_teamCheck = 0;
static volatile LONG g_material = (LONG)Materials::Id::Flat;
static volatile LONG g_occluded = 0;
static float g_color[4] = { 0.0f, 1.0f, 0.4f, 1.0f };
static uintptr_t g_entityListGlobal = 0;
static uintptr_t g_localPawnGlobal = 0;
static volatile LONG64 g_tintCount = 0;

namespace Chams {

    static uintptr_t __TryReadPtr(uintptr_t addr) {
        __try { return *reinterpret_cast<uintptr_t*>(addr); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
    }

    static uint32_t __TryReadU32(uintptr_t addr) {
        __try { return *reinterpret_cast<uint32_t*>(addr); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
    }

    static int __TryReadInt(uintptr_t addr) {
        __try { return *reinterpret_cast<int*>(addr); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
    }

    static void __TryWriteU32(uintptr_t addr, uint32_t value) {
        __try { *reinterpret_cast<uint32_t*>(addr) = value; }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    static void __TryWritePtr(uintptr_t addr, uintptr_t value) {
        __try { *reinterpret_cast<uintptr_t*>(addr) = value; }
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

    static uint32_t PackColor(const float color[4]) {
        uint8_t r = (uint8_t)(color[0] * 255.0f);
        uint8_t g = (uint8_t)(color[1] * 255.0f);
        uint8_t b = (uint8_t)(color[2] * 255.0f);
        uint8_t a = (uint8_t)(color[3] * 255.0f);
        return (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16) | ((uint32_t)a << 24);
    }

    static Materials::Id ResolveMaterial() {
        const int base = (int)g_material;
        if (base <= 0 || base > 5) return Materials::Id::None;
        if (g_occluded != 0) {
            return (Materials::Id)(base + 5);
        }
        return (Materials::Id)base;
    }

    void SetEntityListGlobal(uintptr_t addr) { g_entityListGlobal = addr; }
    void SetLocalPawnGlobal(uintptr_t addr) { g_localPawnGlobal = addr; }

    bool IsEnabled() { return g_enabled != 0; }
    void SetEnabled(bool enabled) { InterlockedExchange(&g_enabled, enabled ? 1 : 0); }

    bool IsTeamCheck() { return g_teamCheck != 0; }
    void SetTeamCheck(bool enabled) { InterlockedExchange(&g_teamCheck, enabled ? 1 : 0); }

    Materials::Id GetMaterial() { return (Materials::Id)g_material; }
    void SetMaterial(Materials::Id id) { InterlockedExchange(&g_material, (LONG)id); }

    bool IsOccluded() { return g_occluded != 0; }
    void SetOccluded(bool occluded) { InterlockedExchange(&g_occluded, occluded ? 1 : 0); }

    float* GetColor() { return g_color; }

    void SetColor(const float color[4]) {
        g_color[0] = color[0];
        g_color[1] = color[1];
        g_color[2] = color[2];
        g_color[3] = color[3];
    }

    int64_t GetTintCount() { return g_tintCount; }
    int64_t GetScannedCount() { return g_tintCount; }

    void ApplyLodOverride() {
        struct Entry {
            const char* name;
            float value;
        };

        const Entry floatCvars[] = {
            { "sc_instanced_mesh_lod_bias", 100.0f },
            { "sc_instanced_mesh_lod_bias_shadow", 100.0f },
            { "sc_instanced_mesh_size_cull_bias", 100.0f },
            { "sc_instanced_mesh_size_cull_bias_shadow", 100.0f },
            { "sc_screen_size_lod_scale_override", 1.0f },
            { "sc_dithered_lod_transition_amt", 0.0f },
            { "r_texture_lod_scale", 0.0f },
            { "r_fallback_texture_lod_scale", 0.0f }
        };

        for (const auto& e : floatCvars) {
            CConVar* c = CVar::Find(e.name);
            if (!c) {
                printf("[Chams] cvar %s NOT FOUND\n", e.name);
                continue;
            }
            float before = c->Get<float>();
            c->Set(e.value);
            float after = c->Get<float>();
            printf("[Chams] %s %.3f -> %.3f\n", e.name, before, after);
        }

        const Entry intCvars[] = {
            { "sc_force_lod_level", 0.0f },
            { "sc_no_cull", 1.0f },
            { "sc_disable_culling_boxes", 1.0f },
            { "sc_skip_traversal", 0.0f },
            { "sc_aggregate_gpu_culling", 0.0f },
            { "sc_aggregate_gpu_occlusion_culling", 0.0f },
            { "sc_instanced_mesh_gpu_culling", 0.0f },
            { "sc_instanced_mesh_gpu_occlusion_culling", 0.0f },
            { "sc_instanced_mesh_gpu_density_culling", 0.0f },
            { "sc_instanced_mesh_gpu_vis_culling", 0.0f },
            { "sc_mesh_gpu_occlusion_culling", 0.0f },
            { "sc_mesh_gpu_volume_culling", 0.0f },
            { "sc_allow_dithered_lod", 0.0f },
            { "skeleton_instance_lod_optimization", 0.0f }
        };

        for (const auto& e : intCvars) {
            CConVar* c = CVar::Find(e.name);
            if (!c) {
                printf("[Chams] cvar %s NOT FOUND\n", e.name);
                continue;
            }
            int before = c->Get<int>();
            c->Set((int)e.value);
            int after = c->Get<int>();
            printf("[Chams] %s %d -> %d\n", e.name, before, after);
        }

        fflush(stdout);
    }

    void OnSortCall(void* array, uint32_t count) {
        if (g_enabled == 0) return;
        uintptr_t arr = (uintptr_t)array;
        if (arr < 0x10000) return;
        if (count > 65536) count = 65536;

        const Materials::Id resolved = ResolveMaterial();
        const uintptr_t material = Materials::Get(resolved);
        const uint32_t packed = PackColor(g_color);

        uintptr_t localPawn = __TryReadPtr(g_localPawnGlobal);

        int localTeam = 0;
        if (g_teamCheck != 0 && localPawn) {
            localTeam = __TryReadInt(localPawn + OffsetConfig::fields.m_iTeamNum);
        }

        int64_t tinted = 0;

        for (uint32_t i = 0; i < count; i++) {
            uintptr_t prim = arr + (uintptr_t)i * 0x70;

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

            if (localTeam != 0) {
                int entTeam = __TryReadInt(ent + OffsetConfig::fields.m_iTeamNum);
                if (entTeam == localTeam) continue;
            }

            if (material) {
                __TryWritePtr(prim + 0x20, material);
                __TryWritePtr(prim + 0x28, material);
            }
            __TryWriteU32(prim + 0x50, packed);
            tinted++;
        }

        if (tinted > 0) {
            InterlockedAdd64(&g_tintCount, tinted);
        }
    }
}