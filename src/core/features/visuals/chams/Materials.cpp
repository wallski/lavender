#include "pch.h"
#include "Materials.h"
#include "../../../memory/PatternScan.h"

namespace Materials {

    struct KeyValues3 {
        uint8_t data[0x50];
    };

    struct KV3ID_t {
        const char* name;
        uint64_t version_lo;
        uint64_t version_hi;
    };

    // CS2 CStrongHandle<T> is a single pointer to internal bind struct.
    // The material pointer itself lives at *handle.binding.
    struct StrongHandle {
        uintptr_t binding;
    };

    static uintptr_t g_setType = 0;
    static uintptr_t g_destroy = 0;
    static uintptr_t g_loadKV3 = 0;
    static uintptr_t g_createMaterial = 0;
    static uintptr_t g_materials[(int)Id::Count] = {};

    static const char* g_names[(int)Id::Count] = {
        "None",
        "Matte", "Flat", "Glow", "Outlines", "Bloom",
        "Matte (ignorez)", "Flat (ignorez)", "Glow (ignorez)", "Outlines (ignorez)", "Bloom (ignorez)"
    };

    static const char* g_paths[(int)Id::Count] = {
        "",
        "materials/dev/lavender_matte.vmat",
        "materials/dev/lavender_flat.vmat",
        "materials/dev/lavender_glow.vmat",
        "materials/dev/lavender_outlines.vmat",
        "materials/dev/lavender_bloom.vmat",
        "materials/dev/lavender_matte_ignorez.vmat",
        "materials/dev/lavender_flat_ignorez.vmat",
        "materials/dev/lavender_glow_ignorez.vmat",
        "materials/dev/lavender_outlines_ignorez.vmat",
        "materials/dev/lavender_bloom_ignorez.vmat"
    };

