#pragma once
#include "../Lagcomp.h"
#include <cstdint>

namespace Resolver {
    void DetectSide(uintptr_t player, int* side);
    float BuildServerAbsYaw(uintptr_t player, float angle);
    void ResolveAngles(uintptr_t player, Lagcomp::Record* record, int missed_shots, int* side);
}