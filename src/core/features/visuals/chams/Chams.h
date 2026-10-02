#pragma once
#include <cstdint>
#include "Materials.h"

namespace Chams {
    void SetEntityListGlobal(uintptr_t addr);
    void SetLocalPawnGlobal(uintptr_t addr);

    bool IsEnabled();
    void SetEnabled(bool enabled);

    bool IsVisibleEnabled();
    void SetVisibleEnabled(bool enabled);

    bool IsOccludedEnabled();
    void SetOccludedEnabled(bool enabled);

    float* GetVisibleColor();
    float* GetOccludedColor();
    void SetVisibleColor(const float color[4]);
    void SetOccludedColor(const float color[4]);

    Materials::Id GetVisibleMaterial();
    Materials::Id GetOccludedMaterial();
    void SetVisibleMaterial(Materials::Id id);
    void SetOccludedMaterial(Materials::Id id);

    void OnSortCall(void* array, uint32_t count);

    int64_t GetTintCount();
    int64_t GetScannedCount();
}