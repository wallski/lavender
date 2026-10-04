#include "pch.h"
#include "Removals.h"
#include "../../../../config/OffsetConfig.h"

static volatile LONG g_flashEnabled = 0;

namespace Removals {

    static bool IsValidPtr(uintptr_t p) {
        return p >= 0x10000 && p <= 0x00007FFFFFFFFFFF;
    }

    bool IsFlashEnabled() { return g_flashEnabled != 0; }
    void SetFlashEnabled(bool enabled) { InterlockedExchange(&g_flashEnabled, enabled ? 1 : 0); }

    static void UpdateFlash() {
        if (g_flashEnabled == 0) return;

        const uintptr_t global = OffsetConfig::globals.dwLocalPlayerPawn;
        if (!IsValidPtr(global)) return;

        uintptr_t localPawn = 0;
        __try { localPawn = *reinterpret_cast<uintptr_t*>(global); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return; }
        if (!IsValidPtr(localPawn)) return;

        __try {
            *reinterpret_cast<float*>(localPawn + OffsetConfig::fields.m_flFlashBangTime) = 0.0f;
            *reinterpret_cast<float*>(localPawn + OffsetConfig::fields.m_flFlashScreenshotAlpha) = 0.0f;
            *reinterpret_cast<float*>(localPawn + OffsetConfig::fields.m_flFlashOverlayAlpha) = 0.0f;
            *reinterpret_cast<float*>(localPawn + OffsetConfig::fields.m_flFlashMaxAlpha) = 0.0f;
            *reinterpret_cast<float*>(localPawn + OffsetConfig::fields.m_flFlashDuration) = 0.0f;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    void Update() {
        UpdateFlash();
    }
}