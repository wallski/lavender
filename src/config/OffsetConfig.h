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
        uintptr_t dwGameTraceManager = 0;
        uintptr_t dwCSGOInput = 0;
        uintptr_t dwViewAngles = 0;
    };

    struct Fields {
        uint32_t m_iHealth = 0;
        uint32_t m_iTeamNum = 0;
        uint32_t m_lifeState = 0;
        uint32_t m_vecOrigin = 0;
        uint32_t m_hPlayerPawn = 0;
        uint32_t m_bDormant = 0;
        uint32_t m_pGameSceneNode = 0;
        uint32_t m_vecAbsOrigin = 0;
        uint32_t m_angAbsRotation = 0;
        uint32_t m_vecViewOffset = 0;
        uint32_t m_bGunGameImmunity = 0;
        uint32_t m_bPawnIsAlive = 0;
        uint32_t m_hPawn = 0;
        uint32_t m_fFlags = 0;
        uint32_t m_vecAbsVelocity = 0;
        uint32_t m_angEyeAngles = 0;
        uint32_t m_pAimPunchServices = 0;
        uint32_t m_pWeaponServices = 0;
        uint32_t m_hActiveWeapon = 0;
        uint32_t m_modelState = 0;
        uint32_t m_pCollision = 0;
        uint32_t m_vecMins = 0;
        uint32_t m_vecMaxs = 0;
        uint32_t m_sSanitizedPlayerName = 0;
        uint32_t m_iClip1 = 0;
        uint32_t m_iMaxClip1 = 0;
        uint32_t m_szName = 0;
        uint32_t m_nSubclassID = 0;
        uint32_t m_flFlashBangTime = 0;
        uint32_t m_flFlashScreenshotAlpha = 0;
        uint32_t m_flFlashOverlayAlpha = 0;
        uint32_t m_flFlashMaxAlpha = 0;
        uint32_t m_flFlashDuration = 0;
    };

    extern Globals globals;
    extern Fields  fields;
    extern bool    loaded;

    bool Load();
    void Resolve();
}