#include "pch.h"
#include "OffsetManager.h"
#include "PatternScan.h"

namespace Offsets {

    static uintptr_t ResolveMovRip(const char* module, const char* pattern) {
        uintptr_t instr = Memory::FindPattern(module, pattern);
        if (!instr) {
            printf("[Offsets] FAIL: pattern not found in %s\n", module);
            fflush(stdout);
            return 0;
        }
        int32_t rel = *reinterpret_cast<int32_t*>(instr + 3);
        return instr + 7 + rel;
    }

    bool Initialize() {
        printf("[Offsets] Resolving engine globals...\n"); fflush(stdout);

        dwEntityList = ResolveMovRip("client.dll", "48 89 0D ? ? ? ? E9 ? ? ? ? CC");
        printf("[Offsets]   dwEntityList            = 0x%llX\n", (unsigned long long)dwEntityList); fflush(stdout);

        dwViewMatrix = ResolveMovRip("client.dll", "48 8D 0D ? ? ? ? 48 C1 E0 06");
        printf("[Offsets]   dwViewMatrix            = 0x%llX\n", (unsigned long long)dwViewMatrix); fflush(stdout);

        dwLocalPlayerController = ResolveMovRip("client.dll", "48 8B 05 ? ? ? ? 41 89 BE");
        printf("[Offsets]   dwLocalPlayerController = 0x%llX\n", (unsigned long long)dwLocalPlayerController); fflush(stdout);

        dwLocalPlayerPawn = ResolveMovRip("client.dll", "48 8D 05 ? ? ? ? C3 CC CC CC CC CC CC CC CC 40 53 56 41 54");
        printf("[Offsets]   dwLocalPlayerPawn       = 0x%llX\n", (unsigned long long)dwLocalPlayerPawn); fflush(stdout);

        dwGlobalVars = ResolveMovRip("engine2.dll", "48 8B 0D ? ? ? ? 48 8B 01 FF 50");
        printf("[Offsets]   dwGlobalVars            = 0x%llX\n", (unsigned long long)dwGlobalVars); fflush(stdout);

        dwGameRules = ResolveMovRip("client.dll", "F6 C1 01 0F 85 ? ? ? ? 4C 8B 05 ? ? ? ? 4D 85");
        printf("[Offsets]   dwGameRules             = 0x%llX\n", (unsigned long long)dwGameRules); fflush(stdout);

        int resolved = 0;
        if (dwEntityList) resolved++;
        if (dwViewMatrix) resolved++;
        if (dwLocalPlayerController) resolved++;
        if (dwLocalPlayerPawn) resolved++;
        if (dwGlobalVars) resolved++;
        if (dwGameRules) resolved++;

        printf("[Offsets] %d/6 globals resolved.\n", resolved); fflush(stdout);
        return resolved > 0;
    }
}