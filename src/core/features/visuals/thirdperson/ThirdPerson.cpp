#include "pch.h"
#include "ThirdPerson.h"
#include "core/features/combat/CombatShared.h"
#include "core/systems/Tracing.h"
#include "core/memory/PatternScan.h"
#include "config/OffsetConfig.h"
#include "../../../math/math.hpp"

namespace ThirdPerson {

    using OverrideViewFn = void(__fastcall*)(uintptr_t, uintptr_t);

    static OverrideViewFn g_original = nullptr;
    static uint8_t g_originalBytes[32] = {};
    static uintptr_t g_targetFn = 0;
    static uint8_t* g_trampoline = nullptr;
    static uint8_t* g_stub = nullptr;
    static int g_patchSize = 0;

    static volatile LONG g_enabled = 0;
    static volatile LONG g_key = VK_MBUTTON;
    static volatile LONG g_active = 0;
    static float g_distance = 85.0f;
    static float g_hullSize = 12.0f;

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

    static bool IsExecutable(uintptr_t addr) {
        MEMORY_BASIC_INFORMATION mbi;
        if (!VirtualQuery((LPCVOID)addr, &mbi, sizeof(mbi))) return false;
        const DWORD exec = PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
        return (mbi.Protect & exec) != 0;
    }

    static int ModRmLength(uint8_t modrm, bool hasSib) {
        const int mod = (modrm >> 6) & 3;
        const int rm = modrm & 7;

        int len = 0;

        if (mod == 0) {
            if (rm == 4) {
                len += 1;
                if (hasSib) len += 1;
            }
            else if (rm == 5) {
                len += 4;
            }
        }
        else if (mod == 1) {
            if (rm == 4) len += 1;
            len += 1;
        }
        else if (mod == 2) {
            if (rm == 4) len += 1;
            len += 4;
        }

        return len;
    }

