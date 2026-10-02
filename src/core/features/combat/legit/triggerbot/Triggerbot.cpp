#include "pch.h"
#include "Triggerbot.h"
#include "../../CombatShared.h"
#include "../../../../../config/OffsetConfig.h"
#include "../../../../../core/memory/PatternScan.h"
#include "../../../../../render/Renderer.h"
#include <cmath>
#include <atomic>

namespace Triggerbot {

    static volatile LONG g_enabled = 0;
    static volatile LONG g_key = VK_XBUTTON2;
    static volatile LONG g_headOnly = 1;
    static volatile LONG g_delay_ms = 50;

    static uintptr_t g_getUsercmdBaseFn = 0;
    static uintptr_t g_getUsercmdFn = 0;

    static std::atomic<bool> g_shouldFire{ false };
    static std::atomic<bool> g_attackCurrentlyWritten{ false };
    static ULONGLONG g_releaseAt = 0;
    static ULONGLONG g_nextFireAllowed = 0;

    static volatile LONG g_writeCount = 0;
    static volatile LONG g_pbSeenCount = 0;
    static volatile LONG g_lastHasTarget = -1;

    static uintptr_t g_createMoveFn = 0;
    static uint8_t* g_createMoveTrampoline = nullptr;
    static uint8_t* g_createMoveStub = nullptr;
    static uint8_t g_createMoveOriginal[8] = { 0 };
    static int32_t g_createMoveJneRel = 0;
    static bool g_createMoveHookInstalled = false;

    static constexpr uintptr_t kMessageImplOffset = 0x10;

    struct HasBits { uint32_t bits; };

    struct RepeatedPtrField {
        void* arena;
        int current_size;
        int total_size;
        struct Rep { int allocated_size; void* elements[1]; };
        Rep* rep;
    };

    struct InButtonStatePb {
        HasBits has_bits;
        uint32_t cached_size;
        uint64_t buttonstate1;
        uint64_t buttonstate2;
        uint64_t buttonstate3;
    };

    struct BaseUsercmdPb {
        HasBits has_bits;
        uint32_t cached_size;
        RepeatedPtrField subtick_moves;
        void* move_crc;
        InButtonStatePb* buttons_pb;
    };

    struct CsgoUsercmdPb {
        HasBits has_bits;
        uint32_t cached_size;
        RepeatedPtrField input_history;
        BaseUsercmdPb* base;
        bool left_hand_desired;
        bool is_predicting_body_shot_fx;
        bool is_predicting_head_shot_fx;
        bool is_predicting_kill_ragdolls;
        int32_t attack1_start_history_index;
        int32_t attack2_start_history_index;
    };

    struct InButtonStateCache {
        void* vtable;
        uintptr_t value;
        uintptr_t value_changed;
        uintptr_t value_scroll;
    };

    struct UsercmdLayout {
        void* vtable;
        intptr_t command_number;
        void* proto_vtable;
        void* proto_arena;
        CsgoUsercmdPb pb;
        InButtonStateCache buttons;
    };

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

