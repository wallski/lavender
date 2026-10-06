#include "pch.h"
#include "Removals.h"
#include "../../../../config/OffsetConfig.h"
#include "../../../memory/PatternScan.h"

static volatile LONG g_flashEnabled = 0;
static volatile LONG g_smokeEnabled = 0;

namespace Removals {

    static uintptr_t g_renderSmokeFn = 0;
    static uint8_t g_renderSmokeOriginal = 0;
    static bool g_smokePatched = false;

    static bool IsValidPtr(uintptr_t p) {
        return p >= 0x10000 && p <= 0x00007FFFFFFFFFFF;
    }

    static bool SafeReadByte(uintptr_t addr, uint8_t* out) {
        __try { *out = *reinterpret_cast<uint8_t*>(addr); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        return true;
    }

    static bool SafeReadI32(uintptr_t addr, int32_t* out) {
        __try { *out = *reinterpret_cast<int32_t*>(addr); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        return true;
    }

    static bool SafeWriteByte(uintptr_t addr, uint8_t value) {
        __try { *reinterpret_cast<uint8_t*>(addr) = value; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        return true;
    }

    static bool SafeReadPtr(uintptr_t addr, uintptr_t* out) {
        __try { *out = *reinterpret_cast<uintptr_t*>(addr); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        return true;
    }

    static bool SafeWriteFloat(uintptr_t addr, float value) {
        __try { *reinterpret_cast<float*>(addr) = value; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        return true;
    }

    static bool ResolveRenderSmoke() {
        const char* sig = "5C 24 28 48 89 44 24 20 E8 ? ? ? ? 48 8B 5C 24 60";

        uintptr_t match = Memory::FindPattern("client.dll", sig);
        if (!match) {
            printf("[removals] render_smoke pattern not found\n");
            fflush(stdout);
            return false;
        }

        uintptr_t e8 = match + 8;
        int32_t rel = 0;
        if (!SafeReadI32(e8 + 1, &rel)) return false;

        uintptr_t fn = e8 + 5 + (intptr_t)rel;
        if (!IsValidPtr(fn)) {
            printf("[removals] render_smoke target invalid: 0x%llX\n", (unsigned long long)fn);
            fflush(stdout);
            return false;
        }

        uint8_t prologue[8] = {};
        for (int i = 0; i < 8; i++) {
            if (!SafeReadByte(fn + i, &prologue[i])) return false;
        }

        printf("[removals] render_smoke=0x%llX prologue=%02X %02X %02X %02X %02X %02X %02X %02X\n",
            (unsigned long long)fn,
            prologue[0], prologue[1], prologue[2], prologue[3],
            prologue[4], prologue[5], prologue[6], prologue[7]);
        fflush(stdout);

        g_renderSmokeFn = fn;
        g_renderSmokeOriginal = prologue[0];
        return true;
    }

    static void ApplySmokePatch(bool enabled) {
        if (!g_renderSmokeFn) return;

        if (enabled && g_smokePatched) return;
        if (!enabled && !g_smokePatched) return;

        DWORD oldProtect = 0;
        if (!VirtualProtect((void*)g_renderSmokeFn, 1, PAGE_EXECUTE_READWRITE, &oldProtect)) return;

        uint8_t value = enabled ? 0xC3 : g_renderSmokeOriginal;
        SafeWriteByte(g_renderSmokeFn, value);

        VirtualProtect((void*)g_renderSmokeFn, 1, oldProtect, &oldProtect);
        FlushInstructionCache(GetCurrentProcess(), (void*)g_renderSmokeFn, 1);
        g_smokePatched = enabled;
    }

    bool Initialize() {
        ResolveRenderSmoke();
        return true;
    }

    void Shutdown() {
        if (g_renderSmokeFn && g_smokePatched) {
            ApplySmokePatch(false);
        }
        g_renderSmokeFn = 0;
        g_renderSmokeOriginal = 0;
        g_smokePatched = false;
    }

    bool IsFlashEnabled() { return g_flashEnabled != 0; }
    void SetFlashEnabled(bool enabled) { InterlockedExchange(&g_flashEnabled, enabled ? 1 : 0); }

    bool IsSmokeEnabled() { return g_smokeEnabled != 0; }
    void SetSmokeEnabled(bool enabled) {
        InterlockedExchange(&g_smokeEnabled, enabled ? 1 : 0);
        ApplySmokePatch(enabled);
    }

    void OnLevelChange(bool inLevel) {
        if (g_smokeEnabled == 0) {
            if (g_smokePatched) ApplySmokePatch(false);
            return;
        }
        ApplySmokePatch(inLevel);
    }

    static void UpdateFlash() {
        if (g_flashEnabled == 0) return;

        const uintptr_t global = OffsetConfig::globals.dwLocalPlayerPawn;
        if (!IsValidPtr(global)) return;

        uintptr_t localPawn = 0;
        if (!SafeReadPtr(global, &localPawn)) return;
        if (!IsValidPtr(localPawn)) return;

        SafeWriteFloat(localPawn + OffsetConfig::fields.m_flFlashBangTime, 0.0f);
        SafeWriteFloat(localPawn + OffsetConfig::fields.m_flFlashScreenshotAlpha, 0.0f);
        SafeWriteFloat(localPawn + OffsetConfig::fields.m_flFlashOverlayAlpha, 0.0f);
        SafeWriteFloat(localPawn + OffsetConfig::fields.m_flFlashMaxAlpha, 0.0f);
        SafeWriteFloat(localPawn + OffsetConfig::fields.m_flFlashDuration, 0.0f);
    }

    void Update() {
        UpdateFlash();
    }
}