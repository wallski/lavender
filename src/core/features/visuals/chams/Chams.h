#pragma once
#include <cstdint>

namespace Chams {
    void SetEntityListGlobal(uintptr_t addr);
    void SetLocalPawnGlobal(uintptr_t addr);

    bool IsEnabled();
    void SetEnabled(bool enabled);

    float* GetColor();
    void SetColor(const float color[4]);

    void OnSortCall(void* array, uint32_t count);

    int64_t GetTintCount();
    int64_t GetScannedCount();
}