#pragma once
#include <cstdint>
#include <unordered_map>

class VmtHook {
public:
    bool Initialize(void* instance);
    void* Hook(int index, void* newFn);
    void Restore();
    void* GetOriginal(int index);

private:
    void** m_originalVtable = nullptr;
    void** m_newVtable = nullptr;
    int    m_methodCount = 0;
    std::unordered_map<int, void*> m_originals;
};