    static int InstructionLength(const uint8_t* p) {
        const uint8_t b0 = p[0];

        if (b0 == 0x50 || b0 == 0x51 || b0 == 0x52 || b0 == 0x53 ||
            b0 == 0x54 || b0 == 0x55 || b0 == 0x56 || b0 == 0x57) return 1;

        int cursor = 0;

        if (b0 == 0x66 || b0 == 0x67 || b0 == 0xF2 || b0 == 0xF3 ||
            b0 == 0x2E || b0 == 0x3E || b0 == 0x26 || b0 == 0x36 ||
            b0 == 0x64 || b0 == 0x65 || b0 == 0xF0) {
            cursor += 1;
        }

        bool hasRex = false;
        if (p[cursor] >= 0x40 && p[cursor] <= 0x4F) {
            hasRex = true;
            cursor += 1;
        }

        const uint8_t op = p[cursor];
        cursor += 1;

        if (op >= 0x50 && op <= 0x57) return cursor;

        if (op == 0x89 || op == 0x8B) {
            const uint8_t modrm = p[cursor];
            const int mod = (modrm >> 6) & 3;
            const int rm = modrm & 7;
            const bool hasSib = (rm == 4) && (mod != 3);
            const int modrmLen = 1 + ModRmLength(modrm, hasSib);
            return cursor + modrmLen;
        }

        if (op == 0x8D) {
            const uint8_t modrm = p[cursor];
            const int mod = (modrm >> 6) & 3;
            const int rm = modrm & 7;
            const bool hasSib = (rm == 4) && (mod != 3);
            const int modrmLen = 1 + ModRmLength(modrm, hasSib);
            return cursor + modrmLen;
        }

        if (op == 0x83) return cursor + 2;
        if (op == 0x81) return cursor + 5;

        if (op == 0x8A || op == 0x88 || op == 0x33 || op == 0x3B) {
            const uint8_t modrm = p[cursor];
            const int mod = (modrm >> 6) & 3;
            const int rm = modrm & 7;
            const bool hasSib = (rm == 4) && (mod != 3);
            const int modrmLen = 1 + ModRmLength(modrm, hasSib);
            return cursor + modrmLen;
        }

        if (op == 0x6A) return cursor + 1;
        if (op == 0x68) return cursor + 4;

        if (op == 0xE8 || op == 0xE9) return cursor + 4;

        if (op == 0xC6 || op == 0xC7) {
            const uint8_t modrm = p[cursor];
            const int mod = (modrm >> 6) & 3;
            const int rm = modrm & 7;
            const bool hasSib = (rm == 4) && (mod != 3);
            const int modrmLen = 1 + ModRmLength(modrm, hasSib);
            const int immLen = (op == 0xC6) ? 1 : 4;
            return cursor + modrmLen + immLen;
        }

        if (op == 0xE8 || op == 0xE9 || op == 0xEB) {
            return 0;
        }

        if (op == 0xB8 || (op >= 0xB8 && op <= 0xBF)) return cursor + 4;

        if (op == 0x84 || op == 0x85 || op == 0x31 || op == 0x39 ||
            op == 0x21 || op == 0x29 || op == 0x09 || op == 0x19 ||
            op == 0x01 || op == 0x11 || op == 0x03 || op == 0x13 ||
            op == 0x23 || op == 0x2B || op == 0x0B || op == 0x1B) {
            const uint8_t modrm = p[cursor];
            const int mod = (modrm >> 6) & 3;
            const int rm = modrm & 7;
            const bool hasSib = (rm == 4) && (mod != 3);
            const int modrmLen = 1 + ModRmLength(modrm, hasSib);
            return cursor + modrmLen;
        }

        if (op == 0xFF) {
            const uint8_t modrm = p[cursor];
            const int mod = (modrm >> 6) & 3;
            if (mod == 3) return cursor + 1;
            const int rm = modrm & 7;
            if (mod == 0 && rm == 5) return cursor + 5;
            return cursor + 1 + ModRmLength(modrm, rm == 4);
        }

        if (op == 0x8F || op == 0x87) {
            const uint8_t modrm = p[cursor];
            const int mod = (modrm >> 6) & 3;
            const int rm = modrm & 7;
            const bool hasSib = (rm == 4) && (mod != 3);
            const int modrmLen = 1 + ModRmLength(modrm, hasSib);
            return cursor + modrmLen;
        }

        if (op == 0xD1 || op == 0xD3 || op == 0xC1 || op == 0xE0 ||
            op == 0xE8 || op == 0xC0 || op == 0xE1 || op == 0xE2 ||
            op == 0xE3 || op == 0xF7) {
            const uint8_t modrm = p[cursor];
            const int mod = (modrm >> 6) & 3;
            const int rm = modrm & 7;
            const bool hasSib = (rm == 4) && (mod != 3);
            const int modrmLen = 1 + ModRmLength(modrm, hasSib);
            int extra = 0;
            if (op == 0xF7) {
                const int reg = (modrm >> 3) & 7;
                if (reg == 0 || reg == 1) extra = 4;
            }
            return cursor + modrmLen + extra;
        }

        if (op == 0xC3 || op == 0xCC || op == 0x90 || op == 0x98 || op == 0x99 ||
            op == 0xF8 || op == 0xF9 || op == 0xFC || op == 0xFD ||
            op == 0x50 || op == 0x58 || op == 0x59 || op == 0x5A || op == 0x5B ||
            op == 0x5C || op == 0x5D || op == 0x5E || op == 0x5F) return cursor;

        if (op == 0x48 && !hasRex) return 1;

        (void)hasRex;
        return 0;
    }

    static uintptr_t FindOverrideViewSlot() {
        const char* sig = "A8 00 00 00 48 8D 05 ? ? ? ? 4C 89 74 24 20";

        uintptr_t match = Memory::FindPattern("client.dll", sig);
        if (!match) return 0;

        int32_t rel = *reinterpret_cast<int32_t*>(match + 7);
        uintptr_t leaTarget = match + 11 + (intptr_t)rel;

        return leaTarget + 0x78;
    }

    static uintptr_t ResolveOverrideView() {
        uintptr_t slot = FindOverrideViewSlot();
        if (!slot) return 0;

        uint64_t deref = *reinterpret_cast<uint64_t*>(slot);
        if (deref < 0x10000) return 0;
        if (!IsExecutable((uintptr_t)deref)) return 0;

        return (uintptr_t)deref;
    }

