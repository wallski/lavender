#pragma once
#include <cstdint>
#include "UsercmdLayout.h"

namespace Triggerbot {

    using ExternalFireCheckFn = bool(*)();

    bool Initialize();
    void Shutdown();

    bool IsEnabled();
    void SetEnabled(bool enabled);

    int GetKey();
    void SetKey(int key);

    bool GetTeamCheck();
    void SetTeamCheck(bool v);

    uint32_t GetHitboxes();
    void SetHitboxes(uint32_t mask);

    int GetDelayMs();
    void SetDelayMs(int ms);

    void Tick();
    void RequestExternalFire();
    void SetExternalFireCheck(ExternalFireCheckFn fn);
}