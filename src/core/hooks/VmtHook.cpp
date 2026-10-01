#include "pch.h"
#include "VmtHook.h"

bool VmtHook::Initialize(void* instance) {
    if (!instance) return false;

    m_originalVtable = *reinterpret_cast<void***>(instance);
    if (!m_originalVtable) return false;

    int count = 0;
    while (m_originalVtable[count] != nullptr && count < 512) count++;
    m_methodCount = count;
    if (m_methodCount == 0) return false;

    m_newVtable = new void* [m_methodCount];
    for (int i = 0; i < m_methodCount; i++) {
        m_newVtable[i] = m_originalVtable[i];
    }

    *reinterpret_cast<void***>(instance) = m_newVtable;
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