    static const char* kMatteKV = R"KV(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "generic.vfx"
    g_vColorTint = [1.0, 1.0, 1.0, 1.0]
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_7652cb.vtex"
})KV";

    static const char* kFlatKV = R"KV(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_unlitgeneric.vfx"
    F_RENDER_BACKFACES = 0
    F_DISABLE_Z_BUFFERING = 0
    F_PAINT_VERTEX_COLORS = 1
    F_TRANSLUCENT = 1
    F_BLEND_MODE = 1
    g_vColorTint = [1.0, 1.0, 1.0, 1.0]
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_7652cb.vtex"
})KV";

    static const char* kFlatIgnoreZKV = R"KV(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_unlitgeneric.vfx"
    F_RENDER_BACKFACES = 0
    F_DISABLE_Z_BUFFERING = 1
    F_DISABLE_Z_WRITE = 1
    F_PAINT_VERTEX_COLORS = 1
    F_TRANSLUCENT = 1
    F_BLEND_MODE = 1
    g_vColorTint = [1.0, 1.0, 1.0, 1.0]
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_7652cb.vtex"
})KV";

    static const char* kGlowKV = R"KV(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_effects.vfx"
    F_ADDITIVE_BLEND = 1
    F_BLEND_MODE = 1
    F_TRANSLUCENT = 1
    F_IGNOREZ = 0
    F_DISABLE_Z_BUFFERING = 0
    F_RENDER_BACKFACES = 0
    g_vColorTint = [1.0, 1.0, 1.0, 1.0]
    g_flFresnelExponent = 1.5
    g_flFresnelFalloff = 5.0
    g_flFresnelMax = 0.0
    g_flFresnelMin = 1.0
    g_flColorBoost = 20.0
    g_flOpacityScale = 0.6
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tMask1 = resource:"materials/default/default_mask_tga_344101f8.vtex"
    g_tMask2 = resource:"materials/default/default_mask_tga_344101f8.vtex"
    g_tMask3 = resource:"materials/default/default_mask_tga_344101f8.vtex"
})KV";

    static const char* kGlowIgnoreZKV = R"KV(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_effects.vfx"
    F_ADDITIVE_BLEND = 1
    F_BLEND_MODE = 1
    F_TRANSLUCENT = 1
    F_IGNOREZ = 1
    F_DISABLE_Z_BUFFERING = 1
    F_DISABLE_Z_WRITE = 1
    F_RENDER_BACKFACES = 0
    g_vColorTint = [1.0, 1.0, 1.0, 1.0]
    g_flFresnelExponent = 1.5
    g_flFresnelFalloff = 5.0
    g_flFresnelMax = 0.0
    g_flFresnelMin = 1.0
    g_flColorBoost = 20.0
    g_flOpacityScale = 0.6
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tMask1 = resource:"materials/default/default_mask_tga_344101f8.vtex"
    g_tMask2 = resource:"materials/default/default_mask_tga_344101f8.vtex"
    g_tMask3 = resource:"materials/default/default_mask_tga_344101f8.vtex"
})KV";

    static const char* kOutlinesKV = R"KV(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_effects.vfx"
    F_ADDITIVE_BLEND = 1
    F_BLEND_MODE = 1
    F_TRANSLUCENT = 1
    g_vColorTint = [1.0, 1.0, 1.0, 0.0]
    g_flOpacityScale = 0.45
    g_flFresnelExponent = 0.75
    g_flFresnelFalloff = 1.0
    g_flFresnelMax = 0.0
    g_flFresnelMin = 1.0
    g_flColorBoost = 2.25
    g_flToolsVisCubemapReflectionRoughness = 1.0
    g_flBeginMixingRoughness = 1.0
    g_tColor = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tMask1 = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tMask2 = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tMask3 = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tSceneDepth = resource:"materials/default/default_mask_tga_fde710a5.vtex"
})KV";

    static const char* kOutlinesIgnoreZKV = R"KV(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_effects.vfx"
    F_ADDITIVE_BLEND = 1
    F_BLEND_MODE = 1
    F_TRANSLUCENT = 1
    F_DISABLE_Z_BUFFERING = 1
    F_DISABLE_Z_WRITE = 1
    g_vColorTint = [1.0, 1.0, 1.0, 0.0]
    g_flOpacityScale = 0.45
    g_flFresnelExponent = 0.75
    g_flFresnelFalloff = 1.0
    g_flFresnelMax = 0.0
    g_flFresnelMin = 1.0
    g_flColorBoost = 2.25
    g_flToolsVisCubemapReflectionRoughness = 1.0
    g_flBeginMixingRoughness = 1.0
    g_tColor = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tMask1 = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tMask2 = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tMask3 = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tSceneDepth = resource:"materials/default/default_mask_tga_fde710a5.vtex"
})KV";

    static const char* kBloomKV = R"KV(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "solidcolor.vfx"
    F_DISABLE_Z_WRITE = 0
    g_vColorTint = [8.0, 8.0, 8.0]
})KV";

    static const char* kBloomIgnoreZKV = R"KV(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "solidcolor.vfx"
    F_IGNOREZ = 1
    F_DISABLE_Z_WRITE = 1
    g_vColorTint = [5.0, 5.0, 5.0]
})KV";

    static const char* GetKV(Id id) {
        switch (id) {
        case Id::Matte:           return kMatteKV;
        case Id::Flat:            return kFlatKV;
        case Id::Glow:            return kGlowKV;
        case Id::Outlines:        return kOutlinesKV;
        case Id::Bloom:           return kBloomKV;
        case Id::MatteIgnoreZ:    return kFlatIgnoreZKV;
        case Id::FlatIgnoreZ:     return kFlatIgnoreZKV;
        case Id::GlowIgnoreZ:     return kGlowIgnoreZKV;
        case Id::OutlinesIgnoreZ: return kOutlinesIgnoreZKV;
        case Id::BloomIgnoreZ:    return kBloomIgnoreZKV;
        default:                  return nullptr;
        }
    }

    static uintptr_t LoadMaterial(const char* kv, const char* name) {
        if (!g_setType || !g_loadKV3 || !g_destroy || !g_createMaterial) {
            printf("[Materials]   [%s] missing fn pointers\n", name);
            return 0;
        }

        KeyValues3 kv3{};
        auto setResult = Memory::Call<KeyValues3*>(g_setType, &kv3, 1u, 6u);
        if (setResult != &kv3) {
            printf("[Materials]   [%s] SetType FAILED (ret=0x%llX expected=0x%llX)\n",
                name, (unsigned long long)setResult, (unsigned long long) & kv3);
            return 0;
        }

        KV3ID_t id{ "generic", 0x41B818518343427E, 0xB5F447C23C0CDF8C };
        const bool loaded = Memory::Call<bool>(g_loadKV3, &kv3, nullptr, kv, &id, nullptr, 0u);
        if (!loaded) {
            printf("[Materials]   [%s] LoadKV3 FAILED\n", name);
            Memory::Call<void>(g_destroy, &kv3, 0u);
            return 0;
        }

        StrongHandle handle{};
        auto ret = Memory::Call<uint64_t>(g_createMaterial,
            (void*)nullptr,
            (void*)&handle,
            (void*)name,
            (void*)&kv3,
            (uint64_t)0,
            (uint64_t)1);

        printf("[Materials]   [%s] CreateMaterial ret=0x%llX handle.binding=0x%llX\n",
            name, (unsigned long long)ret, (unsigned long long)handle.binding);

        Memory::Call<void>(g_destroy, &kv3, 0u);

        if (!handle.binding) {
            printf("[Materials]   [%s] CreateMaterial FAILED (null binding)\n", name);
            return 0;
        }

        auto material = *reinterpret_cast<uintptr_t*>(handle.binding);
        if (!material) {
            printf("[Materials]   [%s] binding ptr is non-null but *binding=0\n", name);
            return 0;
        }

        printf("[Materials]   [%s] OK -> 0x%llX\n", name, (unsigned long long)material);
        return material;
    }

    bool Initialize() {
        g_setType = Memory::FindPattern("tier0.dll",
            "40 53 48 83 EC 30 80 FA 06 0F B6 C2 41 B9 16");
        g_destroy = Memory::FindPattern("tier0.dll",
            "40 57 41 57 48 83 EC 38 4C 8B 01 44 8B FA 49 8B C0 48 8B F9 48 C1 E8 02");
        g_loadKV3 = Memory::GetExport("tier0.dll",
            "?LoadKV3@@YA_NPEAVKeyValues3@@PEAVCUtlString@@PEBDAEBUKV3ID_t@@2I@Z");
        g_createMaterial = Memory::FindPattern("materialsystem2.dll",
            "48 89 5C 24 ?? 48 89 6C 24 ?? 48 89 74 24 ?? 48 89 7C 24 ?? 41 56 48 81 EC ?? ?? ?? ?? 48 8B 05 ?? ?? ?? ?? 48 8B F2");

        HMODULE matMod = GetModuleHandleA("materialsystem2.dll");
        printf("[Materials] materialsystem2.dll = 0x%p\n", matMod);
        printf("[Materials] kv3_set_type    = 0x%llX\n", (unsigned long long)g_setType);
        printf("[Materials] kv3_destroy     = 0x%llX\n", (unsigned long long)g_destroy);
        printf("[Materials] kv3_load        = 0x%llX\n", (unsigned long long)g_loadKV3);
        printf("[Materials] material_create = 0x%llX\n", (unsigned long long)g_createMaterial);

        if (!g_setType || !g_destroy || !g_loadKV3 || !g_createMaterial) {
            printf("[Materials] FATAL: one or more pointers null\n");
            return false;
        }

        int loaded = 0;
        for (int i = 1; i < (int)Id::Count; i++) {
            const auto kv = GetKV((Id)i);
            if (!kv) continue;
            g_materials[i] = LoadMaterial(kv, g_paths[i]);
            if (g_materials[i]) loaded++;
        }

        printf("[Materials] %d/%d materials loaded\n", loaded, (int)Id::Count - 1);
        return loaded > 0;
    }

    void Shutdown() {
        for (auto& m : g_materials) m = 0;
    }

    uintptr_t Get(Id id) {
        const int i = (int)id;
        if (i < 0 || i >= (int)Id::Count) return 0;
        return g_materials[i];
    }

    const char* GetName(Id id) {
        const int i = (int)id;
        if (i < 0 || i >= (int)Id::Count) return "Unknown";
        return g_names[i];
    }

    int GetCount() {
        return (int)Id::Count;
    }
}