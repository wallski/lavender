#pragma once
#include <cstdint>

namespace Schema {
    constexpr uint32_t m_iHealth = 0x344;
    constexpr uint32_t m_iTeamNum = 0x3E3;
    constexpr uint32_t m_lifeState = 0x338;
}

class C_BaseEntity {
public:
    int GetHealth() {
        return *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(this) + Schema::m_iHealth);
    }
    int GetTeam() {
        return *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(this) + Schema::m_iTeamNum);
    }
    int GetLifeState() {
        return *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(this) + Schema::m_lifeState);
    }
};