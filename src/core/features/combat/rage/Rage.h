#pragma once
#include <cstdint>

namespace Rage {

    enum class Group : int {
        Pistol = 0,
        SMG = 1,
        Rifle = 2,
        Shotgun = 3,
        Sniper = 4,
        LMG = 5,
        Count = 6
    };

    void Initialize();
    void Shutdown();
    void Tick();

    bool IsEnabled(Group g);
    void SetEnabled(Group g, bool enabled);

    bool IsAutoShoot(Group g);
    void SetAutoShoot(Group g, bool enabled);

    bool IsSilent(Group g);
    void SetSilent(Group g, bool enabled);

    bool IsNoSpread(Group g);
    void SetNoSpread(Group g, bool enabled);

    bool IsAutowall(Group g);
    void SetAutowall(Group g, bool enabled);

    bool IsTeamCheck(Group g);
    void SetTeamCheck(Group g, bool enabled);

    bool IsBacktrack(Group g);
    void SetBacktrack(Group g, bool enabled);

    int GetPointscale(Group g);
    void SetPointscale(Group g, int scale);

    int GetMinDamage(Group g);
    void SetMinDamage(Group g, int damage);

    uint32_t GetHitboxes(Group g);
    void SetHitboxes(Group g, uint32_t mask);
}