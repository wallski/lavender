#pragma once
#include <cstdint>

namespace Triggerbot {
    bool Initialize();
    void Shutdown();

    bool IsEnabled();
    void SetEnabled(bool enabled);

    int GetKey();
    void SetKey(int key);

    bool GetTeamCheck();
    void SetTeamCheck(bool v);

    uint32_t GetHitboxes();
    void SetHitboxes(uint32_t v);

    int GetDelayMs();
    void SetDelayMs(int ms);

    void Tick();
    void OnGameThreadWrite();
}