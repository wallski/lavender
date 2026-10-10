#include "pch.h"
#include "CreateMoveHook.h"
#include "../memory/PatternScan.h"
#include "../input/InputHelper.h"
#include "../features/combat/legit/triggerbot/UsercmdLayout.h"
#include "../../config/OffsetConfig.h"
#include <vector>
#include <atomic>

namespace CreateMoveHook {

    static std::vector<CallbackFn> g_callbacks;
    static uintptr_t g_getUsercmdBaseFn = 0;
    static uintptr_t g_getUsercmdFn = 0;

    static uintptr_t g_hookFn = 0;
    static uint8_t* g_trampoline = nullptr;
    static uint8_t* g_stub = nullptr;
    static uint8_t g_original[8] = { 0 };

    static constexpr uintptr_t kMessageImplOffset = 0x10;

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

    static bool SafeReadI32(uintptr_t addr, int32_t* out) {
        *out = 0;
        __try { *out = *reinterpret_cast<int32_t*>(addr); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        return true;
    }

    static bool SafeReadU64(uintptr_t addr, uint64_t* out) {
        *out = 0;
        __try { *out = *reinterpret_cast<uint64_t*>(addr); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        return true;
    }

    static bool SafeReadBytes(uintptr_t addr, uint8_t* out, size_t count) {
        __try {
            for (size_t i = 0; i < count; i++) out[i] = *reinterpret_cast<uint8_t*>(addr + i);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }

    static uintptr_t GetLocalController() {
        if (!OffsetConfig::globals.dwLocalPlayerController) return 0;
        uintptr_t p = 0;
        SafeReadU64(OffsetConfig::globals.dwLocalPlayerController, &p);
        return p;
    }

    UsercmdLayout* GetCurrentUsercmd() {
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

    static uintptr_t ResolveRelCall(uintptr_t instr) {
        int32_t rel = 0;
        if (!SafeReadI32(instr + 1, &rel)) return 0;
        return instr + 5 + (intptr_t)rel;
    }

    static uintptr_t ResolveCreateMove() {
        const char* sig = "FF FF FF FF 48 8D 05 ?? ?? ?? ?? 48 89 0D ?? ?? ?? ??";

        uintptr_t match = Memory::FindPattern("client.dll", sig);
        if (!match) return 0;

        int32_t rel = 0;
        if (!SafeReadI32(match + 7, &rel)) return 0;

        uintptr_t leaTarget = match + 11 + (intptr_t)rel;
        uintptr_t ptrAddr = leaTarget + 0x28;

        uint64_t candidate64 = 0;
        if (!SafeReadU64(ptrAddr, &candidate64)) return 0;

        uintptr_t candidate = (uintptr_t)candidate64;
        if (candidate < 0x10000) return 0;

        uint8_t b[8] = {};
        SafeReadBytes(candidate, b, 8);

        if (b[0] != 0x85 || b[1] != 0xD2 || b[2] != 0x0F || b[3] != 0x85) return 0;

        return candidate;
    }

    static void FlushToProtobuf(UsercmdLayout* cmd) {
        if (!cmd) return;
        uintptr_t pbRaw = reinterpret_cast<uintptr_t>(cmd->pb.base);
        if (pbRaw < 0x10000) return;

        proto::base_usercmd_pb* pbase =
            reinterpret_cast<proto::base_usercmd_pb*>(pbRaw + kMessageImplOffset);

        uintptr_t pbBtns = InputHelper::EnsureButtons(pbase);
        if (!pbBtns) return;

        __try { pbase->m_has_bits.set(0x2u); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return; }

        InButtonStatePb* btns = reinterpret_cast<InButtonStatePb*>(pbBtns);
        __try {
            btns->has_bits.bits |= 0x7u;
            btns->buttonstate1 = cmd->buttons.value;
            btns->buttonstate2 = cmd->buttons.value_changed;
            btns->buttonstate3 = cmd->buttons.value_scroll;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    extern "C" void CreateMoveStubBridge() {
        UsercmdLayout* cmd = GetCurrentUsercmd();
        if (!cmd) return;

        for (auto fn : g_callbacks) {
            if (fn) fn(cmd);
        }

        FlushToProtobuf(cmd);
        InputHelper::RecalculateCrc(cmd);
    }

    static uint8_t* BuildTrampoline(uintptr_t fn) {
        uint8_t* tramp = (uint8_t*)AllocNear((void*)fn, 64);
        if (!tramp) return nullptr;

        memcpy(tramp, (void*)fn, 8);

        int32_t orig_jnz_rel = 0;
        memcpy(&orig_jnz_rel, (void*)(fn + 4), 4);
        uintptr_t jnz_target = fn + 8 + (intptr_t)orig_jnz_rel;
        intptr_t new_jnz_rel = (intptr_t)jnz_target - (intptr_t)(tramp + 8);
        if (new_jnz_rel < INT32_MIN || new_jnz_rel > INT32_MAX) return nullptr;
        int32_t new_jnz_rel32 = (int32_t)new_jnz_rel;
        memcpy(tramp + 4, &new_jnz_rel32, 4);

        uint8_t* p = tramp + 8;
        *p++ = 0xFF; *p++ = 0x25; *p++ = 0x00; *p++ = 0x00; *p++ = 0x00; *p++ = 0x00;
        *(uint64_t*)p = (uint64_t)(fn + 8); p += 8;

        return tramp;
    }

    static uint8_t* BuildStub(uintptr_t fn) {
        uint8_t* stub = (uint8_t*)AllocNear((void*)fn, 128);
        if (!stub) return nullptr;
        uint8_t* p = stub;

        *p++ = 0x48; *p++ = 0x83; *p++ = 0xEC; *p++ = 0x28;

        *p++ = 0x48; *p++ = 0xB8;
        *(uint64_t*)p = (uint64_t)g_trampoline; p += 8;
        *p++ = 0xFF; *p++ = 0xD0;

        *p++ = 0x48; *p++ = 0xB8;
        *(uint64_t*)p = (uint64_t)&CreateMoveStubBridge; p += 8;
        *p++ = 0xFF; *p++ = 0xD0;

        *p++ = 0x48; *p++ = 0x83; *p++ = 0xC4; *p++ = 0x28;
        *p++ = 0xC3;

        return stub;
    }

    static bool InstallHook(uintptr_t fn) {
        __try { memcpy(g_original, (void*)fn, 8); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }

        if (g_original[0] != 0x85 || g_original[1] != 0xD2) return false;
        if (g_original[2] != 0x0F || g_original[3] != 0x85) return false;

        g_trampoline = BuildTrampoline(fn);
        if (!g_trampoline) return false;

        g_stub = BuildStub(fn);
        if (!g_stub) return false;

        intptr_t rel = (intptr_t)g_stub - (intptr_t)(fn + 5);
        if (rel < INT32_MIN || rel > INT32_MAX) return false;

        DWORD old = 0;
        if (!VirtualProtect((void*)fn, 8, PAGE_EXECUTE_READWRITE, &old)) return false;

        uint8_t* patch = (uint8_t*)fn;
        patch[0] = 0xE9;
        *(int32_t*)(patch + 1) = (int32_t)rel;
        patch[5] = 0x90;
        patch[6] = 0x90;
        patch[7] = 0x90;

        VirtualProtect((void*)fn, 8, old, &old);
        FlushInstructionCache(GetCurrentProcess(), (void*)fn, 8);
        return true;
    }

    bool Initialize() {
        uintptr_t wrapper = Memory::FindPattern("client.dll",
            "48 83 EC 28 E8 ?? ?? ?? ?? 8B 80 10 59 00 00");
        if (wrapper) g_getUsercmdBaseFn = ResolveRelCall(wrapper + 4);

        g_getUsercmdFn = Memory::FindPattern("client.dll",
            "40 53 48 83 EC 20 8B DA E8 ?? ?? ?? ?? 4C 8B C0");

        if (!g_getUsercmdBaseFn || !g_getUsercmdFn) return false;

        uintptr_t fn = ResolveCreateMove();
        if (!fn) return false;

        if (!InstallHook(fn)) return false;

        g_hookFn = fn;
        printf("[CreateMove] hook ready\n");
        fflush(stdout);
        return true;
    }

    void Shutdown() {
        if (g_hookFn) {
            DWORD old = 0;
            if (VirtualProtect((void*)g_hookFn, 8, PAGE_EXECUTE_READWRITE, &old)) {
                memcpy((void*)g_hookFn, g_original, 8);
                VirtualProtect((void*)g_hookFn, 8, old, &old);
                FlushInstructionCache(GetCurrentProcess(), (void*)g_hookFn, 8);
            }
            g_hookFn = 0;
        }
        if (g_trampoline) { VirtualFree(g_trampoline, 0, MEM_RELEASE); g_trampoline = nullptr; }
        if (g_stub) { VirtualFree(g_stub, 0, MEM_RELEASE); g_stub = nullptr; }
        g_callbacks.clear();
    }

    void Register(CallbackFn fn) {
        if (fn) g_callbacks.push_back(fn);
    }
}