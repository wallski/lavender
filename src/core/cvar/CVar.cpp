#include "pch.h"
#include "CVar.h"
#include "../interfaces/InterfaceManager.h"

static bool TryReadContainer(const CEngineCVar::Container* base, uint16_t index, CEngineCVar::Container& out) {
    __try {
        out = base[index];
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

static CConVar* TryReadCvar(const CEngineCVar::Container& entry) {
    __try { return entry.m_cvar; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}

static const char* TryReadName(const CConVar* cvar) {
    __try { return cvar->m_name; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}

static uint32_t HashSafe(const char* name) {
    if (!name) return 0;
    uint32_t hash = 0x811C9DC5u;
    __try {
        while (*name) {
            hash ^= static_cast<uint32_t>(static_cast<uint8_t>(*name++));
            hash *= 0x01000193u;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
    return hash;
}

CConVar* CEngineCVar::Find(uint32_t name_hash) {
    constexpr uint16_t invalid_index = 0xFFFF;
    constexpr uint16_t capacity_mask = 0x7FFF;

    if (!m_container) return nullptr;

    const uint16_t capacity = static_cast<uint16_t>(m_allocation_count & capacity_mask);
    if (capacity == 0) return nullptr;
    if (m_head == invalid_index) return nullptr;

    uint16_t current = m_head;

    for (std::size_t visited = 0; current != invalid_index && visited < capacity; ++visited) {
        if (current >= capacity) return nullptr;

        CEngineCVar::Container entry{};
        if (!TryReadContainer(m_container, current, entry)) return nullptr;

        CConVar* cvar = TryReadCvar(entry);
        if (cvar) {
            const char* name = TryReadName(cvar);
            if (name && HashSafe(name) == name_hash) return cvar;
        }

        current = entry.m_next_index;
    }

    return nullptr;
}

bool CEngineCVar::UnlockAll() {
    constexpr uint64_t kDevOnly = 1ull << 1;
    constexpr uint64_t kHidden = 1ull << 4;
    constexpr uint64_t kRegistryRestricted = 1ull << 10;
    constexpr uint64_t mask = kDevOnly | kHidden | kRegistryRestricted;

    constexpr uint16_t invalid_index = 0xFFFF;
    constexpr uint16_t capacity_mask = 0x7FFF;

    if (!m_container) return false;

    const uint16_t capacity = static_cast<uint16_t>(m_allocation_count & capacity_mask);
    if (capacity == 0) return false;
    if (m_head == invalid_index) return false;

    uint16_t current = m_head;

    for (std::size_t visited = 0; current != invalid_index && visited < capacity; ++visited) {
        if (current >= capacity) return false;

        CEngineCVar::Container entry{};
        if (!TryReadContainer(m_container, current, entry)) return false;

        CConVar* cvar = TryReadCvar(entry);
        if (cvar) {
            __try { cvar->m_flags &= ~mask; }
            __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        }

        current = entry.m_next_index;
    }

    return current == invalid_index;
}

namespace CVar {

    static CEngineCVar* g_cvar = nullptr;

    bool Initialize() {
        const char* modules[] = {
            "tier0.dll",
            "engine2.dll",
            "vphysics2.dll",
            "client.dll",
            "server.dll",
            "materialsystem2.dll",
            "scenesystem.dll"
        };

        for (const char* mod : modules) {
            g_cvar = InterfaceManager::Get<CEngineCVar>(mod, "VEngineCvar007");
            if (g_cvar) {
                printf("[CVar] VEngineCvar007 found in %s (0x%llX)\n",
                    mod, (unsigned long long)g_cvar);
                fflush(stdout);
                return true;
            }
        }

        printf("[CVar] VEngineCvar007 not found in any known module\n");
        fflush(stdout);
        return false;
    }

    void Shutdown() {
        g_cvar = nullptr;
    }

    CEngineCVar* GetInterface() {
        return g_cvar;
    }

    CConVar* Find(const char* name) {
        if (!g_cvar || !name) return nullptr;
        return g_cvar->Find(Hash(name));
    }

    bool GetBool(const char* name, bool fallback) {
        CConVar* c = Find(name);
        return c ? c->Get<bool>() : fallback;
    }

    int GetInt(const char* name, int fallback) {
        CConVar* c = Find(name);
        return c ? c->Get<int>() : fallback;
    }

    float GetFloat(const char* name, float fallback) {
        CConVar* c = Find(name);
        return c ? c->Get<float>() : fallback;
    }

    void SetBool(const char* name, bool value) {
        CConVar* c = Find(name);
        if (c) c->Set(value);
    }

    void SetInt(const char* name, int value) {
        CConVar* c = Find(name);
        if (c) c->Set(value);
    }

    void SetFloat(const char* name, float value) {
        CConVar* c = Find(name);
        if (c) c->Set(value);
    }

    void DumpMatching(const char* substring) {
        if (!g_cvar || !substring) return;

        constexpr uint16_t invalid_index = 0xFFFF;
        constexpr uint16_t capacity_mask = 0x7FFF;

        CEngineCVar* cv = g_cvar;
        if (!cv->m_container) return;

        const uint16_t capacity = static_cast<uint16_t>(cv->m_allocation_count & capacity_mask);
        if (capacity == 0) return;
        if (cv->m_head == invalid_index) return;

        uint16_t current = cv->m_head;
        int found = 0;

        for (std::size_t visited = 0; current != invalid_index && visited < capacity; ++visited) {
            if (current >= capacity) break;

            CEngineCVar::Container entry{};
            if (!TryReadContainer(cv->m_container, current, entry)) break;

            CConVar* cvar = TryReadCvar(entry);
            if (cvar) {
                const char* name = TryReadName(cvar);
                if (name && strstr(name, substring)) {
                    printf("[CVar]   %s\n", name);
                    found++;
                }
            }

            current = entry.m_next_index;
        }

        printf("[CVar] DumpMatching(\"%s\"): %d matches\n", substring, found);
        fflush(stdout);
    }
}