#pragma once
#include <cstdint>

namespace Aim {
    void Initialize();
    void Shutdown();
    void Tick();

    bool IsEnabled();
    void SetEnabled(bool enabled);

    int  GetKey();
    void SetKey(int key);

    float GetFov();
    void SetFov(float fov);

    int  GetSmooth();
    void SetSmooth(int smooth);

    uint32_t GetHitboxes();
    void SetHitboxes(uint32_t mask);

    bool IsTeamCheck();
    void SetTeamCheck(bool enabled);

    bool IsOnAccuracy();
    void SetOnAccuracy(bool enabled);

    float GetAccuracySpeed();
    void SetAccuracySpeed(float speed);
}