    static void ApplyThirdPerson(uintptr_t viewSetup) {
        uintptr_t localPawn = CombatShared::GetLocalPawn();
        if (localPawn < 0x10000) return;

        Vec3 eye;
        if (!CombatShared::GetEyePosition(localPawn, eye)) return;

        uintptr_t va = OffsetConfig::globals.dwViewAngles;
        if (!va) return;

        math::vector3 angles{ 0.0f, 0.0f, 0.0f };
        angles.x = *reinterpret_cast<float*>(va + 0);
        angles.y = *reinterpret_cast<float*>(va + 4);

        math::vector3 forward{};
        math::helpers::angle_vectors_left(angles, &forward);

        math::vector3 eyeM{ eye.x, eye.y, eye.z };
        math::vector3 camera = eyeM - forward * g_distance;

        math::vector3 hull{ -g_hullSize, -g_hullSize, -g_hullSize };

        auto filter = Tracing::MakeFilter(localPawn, 0x1c3003, 4);
        auto result = Tracing::TraceHull(eyeM, camera, hull, hull, filter);

        if (result.fraction < 1.0f) {
            uintptr_t world = CombatShared::GetEntityByIndex(0);
            if (result.hitEntity == world) {
                camera = eyeM + (camera - eyeM) * result.fraction;
            }
        }

        float* dst = reinterpret_cast<float*>(viewSetup + 0x4a0);
        dst[0] = camera.x;
        dst[1] = camera.y;
        dst[2] = camera.z;
    }

    static void __fastcall HookedOverrideView(uintptr_t thisptr, uintptr_t viewSetup) {
        if (g_original) {
            g_original(thisptr, viewSetup);
        }

        if (g_enabled == 0) {
            InterlockedExchange(&g_active, 0);
            return;
        }

        static bool s_prevKeyDown = false;
        const bool keyDown = (GetAsyncKeyState(g_key) & 0x8000) != 0;
        if (keyDown && !s_prevKeyDown) {
            const LONG cur = g_active;
            InterlockedExchange(&g_active, cur ? 0 : 1);
        }
        s_prevKeyDown = keyDown;

        if (g_active == 0) return;

        ApplyThirdPerson(viewSetup);
    }

    static bool BuildTrampoline(uintptr_t fn) {
        const uint8_t* targetBytes = (const uint8_t*)fn;

        int boundary = 0;
        while (boundary < 5) {
            int len = InstructionLength(targetBytes + boundary);
            if (len <= 0) {
                printf("[ThirdPerson] disasm stop at offset %d, byte %02X %02X %02X\n",
                    boundary,
                    targetBytes[boundary],
                    targetBytes[boundary + 1],
                    targetBytes[boundary + 2]);
                fflush(stdout);
                return false;
            }
            boundary += len;
            if (boundary > 30) return false;
        }

        g_patchSize = boundary;

        g_trampoline = (uint8_t*)AllocNear((void*)fn, 64);
        if (!g_trampoline) return false;

        memcpy(g_trampoline, (void*)fn, g_patchSize);

        uint8_t* tp = g_trampoline + g_patchSize;
        *tp++ = 0xFF;
        *tp++ = 0x25;
        *tp++ = 0x00;
        *tp++ = 0x00;
        *tp++ = 0x00;
        *tp++ = 0x00;
        *(uint64_t*)tp = (uint64_t)(fn + g_patchSize);

        g_original = (OverrideViewFn)g_trampoline;
        return true;
    }

    static bool BuildStub() {
        g_stub = (uint8_t*)AllocNear((void*)g_targetFn, 64);
        if (!g_stub) return false;

        uint8_t* p = g_stub;
        *p++ = 0x51;
        *p++ = 0x52;
        *p++ = 0x48; *p++ = 0x83; *p++ = 0xEC; *p++ = 0x28;

        *p++ = 0x48; *p++ = 0xB8;
        *(uint64_t*)p = (uint64_t)&HookedOverrideView; p += 8;

        *p++ = 0xFF; *p++ = 0xD0;

        *p++ = 0x48; *p++ = 0x83; *p++ = 0xC4; *p++ = 0x28;
        *p++ = 0x5A;
        *p++ = 0x59;
        *p++ = 0xC3;

        return true;
    }

