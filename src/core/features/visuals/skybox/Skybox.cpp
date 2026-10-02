#include "pch.h"
#include "Skybox.h"
#include "../chams/Materials.h"
#include "../../../../config/OffsetConfig.h"
#include "../../../../core/memory/PatternScan.h"
#include "../../../../core/interfaces/InterfaceManager.h"

namespace Skybox {

    static const Entry g_builtin[] = {
        { "Map default",  "" },
        { "Vertigo",      "materials/skybox/sky_de_vertigo_exr_c70a3937.vtex" },
        { "Mirage",       "materials/skybox/sky_de_mirage_exr_71e5f2a1.vtex" },
        { "Dust 2",       "materials/skybox/sky_de_dust2_exr_908a35ba.vtex" },
        { "Nuke",         "materials/skybox/sky_de_nuke_exr_f04e84b2.vtex" },
        { "Anubis",       "materials/skybox/sky_de_annubis_exr_2c5e0b53.vtex" },
        { "Overpass",     "materials/skybox/sky_de_overpass_01_exr_f8534391.vtex" },
        { "Train",        "materials/skybox/sky_de_train03_exr_4fdb8a38.vtex" },
        { "Aztec",        "materials/skybox/sky_hr_aztec_02_exr_f84f8de9.vtex" },
        { "Italy",        "materials/skybox/cs_italy_s2_skybox_sunset_2_exr_e56cedf6.vtex" },
        { "Office",       "materials/skybox/sky_cs_office_45_0_exr_d0152542.vtex" },
        { "Cloudy",       "materials/skybox/sky_csgo_cloudy01_cube_pfm_f9a0b177.vtex" },
        { "Rain Night",   "materials/skybox/sky_rain_night_01_exr_8d775aee.vtex" },
        { "Daylight",     "materials/skybox/sky_cs15_daylight01_hdr_cube_pfm_a4b050d1.vtex" },
        { "Jungle",       "materials/skybox/jungle_cube_pfm_bc16d813.vtex" },
        { "Sunset",       "materials/skybox/tests/src/lightingtest_sky_sunset_light_exr_f7b19a45.vtex" }
    };

    static std::vector<Entry> g_entries;
    static std::vector<uintptr_t> g_materials;
    static volatile LONG g_enabled = 0;
    static volatile LONG g_selected = 0;
    static volatile LONG g_materials_ready = 0;

    static uintptr_t g_resource_system = 0;
    static uintptr_t g_fn_init_path_buffer = 0;
    static uintptr_t g_fn_precache_resource = 0;

    static uintptr_t g_draw_hook_fn = 0;
    static uint8_t* g_draw_stub = nullptr;
    static uint8_t g_draw_original_prologue[5] = { 0 };
    static int32_t g_draw_original_jle_rel = 0;

    static uintptr_t g_fsn_hook_fn = 0;
    static uint8_t* g_fsn_stub = nullptr;
    static uint8_t g_fsn_original_prologue[5] = { 0 };

    static uintptr_t g_last_written = 0;
    static uintptr_t g_original_material = 0;

    static uint32_t HashPath(const char* s) {
        uint32_t h = 2166136261u;
        while (*s) {
            h ^= (uint32_t)(uint8_t)(*s++);
            h *= 16777619u;
        }
        return h;
    }

