#pragma once
#include <cstdint>

namespace Offsets {
    bool Initialize();

    inline uintptr_t dwEntityList = 0;
    inline uintptr_t dwLocalPlayerController = 0;
    inline uintptr_t dwLocalPlayerPawn = 0;
    inline uintptr_t dwViewMatrix = 0;
    inline uintptr_t dwGlobalVars = 0;
    inline uintptr_t dwGameRules = 0;
}