    static uintptr_t GetLocalController() {
        if (!OffsetConfig::globals.dwLocalPlayerController) return 0;
        __try { return *(uintptr_t*)OffsetConfig::globals.dwLocalPlayerController; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
    }

    static UsercmdLayout* GetCurrentUsercmd() {
        if (!g_getUsercmdBaseFn || !g_getUsercmdFn) return nullptr;
        uintptr_t controller = GetLocalController();
        if (!controller) return nullptr;

        using GetBaseFn = uintptr_t(*)(uintptr_t);
        using GetCmdFn = UsercmdLayout * (*)(uintptr_t, int);

        uintptr_t base = 0;
        __try { base = ((GetBaseFn)g_getUsercmdBaseFn)(controller); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
        if (!base) return nullptr;

        int sequence = 0;
        __try { sequence = *(int*)(base + 0x5910); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
        if (!sequence) return nullptr;

        UsercmdLayout* cmd = nullptr;
        __try { cmd = ((GetCmdFn)g_getUsercmdFn)(controller, sequence); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
        return cmd;
    }

    static void WritePbButtons(InButtonStatePb* pbBtns, uint64_t value, uint64_t changed, uint64_t scroll) {
        __try {
            pbBtns->has_bits.bits |= 0x1u;
            pbBtns->buttonstate1 = value;
            pbBtns->has_bits.bits |= 0x2u;
            pbBtns->buttonstate2 = changed;
            pbBtns->has_bits.bits |= 0x4u;
            pbBtns->buttonstate3 = scroll;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    static void WriteAttack(bool pressed) {
        UsercmdLayout* cmd = GetCurrentUsercmd();
        if (!cmd) return;
        if (!cmd->pb.base) return;

        BaseUsercmdPb* base = (BaseUsercmdPb*)((uintptr_t)cmd->pb.base + kMessageImplOffset);

        InButtonStateCache* btnsCache = &cmd->buttons;
        if (pressed) {
            btnsCache->value |= 1ull;
            btnsCache->value_changed |= 1ull;
        }
        else {
            btnsCache->value &= ~1ull;
            btnsCache->value_changed |= 1ull;
        }

        base->has_bits.bits |= 0x2u;

        InButtonStatePb* pbBtns = base->buttons_pb;
        if (pbBtns) {
            InterlockedIncrement(&g_pbSeenCount);
            InButtonStatePb* actualPb = (InButtonStatePb*)((uintptr_t)pbBtns + kMessageImplOffset);
            WritePbButtons(actualPb, btnsCache->value, btnsCache->value_changed, btnsCache->value_scroll);
        }

        LONG n = InterlockedIncrement(&g_writeCount);
        if (n == 1 || (n % 120) == 0) {
            printf("[Trig] Write(%d) cmd=0x%llX v=0x%llX c=0x%llX s=0x%llX pb=0x%llX (pbSeen=%ld)\n",
                pressed ? 1 : 0,
                (unsigned long long)cmd,
                (unsigned long long)btnsCache->value,
                (unsigned long long)btnsCache->value_changed,
                (unsigned long long)btnsCache->value_scroll,
                (unsigned long long)pbBtns,
                (long)g_pbSeenCount);
            fflush(stdout);
        }
    }

    static void DoWrite() {
        if (g_enabled == 0) {
            if (g_attackCurrentlyWritten.load()) {
                g_attackCurrentlyWritten.store(false);
                WriteAttack(false);
            }
            return;
        }

        ULONGLONG now = GetTickCount64();

        if (g_releaseAt && now >= g_releaseAt) {
            g_releaseAt = 0;
            g_shouldFire.store(false);
        }

        if (g_shouldFire.load()) {
            WriteAttack(true);
            g_attackCurrentlyWritten.store(true);
        }
        else if (g_attackCurrentlyWritten.load()) {
            g_attackCurrentlyWritten.store(false);
            WriteAttack(false);
        }
    }

    extern "C" void CreateMoveStubBridge(uintptr_t thisptr, int slot, bool active) {
        (void)thisptr; (void)slot; (void)active;
        DoWrite();
    }

    static uint8_t* BuildCreateMoveTrampoline(uintptr_t fn) {
        uint8_t* tramp = (uint8_t*)AllocNear((void*)fn, 128);
        if (!tramp) return nullptr;
        uint8_t* p = tramp;

        *p++ = 0x85; *p++ = 0xD2;

        *p++ = 0x0F; *p++ = 0x85;
        uintptr_t jne_target = fn + 8 + (intptr_t)g_createMoveJneRel;
        intptr_t tramp_jne_rel = (intptr_t)jne_target - (intptr_t)(p + 4);
        *(int32_t*)p = (int32_t)tramp_jne_rel; p += 4;

        *p++ = 0xFF; *p++ = 0x25; *p++ = 0x00; *p++ = 0x00; *p++ = 0x00; *p++ = 0x00;
        *(uint64_t*)p = fn + 8; p += 8;

        return tramp;
    }

    static uint8_t* BuildCreateMoveStub(uintptr_t fn) {
        uint8_t* stub = (uint8_t*)AllocNear((void*)fn, 256);
        if (!stub) return nullptr;
        uint8_t* p = stub;

        *p++ = 0x50;
        *p++ = 0x51;
        *p++ = 0x52;
        *p++ = 0x41; *p++ = 0x50;
        *p++ = 0x41; *p++ = 0x51;
        *p++ = 0x41; *p++ = 0x52;
        *p++ = 0x41; *p++ = 0x53;
        *p++ = 0x48; *p++ = 0x83; *p++ = 0xEC; *p++ = 0x20;

        *p++ = 0x48; *p++ = 0xB8;
        *(uint64_t*)p = (uint64_t)g_createMoveTrampoline; p += 8;
        *p++ = 0xFF; *p++ = 0xD0;

        *p++ = 0x48; *p++ = 0xB8;
        *(uint64_t*)p = (uint64_t)&CreateMoveStubBridge; p += 8;
        *p++ = 0xFF; *p++ = 0xD0;

        *p++ = 0x48; *p++ = 0x83; *p++ = 0xC4; *p++ = 0x20;
        *p++ = 0x41; *p++ = 0x5B;
        *p++ = 0x41; *p++ = 0x5A;
        *p++ = 0x41; *p++ = 0x59;
        *p++ = 0x41; *p++ = 0x58;
        *p++ = 0x5A;
        *p++ = 0x59;
        *p++ = 0x58;
        *p++ = 0xC3;

        return stub;
    }

    static bool InstallCreateMoveHook(uintptr_t fn) {
        __try {
            memcpy(g_createMoveOriginal, (void*)fn, 8);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }

        if (g_createMoveOriginal[0] != 0x85 || g_createMoveOriginal[1] != 0xD2) return false;
        if (g_createMoveOriginal[2] != 0x0F || g_createMoveOriginal[3] != 0x85) return false;

        g_createMoveJneRel = *(int32_t*)(fn + 4);

        g_createMoveTrampoline = BuildCreateMoveTrampoline(fn);
        if (!g_createMoveTrampoline) return false;

        g_createMoveStub = BuildCreateMoveStub(fn);
        if (!g_createMoveStub) return false;

        intptr_t rel = (intptr_t)g_createMoveStub - (intptr_t)(fn + 5);
        if (rel < INT32_MIN || rel > INT32_MAX) return false;

        DWORD old = 0;
        if (!VirtualProtect((void*)fn, 5, PAGE_EXECUTE_READWRITE, &old)) return false;

        uint8_t* patch = (uint8_t*)fn;
        patch[0] = 0xE9;
        *(int32_t*)(patch + 1) = (int32_t)rel;
        patch[5] = 0x90;
        patch[6] = 0x90;
        patch[7] = 0x90;

        VirtualProtect((void*)fn, 5, old, &old);
        FlushInstructionCache(GetCurrentProcess(), (void*)fn, 5);
        return true;
    }

    static uintptr_t ResolveRelCall(uintptr_t instr) {
        int32_t rel = 0;
        __try { rel = *(int32_t*)(instr + 1); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
        return instr + 5 + (intptr_t)rel;
    }

    static uintptr_t DerefCreateMoveFromMatch(uintptr_t match) {
        __try {
            int32_t rel = *(int32_t*)(match + 7);
            uintptr_t leaTarget = match + 11 + (intptr_t)rel;
            uintptr_t ptrAddr = leaTarget + 0x28;
            return *(uintptr_t*)ptrAddr;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return 0;
        }
    }

    static uintptr_t ResolveCreateMove() {
        uintptr_t match = Memory::FindPattern("client.dll",
            "FF FF FF FF 48 8D 05 ?? ?? ?? ?? 48 89 0D ?? ?? ?? ??");
        if (!match) return 0;
        return DerefCreateMoveFromMatch(match);
    }

    bool Initialize() {
        uintptr_t wrapper = Memory::FindPattern("client.dll",
            "48 83 EC 28 E8 ?? ?? ?? ?? 8B 80 10 59 00 00");

        if (wrapper) {
            g_getUsercmdBaseFn = ResolveRelCall(wrapper + 4);
        }

        g_getUsercmdFn = Memory::FindPattern("client.dll",
            "40 53 48 83 EC 20 8B DA E8 ?? ?? ?? ?? 4C 8B C0");

        if (!g_getUsercmdBaseFn || !g_getUsercmdFn) {
            printf("[Triggerbot] sigs missing\n");
            fflush(stdout);
            return false;
        }

        uintptr_t createMoveFn = ResolveCreateMove();
        if (createMoveFn) {
            if (InstallCreateMoveHook(createMoveFn)) {
                g_createMoveFn = createMoveFn;
                g_createMoveHookInstalled = true;
                printf("[Triggerbot] create_move hooked at 0x%llX\n",
                    (unsigned long long)createMoveFn);
            }
            else {
                printf("[Triggerbot] create_move hook install failed\n");
            }
        }
        else {
            printf("[Triggerbot] create_move sig failed\n");
        }

        printf("[Triggerbot] ready\n");
        fflush(stdout);
        return true;
    }

    void Shutdown() {
        if (g_createMoveHookInstalled && g_createMoveFn) {
            DWORD old = 0;
            if (VirtualProtect((void*)g_createMoveFn, 5, PAGE_EXECUTE_READWRITE, &old)) {
                memcpy((void*)g_createMoveFn, g_createMoveOriginal, 5);
                VirtualProtect((void*)g_createMoveFn, 5, old, &old);
                FlushInstructionCache(GetCurrentProcess(), (void*)g_createMoveFn, 5);
            }
            if (g_createMoveTrampoline) { VirtualFree(g_createMoveTrampoline, 0, MEM_RELEASE); g_createMoveTrampoline = nullptr; }
            if (g_createMoveStub) { VirtualFree(g_createMoveStub, 0, MEM_RELEASE); g_createMoveStub = nullptr; }
            g_createMoveHookInstalled = false;
        }
    }

    void OnGameThreadWrite() {
        if (g_createMoveHookInstalled) return;
        DoWrite();
    }

    bool IsEnabled() { return g_enabled != 0; }
    void SetEnabled(bool enabled) {
        InterlockedExchange(&g_enabled, enabled ? 1 : 0);
        if (!enabled) {
            g_shouldFire.store(false);
            g_releaseAt = 0;
        }
    }

    int GetKey() { return g_key; }
    void SetKey(int key) { InterlockedExchange(&g_key, key); }

    bool GetHeadOnly() { return g_headOnly != 0; }
    void SetHeadOnly(bool headOnly) { InterlockedExchange(&g_headOnly, headOnly ? 1 : 0); }

    int GetDelayMs() { return g_delay_ms; }
    void SetDelayMs(int ms) {
        if (ms < 0) ms = 0;
        if (ms > 500) ms = 500;
        InterlockedExchange(&g_delay_ms, ms);
    }

    static bool IsGameFocused() {
        HWND fg = GetForegroundWindow();
        if (!fg) return false;
        DWORD pid = 0;
        GetWindowThreadProcessId(fg, &pid);
        return pid == GetCurrentProcessId();
    }

    void Tick() {
        if (g_enabled == 0) {
            g_shouldFire.store(false);
            g_releaseAt = 0;
            return;
        }
        if (Renderer::IsMenuOpen()) return;
        if (!IsGameFocused()) return;

        ULONGLONG now = GetTickCount64();

        if (g_releaseAt && now >= g_releaseAt) {
            g_releaseAt = 0;
            g_shouldFire.store(false);
        }

        if ((GetAsyncKeyState(g_key) & 0x8000) == 0) {
            if (g_shouldFire.load()) {
                g_shouldFire.store(false);
                g_releaseAt = 0;
            }
            InterlockedExchange(&g_lastHasTarget, -1);
            return;
        }

        if (g_shouldFire.load()) return;
        if (now < g_nextFireAllowed) return;

        uintptr_t localPawn = CombatShared::GetLocalPawn();
        if (!localPawn) return;

        RECT rect;
        HWND hwnd = GetForegroundWindow();
        if (!hwnd || !GetClientRect(hwnd, &rect)) return;

        float screenW = (float)(rect.right - rect.left);
        float screenH = (float)(rect.bottom - rect.top);
        float cx = screenW * 0.5f;
        float cy = screenH * 0.5f;

        const float radiusSq = 15.0f * 15.0f;
        const bool headOnly = (g_headOnly != 0);

        auto enemies = CombatShared::GetEnemies();
        bool hasTarget = false;

        for (const auto& e : enemies) {
            if (!e.visible) continue;

            Vec3 headScreen;
            if (CombatShared::WorldToScreen(e.headPos, headScreen)) {
                float dx = headScreen.x - cx;
                float dy = headScreen.y - cy;
                if (dx * dx + dy * dy < radiusSq) {
                    hasTarget = true;
                    break;
                }
            }

            if (headOnly) continue;

            Vec3 chestScreen;
            if (CombatShared::WorldToScreen(e.chestPos, chestScreen)) {
                float dx = chestScreen.x - cx;
                float dy = chestScreen.y - cy;
                if (dx * dx + dy * dy < radiusSq) {
                    hasTarget = true;
                    break;
                }
            }
        }

        LONG prev = InterlockedExchange(&g_lastHasTarget, hasTarget ? 1 : 0);
        if (prev != (hasTarget ? 1 : 0)) {
            printf("[Trig] target state -> %d (enemies=%d)\n", hasTarget ? 1 : 0, (int)enemies.size());
            fflush(stdout);
        }

        if (!hasTarget) return;

        int delay = g_delay_ms;
        ULONGLONG holdMs = (ULONGLONG)delay + 40ull;

        g_shouldFire.store(true);
        g_releaseAt = now + holdMs;
        g_nextFireAllowed = now + holdMs + 60ull;
    }
}