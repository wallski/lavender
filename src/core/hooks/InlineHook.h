#pragma once
#include <cstdint>

class InlineHook {
public:
    bool Install(void* target, void* detour);
    void Remove();

    template <typename T>
    T CallOriginal() const {
        return reinterpret_cast<T>(m_trampoline);
    }

    bool IsInstalled() const { return m_installed; }

private:
    void* m_target = nullptr;
    void* m_detour = nullptr;
    void* m_trampoline = nullptr;
    uint8_t m_originalBytes[16] = { 0 };
    size_t m_patchSize = 0;
    bool m_installed = false;
};