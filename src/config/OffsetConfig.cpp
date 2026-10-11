#include "pch.h"
#include "OffsetConfig.h"
#include "GeneratedOffsets.h"
#include "../core/memory/PatternScan.h"

namespace OffsetConfig {
    Globals globals;
    Fields  fields;
    bool    loaded = false;

    static bool IsReadableRange(uintptr_t addr, size_t size) {
        if (addr < 0x10000) return false;
        uintptr_t end = addr + size;
        uintptr_t cur = addr;
        while (cur < end) {
            MEMORY_BASIC_INFORMATION mbi;
            if (VirtualQuery((LPCVOID)cur, &mbi, sizeof(mbi)) == 0) return false;
            if (mbi.State != MEM_COMMIT) return false;
            if (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) return false;
            cur = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
        }
        return true;
    }

    static bool IsWritableRange(uintptr_t addr, size_t size) {
        if (addr < 0x10000) return false;
        uintptr_t end = addr + size;
        uintptr_t cur = addr;
        while (cur < end) {
            MEMORY_BASIC_INFORMATION mbi;
            if (VirtualQuery((LPCVOID)cur, &mbi, sizeof(mbi)) == 0) return false;
            if (mbi.State != MEM_COMMIT) return false;
            if (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) return false;
            if (mbi.Protect & (PAGE_READONLY | PAGE_EXECUTE_READ)) return false;
            cur = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
        }
        return true;
    }

    static bool LooksLikeGlobalVarsPtr(uintptr_t ptrPtr, uintptr_t* outDeref) {
        if (outDeref) *outDeref = 0;
        if (!IsReadableRange(ptrPtr, sizeof(uintptr_t))) return false;

        uintptr_t val = 0;
        __try { val = *reinterpret_cast<uintptr_t*>(ptrPtr); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }

        if (outDeref) *outDeref = val;
        if (val < 0x10000 || val > 0x00007FFFFFFFFFFFULL) return false;

        if (!IsReadableRange(val, 0x48)) return false;
        if (!IsWritableRange(val + 0x30, 0x18)) return false;

        return true;
    }

    bool Load() {
        globals.dwEntityList = GeneratedOffsets::Globals::dwEntityList;
        globals.dwLocalPlayerController = GeneratedOffsets::Globals::dwLocalPlayerController;
        globals.dwLocalPlayerPawn = GeneratedOffsets::Globals::dwLocalPlayerPawn;
        globals.dwViewMatrix = GeneratedOffsets::Globals::dwViewMatrix;
        globals.dwGlobalVars = GeneratedOffsets::Globals::dwGlobalVars;
        globals.dwGameRules = GeneratedOffsets::Globals::dwGameRules;
        globals.dwInputSystem = GeneratedOffsets::Globals::dwInputSystem;
        globals.dwCSGOInput = GeneratedOffsets::Globals::dwCSGOInput;
        globals.dwViewAngles = GeneratedOffsets::Globals::dwViewAngles;

        fields.m_iHealth = GeneratedOffsets::Fields::m_iHealth;
        fields.m_iTeamNum = GeneratedOffsets::Fields::m_iTeamNum;
        fields.m_lifeState = GeneratedOffsets::Fields::m_lifeState;
        fields.m_vecOrigin = GeneratedOffsets::Fields::m_vecOrigin;
        fields.m_hPlayerPawn = GeneratedOffsets::Fields::m_hPlayerPawn;
        fields.m_bDormant = GeneratedOffsets::Fields::m_bDormant;
        fields.m_pGameSceneNode = GeneratedOffsets::Fields::m_pGameSceneNode;
        fields.m_vecAbsOrigin = GeneratedOffsets::Fields::m_vecAbsOrigin;
        fields.m_angAbsRotation = GeneratedOffsets::Fields::m_angAbsRotation;
        fields.m_vecViewOffset = GeneratedOffsets::Fields::m_vecViewOffset;
        fields.m_bGunGameImmunity = GeneratedOffsets::Fields::m_bGunGameImmunity;
        fields.m_bPawnIsAlive = GeneratedOffsets::Fields::m_bPawnIsAlive;
        fields.m_fFlags = GeneratedOffsets::Fields::m_fFlags;
        fields.m_vecAbsVelocity = GeneratedOffsets::Fields::m_vecAbsVelocity;
        fields.m_angEyeAngles = GeneratedOffsets::Fields::m_angEyeAngles;
        fields.m_pAimPunchServices = GeneratedOffsets::Fields::m_pAimPunchServices;
        fields.m_pWeaponServices = GeneratedOffsets::Fields::m_pWeaponServices;
        fields.m_pMovementServices = GeneratedOffsets::Fields::m_pMovementServices;
        fields.m_hActiveWeapon = GeneratedOffsets::Fields::m_hActiveWeapon;
        fields.m_modelState = GeneratedOffsets::Fields::m_modelState;
        fields.m_pCollision = GeneratedOffsets::Fields::m_pCollision;
        fields.m_vecMins = GeneratedOffsets::Fields::m_vecMins;
        fields.m_vecMaxs = GeneratedOffsets::Fields::m_vecMaxs;
        fields.m_sSanitizedPlayerName = GeneratedOffsets::Fields::m_sSanitizedPlayerName;
        fields.m_iClip1 = GeneratedOffsets::Fields::m_iClip1;
        fields.m_iMaxClip1 = GeneratedOffsets::Fields::m_iMaxClip1;
        fields.m_szName = GeneratedOffsets::Fields::m_szName;
        fields.m_nSubclassID = GeneratedOffsets::Fields::m_nSubclassID;
        fields.m_flFlashBangTime = GeneratedOffsets::Fields::m_flFlashBangTime;
        fields.m_flFlashScreenshotAlpha = GeneratedOffsets::Fields::m_flFlashScreenshotAlpha;
        fields.m_flFlashOverlayAlpha = GeneratedOffsets::Fields::m_flFlashOverlayAlpha;
        fields.m_flFlashMaxAlpha = GeneratedOffsets::Fields::m_flFlashMaxAlpha;
        fields.m_flFlashDuration = GeneratedOffsets::Fields::m_flFlashDuration;

        if (globals.dwEntityList == 0 ||
            globals.dwLocalPlayerPawn == 0 ||
            fields.m_iTeamNum == 0 ||
            fields.m_pGameSceneNode == 0) {
            printf("[OffsetConfig] Generated offsets are empty/stale - run a2x and rebuild\n");
            fflush(stdout);
            return false;
        }

        printf("[OffsetConfig] Loaded embedded offsets (entityList=0x%llX localPawn=0x%llX)\n",
            (unsigned long long)globals.dwEntityList,
            (unsigned long long)globals.dwLocalPlayerPawn);
        fflush(stdout);

        loaded = true;
        return true;
    }

