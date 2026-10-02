#pragma once
#include <cstdint>

namespace Triggerbot {
    bool Initialize();
    void Shutdown();

    bool IsEnabled();
    void SetEnabled(bool enabled);

    int GetKey();
    void SetKey(int key);

    bool GetHeadOnly();
    void SetHeadOnly(bool headOnly);

    int GetDelayMs();
    void SetDelayMs(int ms);

    void Tick();
    void OnGameThreadWrite();
}