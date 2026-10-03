#pragma once
#include <cstdint>
#include "Materials.h"

namespace Chams {
    void SetEntityListGlobal(uintptr_t addr);
    void SetLocalPawnGlobal(uintptr_t addr);

    bool IsEnabled();
    void SetEnabled(bool enabled);

    bool IsTeamCheck();
    void SetTeamCheck(bool enabled);

    Materials::Id GetMaterial();
    void SetMaterial(Materials::Id id);

    bool IsOccluded();
    void SetOccluded(bool occluded);

    float* GetColor();
    void SetColor(const float color[4]);

    void OnSortCall(void* array, uint32_t count);
}