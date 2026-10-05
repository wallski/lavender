#pragma once
#include <cstdint>

namespace Removals {
    bool Initialize();
    void Shutdown();

    bool IsFlashEnabled();
    void SetFlashEnabled(bool enabled);

    bool IsSmokeEnabled();
    void SetSmokeEnabled(bool enabled);

    void Update();
}