#pragma once
#include <cstdint>

namespace GeneratedOffsets {

    namespace Globals {
        constexpr uintptr_t dwEntityList = 0x2715818;
        constexpr uintptr_t dwLocalPlayerController = 0x2538008;
        constexpr uintptr_t dwLocalPlayerPawn = 0x2560698;
        constexpr uintptr_t dwViewMatrix = 0x2566910;
        constexpr uintptr_t dwGlobalVars = 0x222BE98;
        constexpr uintptr_t dwGameRules = 0x255CE50;
        constexpr uintptr_t dwInputSystem = 0x46BC0;
    }

    namespace Fields {
        constexpr uint32_t m_iHealth = 0x34C;
        constexpr uint32_t m_iTeamNum = 0x3E7;
        constexpr uint32_t m_lifeState = 0x354;
        constexpr uint32_t m_vecOrigin = 0x600;
        constexpr uint32_t m_hPlayerPawn = 0x92C;
        constexpr uint32_t m_bDormant = 0x103;
        constexpr uint32_t m_pGameSceneNode = 0x330;
        constexpr uint32_t m_vecAbsOrigin = 0xC8;
        constexpr uint32_t m_angAbsRotation = 0xD4;
        constexpr uint32_t m_vecViewOffset = 0xF60;
        constexpr uint32_t m_bGunGameImmunity = 0x3508;
        constexpr uint32_t m_bPawnIsAlive = 0x934;
        constexpr uint32_t m_hPawn = 0x600;
        constexpr uint32_t m_fFlags = 0x3F4;
        constexpr uint32_t m_angEyeAngles = 0x35F0;
        constexpr uint32_t m_pAimPunchServices = 0x1598;
        constexpr uint32_t m_pWeaponServices = 0x12F0;
        constexpr uint32_t m_hActiveWeapon = 0x60;
        constexpr uint32_t m_modelState = 0x140;
        constexpr uint32_t m_pCollision = 0x340;
        constexpr uint32_t m_vecMins = 0x40;
        constexpr uint32_t m_vecMaxs = 0x4C;
        constexpr uint32_t m_sSanitizedPlayerName = 0x878;
        constexpr uint32_t m_iClip1 = 0x1928;
        constexpr uint32_t m_iMaxClip1 = 0x4D0;
        constexpr uint32_t m_szName = 0x720;
        constexpr uint32_t m_nSubclassID = 0x380;
    }

}
