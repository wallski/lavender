#include "pch.h"
#include "InlineHook.h"

bool InlineHook::Install(void* target, void* detour) {
    if (!target || !detour) return false;
    if (m_installed) return false;

    m_target = target;
    m_detour = detour;

    m_trampoline = VirtualAlloc(nullptr, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!m_trampoline) return false;

    DWORD oldProtect = 0;
    if (!VirtualProtect(target, 16, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        VirtualFree(m_trampoline, 0, MEM_RELEASE);
        m_trampoline = nullptr;
        return false;
    }

    memcpy(m_originalBytes, target, 16);

    uint8_t* tramp = (uint8_t*)m_trampoline;
    memcpy(tramp, target, 16);

    uint8_t* back = tramp + 16;
    back[0] = 0xFF;
    back[1] = 0x25;
    back[2] = 0x00;
    back[3] = 0x00;
    back[4] = 0x00;
    back[5] = 0x00;
    *(uint64_t*)(back + 6) = (uint64_t)((uint8_t*)target + 16);

    uint8_t* patch = (uint8_t*)target;
    patch[0] = 0xFF;
    patch[1] = 0x25;
    patch[2] = 0x00;
    patch[3] = 0x00;
    patch[4] = 0x00;
    patch[5] = 0x00;
    *(uint64_t*)(patch + 6) = (uint64_t)detour;

    for (size_t i = 14; i < 16; i++) {
        patch[i] = 0x90;
    }

    VirtualProtect(target, 16, oldProtect, &oldProtect);

    FlushInstructionCache(GetCurrentProcess(), target, 16);
    FlushInstructionCache(GetCurrentProcess(), m_trampoline, 64);

    m_patchSize = 16;
    m_installed = true;
    return true;
}

void InlineHook::Remove() {
    if (!m_installed || !m_target) return;

    DWORD oldProtect = 0;
    if (VirtualProtect(m_target, 16, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        memcpy(m_target, m_originalBytes, 16);
        VirtualProtect(m_target, 16, oldProtect, &oldProtect);
        FlushInstructionCache(GetCurrentProcess(), m_target, 16);
    }

    if (m_trampoline) {
        VirtualFree(m_trampoline, 0, MEM_RELEASE);
        m_trampoline = nullptr;
    }

    m_installed = false;
}