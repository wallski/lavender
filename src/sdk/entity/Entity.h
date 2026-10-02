#pragma once
#include <cstdint>

namespace Schema {
    constexpr uint32_t m_iHealth = 0x34C;
    constexpr uint32_t m_iTeamNum = 0x3E7;
    constexpr uint32_t m_lifeState = 0x354;
    constexpr uint32_t m_vecOrigin = 0x600;
    constexpr uint32_t m_clrRender = 0xCA0;
    constexpr uint32_t m_nRenderMode = 0xC80;
    constexpr uint32_t m_ClientOverrideTint = 0x1048;
    constexpr uint32_t m_bUseClientOverrideTint = 0x104C;
}

class C_BaseEntity {
public:
    int GetHealth() { return *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(this) + Schema::m_iHealth); }
    int GetTeam() { return *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(this) + Schema::m_iTeamNum); }
    int GetLifeState() { return *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(this) + Schema::m_lifeState); }
    bool IsAlive() { return GetLifeState() == 0; }
    bool IsPlayer() {
        int h = GetHealth();
        int t = GetTeam();
        return h > 0 && h <= 200 && (t == 2 || t == 3);
    }
};