    static void* AllocNear(void* hint, size_t size) {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        uintptr_t gran = si.dwAllocationGranularity;
        uintptr_t base = (uintptr_t)hint & ~(gran - 1);
        for (int i = 1; i < 0x1000; i++) {
            uintptr_t addr = base - (uintptr_t)i * gran;
            void* mem = VirtualAlloc((void*)addr, size, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
            if (mem) return mem;
        }
        for (int i = 1; i < 0x1000; i++) {
            uintptr_t addr = base + (uintptr_t)i * gran;
            void* mem = VirtualAlloc((void*)addr, size, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
            if (mem) return mem;
        }
        return VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    }

    extern "C" void SkyboxCapture(void* mesh_array, uint32_t count) {
        OnSkyboxDraw(mesh_array, count);
    }

    void TryPrecache();

    extern "C" void SkyboxFSNHandler() {
        TryPrecache();
    }

    static uint8_t* WriteSkyboxStub(uintptr_t fn) {
        uint8_t* stub = (uint8_t*)AllocNear((void*)fn, 128);
        if (!stub) return nullptr;
        uint8_t* p = stub;

        *p++ = 0x51;
        *p++ = 0x52;
        *p++ = 0x41; *p++ = 0x50;
        *p++ = 0x41; *p++ = 0x51;

        *p++ = 0x48; *p++ = 0x83; *p++ = 0xEC; *p++ = 0x28;

        *p++ = 0x4C; *p++ = 0x89; *p++ = 0xC1;
        *p++ = 0x44; *p++ = 0x89; *p++ = 0xCA;

        *p++ = 0x48; *p++ = 0xB8;
        *(uint64_t*)p = (uint64_t)&SkyboxCapture; p += 8;
        *p++ = 0xFF; *p++ = 0xD0;

        *p++ = 0x48; *p++ = 0x83; *p++ = 0xC4; *p++ = 0x28;

        *p++ = 0x41; *p++ = 0x59;
        *p++ = 0x41; *p++ = 0x58;
        *p++ = 0x5A;
        *p++ = 0x59;

        *p++ = 0x45; *p++ = 0x85; *p++ = 0xC9;

        *p++ = 0x0F; *p++ = 0x8E;
        int32_t jle_rel = (int32_t)((fn + 9 + g_draw_original_jle_rel) - ((uintptr_t)p + 4));
        *(int32_t*)p = jle_rel; p += 4;

        *p++ = 0x48; *p++ = 0xB8;
        *(uint64_t*)p = fn + 9; p += 8;
        *p++ = 0xFF; *p++ = 0xE0;

        return stub;
    }

    static bool InstallSkyboxDrawHook(uintptr_t fn) {
        memcpy(g_draw_original_prologue, (void*)fn, 5);
        if (g_draw_original_prologue[0] != 0x45 || g_draw_original_prologue[1] != 0x85) return false;
        if (g_draw_original_prologue[2] != 0xC9 || g_draw_original_prologue[3] != 0x0F) return false;
        if (g_draw_original_prologue[4] != 0x8E) return false;

        g_draw_original_jle_rel = *(int32_t*)(fn + 5);

        g_draw_stub = WriteSkyboxStub(fn);
        if (!g_draw_stub) return false;

        intptr_t rel = (intptr_t)g_draw_stub - (intptr_t)(fn + 5);
        if (rel < INT32_MIN || rel > INT32_MAX) return false;

        DWORD old = 0;
        if (!VirtualProtect((void*)fn, 5, PAGE_EXECUTE_READWRITE, &old)) return false;
        uint8_t* patch = (uint8_t*)fn;
        patch[0] = 0xE9;
        *(int32_t*)(patch + 1) = (int32_t)rel;
        VirtualProtect((void*)fn, 5, old, &old);
        FlushInstructionCache(GetCurrentProcess(), (void*)fn, 5);
        return true;
    }

    static uint8_t* WriteFSNStub(uintptr_t fn) {
        uint8_t* stub = (uint8_t*)AllocNear((void*)fn, 128);
        if (!stub) return nullptr;
        uint8_t* p = stub;

        *p++ = 0x51;
        *p++ = 0x52;
        *p++ = 0x48; *p++ = 0x83; *p++ = 0xEC; *p++ = 0x28;

        *p++ = 0x48; *p++ = 0xB8;
        *(uint64_t*)p = (uint64_t)&SkyboxFSNHandler; p += 8;
        *p++ = 0xFF; *p++ = 0xD0;

        *p++ = 0x48; *p++ = 0x83; *p++ = 0xC4; *p++ = 0x28;
        *p++ = 0x5A;
        *p++ = 0x59;

        *p++ = 0x48; *p++ = 0x89; *p++ = 0x5C; *p++ = 0x24; *p++ = 0x18;

        *p++ = 0x48; *p++ = 0xB8;
        *(uint64_t*)p = fn + 5; p += 8;
        *p++ = 0xFF; *p++ = 0xE0;

        return stub;
    }

    static bool InstallFSNHook(uintptr_t fn) {
        memcpy(g_fsn_original_prologue, (void*)fn, 5);
        if (g_fsn_original_prologue[0] != 0x48 || g_fsn_original_prologue[1] != 0x89) return false;
        if (g_fsn_original_prologue[2] != 0x5C || g_fsn_original_prologue[3] != 0x24) return false;

        g_fsn_stub = WriteFSNStub(fn);
        if (!g_fsn_stub) return false;

        intptr_t rel = (intptr_t)g_fsn_stub - (intptr_t)(fn + 5);
        if (rel < INT32_MIN || rel > INT32_MAX) return false;

        DWORD old = 0;
        if (!VirtualProtect((void*)fn, 5, PAGE_EXECUTE_READWRITE, &old)) return false;
        uint8_t* patch = (uint8_t*)fn;
        patch[0] = 0xE9;
        *(int32_t*)(patch + 1) = (int32_t)rel;
        VirtualProtect((void*)fn, 5, old, &old);
        FlushInstructionCache(GetCurrentProcess(), (void*)fn, 5);
        return true;
    }

    static void UninstallHooks() {
        if (g_draw_hook_fn) {
            DWORD old = 0;
            if (VirtualProtect((void*)g_draw_hook_fn, 5, PAGE_EXECUTE_READWRITE, &old)) {
                memcpy((void*)g_draw_hook_fn, g_draw_original_prologue, 5);
                VirtualProtect((void*)g_draw_hook_fn, 5, old, &old);
                FlushInstructionCache(GetCurrentProcess(), (void*)g_draw_hook_fn, 5);
            }
        }
        if (g_draw_stub) { VirtualFree(g_draw_stub, 0, MEM_RELEASE); g_draw_stub = nullptr; }

        if (g_fsn_hook_fn) {
            DWORD old = 0;
            if (VirtualProtect((void*)g_fsn_hook_fn, 5, PAGE_EXECUTE_READWRITE, &old)) {
                memcpy((void*)g_fsn_hook_fn, g_fsn_original_prologue, 5);
                VirtualProtect((void*)g_fsn_hook_fn, 5, old, &old);
                FlushInstructionCache(GetCurrentProcess(), (void*)g_fsn_hook_fn, 5);
            }
        }
        if (g_fsn_stub) { VirtualFree(g_fsn_stub, 0, MEM_RELEASE); g_fsn_stub = nullptr; }
    }

    static bool PrecacheTexture(const char* path) {
        if (!g_resource_system || !g_fn_init_path_buffer || !g_fn_precache_resource) return false;

        struct BufferString {
            uint32_t unk1;
            uint32_t unk2;
            union {
                uintptr_t str_ptr;
                uint8_t data[0xc8];
            };
            uintptr_t unk3;
            uintptr_t unk4;
        };

        BufferString buf{};
        buf.unk2 = 0xc00000c8;

        Memory::Call<void>(g_fn_init_path_buffer, &buf, path);
        buf.unk4 = 'xetv';

        Memory::Call<void>(g_fn_precache_resource, g_resource_system, &buf, "");

        Memory::Call<void>(g_fn_init_path_buffer, &buf, path);
        buf.unk4 = 'xetv';

        uintptr_t vtable = *(uintptr_t*)g_resource_system;
        auto getOrLoad = (uintptr_t(*)(uintptr_t, void*, uint64_t))(*(uintptr_t*)(vtable + 79 * 8));
        uintptr_t binding = getOrLoad(g_resource_system, &buf, 0);
        return binding != 0;
    }

    static uintptr_t BuildSkyboxMaterial(const char* vtex_path) {
        if (!vtex_path || !*vtex_path) return 0;

        PrecacheTexture(vtex_path);

        char kv[2048];
        _snprintf_s(kv, sizeof(kv), _TRUNCATE,
            "<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} "
            "format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->\n"
            "{\n"
            "    Shader = \"sky.vfx\"\n"
            "    g_flBrightnessExposureBias = 0.0\n"
            "    g_flRenderOnlyExposureBias = 0.0\n"
            "    SkyTexture = resource:\"%s\"\n"
            "    g_tSkyTexture = resource:\"%s\"\n"
            "}\n",
            vtex_path, vtex_path);

        char name[128];
        _snprintf_s(name, sizeof(name), _TRUNCATE, "materials/dev/lavender_skybox_%08x.vmat",
            HashPath(vtex_path));

        return Materials::Compile(kv, name);
    }

    static uintptr_t TryBuildMaterial(const char* path) {
        uintptr_t mat = 0;
        __try {
            mat = BuildSkyboxMaterial(path);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            mat = 0;
        }
        return mat;
    }

    bool Initialize() {
        for (const auto& e : g_builtin) g_entries.push_back(e);
        g_materials.assign(g_entries.size(), 0);

        g_resource_system = (uintptr_t)InterfaceManager::Get<void>("resourcesystem.dll", "ResourceSystem013");
        if (!g_resource_system) {
            printf("[Skybox] ResourceSystem013 not found\n");
            return false;
        }

        g_fn_init_path_buffer = Memory::FindPattern("client.dll",
            "48 89 5C 24 ?? 57 48 83 EC ?? 8B 41 ?? 48 8D 79");
        g_fn_precache_resource = Memory::FindPattern("resourcesystem.dll",
            "40 53 55 57 48 81 EC 80 00 00 00");

        if (!g_fn_init_path_buffer || !g_fn_precache_resource) {
            printf("[Skybox] precache patterns not found\n");
            return false;
        }

        uintptr_t draw_fn = Memory::FindPattern("scenesystem.dll",
            "45 85 C9 0F 8E ? ? ? ? 4C 8B DC");
        if (!draw_fn) {
            printf("[Skybox] draw_skybox_array pattern not found\n");
            return false;
        }
        g_draw_hook_fn = draw_fn;
        if (!InstallSkyboxDrawHook(draw_fn)) {
            printf("[Skybox] draw hook install failed\n");
            return false;
        }

        uintptr_t fsn_fn = Memory::FindPattern("client.dll",
            "48 89 5C 24 18 48 89 6C 24 20 57 48 83 EC 40 48 8B F9");
        if (!fsn_fn) {
            printf("[Skybox] frame_stage_notify pattern not found\n");
            return false;
        }
        g_fsn_hook_fn = fsn_fn;
        if (!InstallFSNHook(fsn_fn)) {
            printf("[Skybox] fsn hook install failed\n");
            return false;
        }

        printf("[Skybox] Ready.\n");
        return true;
    }

    void TryPrecache() {
        if (g_materials_ready != 0) return;

        static volatile LONG s_enter_guard = 0;
        if (InterlockedCompareExchange(&s_enter_guard, 1, 0) != 0) return;

        if (!OffsetConfig::globals.dwLocalPlayerPawn) {
            InterlockedExchange(&s_enter_guard, 0);
            return;
        }
        uintptr_t localPawn = 0;
        __try {
            localPawn = *reinterpret_cast<uintptr_t*>(OffsetConfig::globals.dwLocalPlayerPawn);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            InterlockedExchange(&s_enter_guard, 0);
            return;
        }
        if (localPawn < 0x10000) {
            InterlockedExchange(&s_enter_guard, 0);
            return;
        }

        InterlockedExchange(&g_materials_ready, 1);

        int ok = 0;
        const int total = (int)g_entries.size() - 1;

        for (size_t i = 1; i < g_entries.size(); i++) {
            uintptr_t mat = TryBuildMaterial(g_entries[i].path.c_str());
            g_materials[i] = mat;
            if (mat) ok++;
            else printf("[Skybox] Failed to compile %s\n", g_entries[i].name.c_str());
        }

        printf("[Skybox] Compiled %d/%d skyboxes\n", ok, total);
    }

    void Shutdown() {
        UninstallHooks();
        g_draw_hook_fn = 0;
        g_fsn_hook_fn = 0;
        g_materials.clear();
    }

    bool IsEnabled() { return g_enabled != 0; }

    void SetEnabled(bool enabled) {
        InterlockedExchange(&g_enabled, enabled ? 1 : 0);
    }

    int GetSelectedIndex() { return g_selected; }

    void SetSelectedIndex(int index) {
        if (index < 0) index = 0;
        if (index >= (int)g_entries.size()) index = (int)g_entries.size() - 1;
        InterlockedExchange(&g_selected, index);
    }

    const std::vector<Entry>& GetEntries() { return g_entries; }

    void OnSkyboxDraw(void* mesh_array, uint32_t count) {
        if (g_materials_ready == 0) return;
        if (!mesh_array || count == 0) return;

        uintptr_t base = (uintptr_t)mesh_array;
        uintptr_t slot = base + (uintptr_t)count * 0x70 - 0x58;

        uintptr_t descriptor = 0;
        __try { descriptor = *(uintptr_t*)slot; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return; }
        if (!descriptor) return;

        uintptr_t binding = 0;
        __try { binding = *(uintptr_t*)(descriptor + 0xD0); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return; }
        if (!binding) return;

        uintptr_t current = 0;
        __try { current = *(uintptr_t*)binding; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return; }

        if (current != g_last_written) {
            g_original_material = current;
        }

        const int sel = g_selected;
        const bool enabled = (g_enabled != 0);
        const uintptr_t custom = (sel > 0 && sel < (int)g_entries.size())
            ? g_materials[sel] : 0;

        if (enabled && custom != 0) {
            if (current != custom) {
                __try { *(uintptr_t*)binding = custom; }
                __except (EXCEPTION_EXECUTE_HANDLER) {}
                g_last_written = custom;
            }
        }
        else {
            if (g_last_written != 0 && g_original_material != 0 && current == g_last_written) {
                __try { *(uintptr_t*)binding = g_original_material; }
                __except (EXCEPTION_EXECUTE_HANDLER) {}
                g_last_written = 0;
            }
        }
    }
}