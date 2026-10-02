#pragma once
#include <cstdint>
#include <string>

namespace OffsetConfig {
    struct Globals {
        uintptr_t dwEntityList = 0;
        uintptr_t dwLocalPlayerController = 0;
        uintptr_t dwLocalPlayerPawn = 0;
        uintptr_t dwViewMatrix = 0;
        uintptr_t dwGlobalVars = 0;
        uintptr_t dwGameRules = 0;
        uintptr_t dwInputSystem = 0;
    };

    struct Fields {
        uint32_t m_iHealth = 0;
        uint32_t m_iTeamNum = 0;
        uint32_t m_lifeState = 0;
        uint32_t m_vecOrigin = 0;
        uint32_t m_hPlayerPawn = 0;
        uint32_t m_bDormant = 0;
    };

    extern Globals globals;
    extern Fields  fields;
    extern bool    loaded;

    bool Load();
    bool Load(const std::string& a2xFolder);
    void Resolve();
}