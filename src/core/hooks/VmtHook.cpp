#include "pch.h"
#include "VmtHook.h"

bool VmtHook::Initialize(void* instance) {
    if (!instance) return false;

    void** vtable = nullptr;
    __try {
        vtable = *reinterpret_cast<void***>(instance);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    if (!vtable) return false;

    uintptr_t firstFn = 0;
    __try {
        firstFn = reinterpret_cast<uintptr_t>(vtable[0]);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    if (firstFn < 0x10000 || firstFn > 0x00007FFFFFFFFFFF) return false;

    int count = 0;
    __try {
        while (count < 64 && vtable[count] != nullptr) count++;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    if (count < 4) return false;

    m_originalVtable = vtable;
    m_methodCount = count;

    m_newVtable = new void* [m_methodCount];
    for (int i = 0; i < m_methodCount; i++) {
        m_newVtable[i] = m_originalVtable[i];
    }

    __try {
        *reinterpret_cast<void***>(instance) = m_newVtable;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        delete[] m_newVtable;
        m_newVtable = nullptr;
        return false;
    }
    return true;
}

void* VmtHook::Hook(int index, void* newFn) {
    if (!m_newVtable || index < 0 || index >= m_methodCount) return nullptr;

    void* original = m_newVtable[index];
    m_newVtable[index] = newFn;
    m_originals[index] = original;
    return original;
}

void* VmtHook::GetOriginal(int index) {
    auto it = m_originals.find(index);
    if (it == m_originals.end()) return nullptr;
    return it->second;
}

void VmtHook::Restore() {
    if (!m_newVtable) return;
    for (auto& [idx, fn] : m_originals) {
        if (idx < m_methodCount) m_newVtable[idx] = fn;
    }
    m_originals.clear();
}