    static uintptr_t GetModuleBase(const char* name) {
        HMODULE h = GetModuleHandleA(name);
        return h ? reinterpret_cast<uintptr_t>(h) : 0;
    }

    static uintptr_t ResolveMovRip(const char* module, const char* pattern) {
        uintptr_t instr = Memory::FindPattern(module, pattern);
        if (!instr) return 0;
        int32_t rel = *reinterpret_cast<int32_t*>(instr + 3);
        return instr + 7 + rel;
    }

    void Resolve() {
        uintptr_t clientBase = GetModuleBase("client.dll");
        uintptr_t engineBase = GetModuleBase("engine2.dll");
        uintptr_t inputBase = GetModuleBase("inputsystem.dll");

        printf("[OffsetConfig] Resolving RVAs (client=0x%llX engine=0x%llX input=0x%llX)\n",
            (unsigned long long)clientBase, (unsigned long long)engineBase, (unsigned long long)inputBase);
        fflush(stdout);

        globals.dwEntityList += clientBase;
        globals.dwLocalPlayerController += clientBase;
        globals.dwLocalPlayerPawn += clientBase;
        globals.dwViewMatrix += clientBase;
        globals.dwGameRules += clientBase;
        globals.dwCSGOInput += clientBase;
        globals.dwViewAngles += clientBase;

        {
            uintptr_t rva = globals.dwGlobalVars;
            uintptr_t engineCandidate = rva + engineBase;
            uintptr_t clientCandidate = rva + clientBase;

            uintptr_t clientDeref = 0;
            uintptr_t engineDeref = 0;

            const bool clientOk = LooksLikeGlobalVarsPtr(clientCandidate, &clientDeref);
            const bool engineOk = LooksLikeGlobalVarsPtr(engineCandidate, &engineDeref);

            printf("[OffsetConfig] dwGlobalVars candidates:\n");
            printf("[OffsetConfig]   client=0x%llX deref=0x%llX valid=%d\n",
                (unsigned long long)clientCandidate,
                (unsigned long long)clientDeref,
                clientOk ? 1 : 0);
            printf("[OffsetConfig]   engine=0x%llX deref=0x%llX valid=%d\n",
                (unsigned long long)engineCandidate,
                (unsigned long long)engineDeref,
                engineOk ? 1 : 0);

            if (clientOk) {
                globals.dwGlobalVars = clientCandidate;
                printf("[OffsetConfig] dwGlobalVars: using client base\n");
            }
            else if (engineOk) {
                globals.dwGlobalVars = engineCandidate;
                printf("[OffsetConfig] dwGlobalVars: using engine base\n");
            }
            else {
                globals.dwGlobalVars = clientCandidate;
                printf("[OffsetConfig] dwGlobalVars: neither candidate validated, fallback to client\n");
            }
            fflush(stdout);
        }

        globals.dwInputSystem += inputBase;

        if (!globals.dwInputSystem) {
            globals.dwInputSystem = ResolveMovRip("inputsystem.dll", "48 8B 0D ? ? ? ? 48 8B 01 FF 50");
        }

        uintptr_t gtm = Memory::FindPattern("client.dll", "48 8B 0D ? ? ? ? 48 8D 34 52");
        if (gtm) {
            int32_t rel = *reinterpret_cast<int32_t*>(gtm + 3);
            uintptr_t resolved = gtm + 7 + rel;
            globals.dwGameTraceManager = *reinterpret_cast<uintptr_t*>(resolved);
        }

        printf("[OffsetConfig] Resolved absolute addresses:\n");
        printf("  dwEntityList            = 0x%llX\n", (unsigned long long)globals.dwEntityList);
        printf("  dwLocalPlayerController = 0x%llX\n", (unsigned long long)globals.dwLocalPlayerController);
        printf("  dwLocalPlayerPawn       = 0x%llX\n", (unsigned long long)globals.dwLocalPlayerPawn);
        printf("  dwViewMatrix            = 0x%llX\n", (unsigned long long)globals.dwViewMatrix);
        printf("  dwGameRules             = 0x%llX\n", (unsigned long long)globals.dwGameRules);
        printf("  dwGlobalVars            = 0x%llX\n", (unsigned long long)globals.dwGlobalVars);
        printf("  dwInputSystem           = 0x%llX\n", (unsigned long long)globals.dwInputSystem);
        printf("  dwCSGOInput             = 0x%llX\n", (unsigned long long)globals.dwCSGOInput);
        printf("  dwViewAngles            = 0x%llX\n", (unsigned long long)globals.dwViewAngles);
        printf("  dwGameTraceManager      = 0x%llX\n", (unsigned long long)globals.dwGameTraceManager);
        fflush(stdout);
    }
}