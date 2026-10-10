#pragma once
#include <cstdint>

namespace Rage {
    void Initialize();
    void Shutdown();

    void Tick();

    bool IsEnabled();
    void SetEnabled(bool enabled);

    bool IsTeamCheck();
    void SetTeamCheck(bool enabled);

    uint32_t GetHitboxes();
    void SetHitboxes(uint32_t mask);

    float GetFov();
    void SetFov(float fov);

    int GetMinDamage();
    void SetMinDamage(int damage);

    bool IsSilent();
    void SetSilent(bool enabled);

    bool IsAutoShoot();
    void SetAutoShoot(bool enabled);

    bool IsNoSpread();
    void SetNoSpread(bool enabled);

    bool IsAutoScope();
    void SetAutoScope(bool enabled);
}