    bool Initialize() {
        g_targetFn = ResolveOverrideView();
        if (!g_targetFn) {
            printf("[ThirdPerson] override_view pattern not found\n");
            fflush(stdout);
            return false;
        }

        const uint8_t* probe = (const uint8_t*)g_targetFn;
        printf("[ThirdPerson] target bytes: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
            probe[0], probe[1], probe[2], probe[3], probe[4], probe[5], probe[6], probe[7],
            probe[8], probe[9], probe[10], probe[11], probe[12], probe[13], probe[14], probe[15]);
        fflush(stdout);

        if (!BuildTrampoline(g_targetFn)) {
            printf("[ThirdPerson] trampoline build failed\n");
            fflush(stdout);
            return false;
        }

        if (!BuildStub()) {
            printf("[ThirdPerson] stub alloc failed\n");
            fflush(stdout);
            return false;
        }

        memcpy(g_originalBytes, (void*)g_targetFn, g_patchSize);

        intptr_t stubRel = (intptr_t)g_stub - (intptr_t)(g_targetFn + 5);
        if (stubRel < INT32_MIN || stubRel > INT32_MAX) {
            printf("[ThirdPerson] stub out of rel32 range\n");
            fflush(stdout);
            return false;
        }

        DWORD old = 0;
        if (!VirtualProtect((void*)g_targetFn, g_patchSize, PAGE_EXECUTE_READWRITE, &old)) return false;

        uint8_t* patch = (uint8_t*)g_targetFn;
        patch[0] = 0xE9;
        int32_t rel32 = (int32_t)stubRel;
        memcpy(patch + 1, &rel32, 4);

        for (int i = 5; i < g_patchSize; i++) patch[i] = 0x90;

        VirtualProtect((void*)g_targetFn, g_patchSize, old, &old);
        FlushInstructionCache(GetCurrentProcess(), (void*)g_targetFn, g_patchSize);

        printf("[ThirdPerson] ready (target=0x%llX patchSize=%d trampoline=0x%llX stub=0x%llX hook=0x%llX)\n",
            (unsigned long long)g_targetFn, g_patchSize,
            (unsigned long long)g_trampoline,
            (unsigned long long)g_stub,
            (unsigned long long) & HookedOverrideView);
        fflush(stdout);
        return true;
    }

    void Shutdown() {
        if (g_targetFn && g_patchSize > 0) {
            DWORD old = 0;
            if (VirtualProtect((void*)g_targetFn, g_patchSize, PAGE_EXECUTE_READWRITE, &old)) {
                memcpy((void*)g_targetFn, g_originalBytes, g_patchSize);
                VirtualProtect((void*)g_targetFn, g_patchSize, old, &old);
                FlushInstructionCache(GetCurrentProcess(), (void*)g_targetFn, g_patchSize);
            }
        }
        if (g_stub) { VirtualFree(g_stub, 0, MEM_RELEASE); g_stub = nullptr; }
        if (g_trampoline) { VirtualFree(g_trampoline, 0, MEM_RELEASE); g_trampoline = nullptr; }
        g_original = nullptr;
        g_targetFn = 0;
        g_patchSize = 0;
        InterlockedExchange(&g_active, 0);
    }

    bool IsEnabled() { return g_enabled != 0; }
    void SetEnabled(bool enabled) {
        InterlockedExchange(&g_enabled, enabled ? 1 : 0);
        if (!enabled) InterlockedExchange(&g_active, 0);
    }

    int GetKey() { return g_key; }
    void SetKey(int key) { InterlockedExchange(&g_key, key); }

    float GetDistance() { return g_distance; }
    void SetDistance(float distance) {
        if (distance < 20.0f) distance = 20.0f;
        if (distance > 400.0f) distance = 400.0f;
        g_distance = distance;
    }

    float GetHullSize() { return g_hullSize; }
    void SetHullSize(float size) {
        if (size < 1.0f) size = 1.0f;
        if (size > 40.0f) size = 40.0f;
        g_hullSize = size;
    }
}