#pragma once
#include <cstdint>

namespace BombTimer {

    void Initialize();
    void Shutdown();

    void Update();
    void Render();

    bool IsEnabled();
    void SetEnabled(bool enabled);

    bool IsDefuseEnabled();
    void SetDefuseEnabled(bool enabled);

    bool IsSiteEnabled();
    void SetSiteEnabled(bool enabled);

    float GetScale();
    void SetScale(float scale);

    float* GetColor();
    void SetColor(const float color[4]);

    float* GetDefuseColor();
    void SetDefuseColor(const float color[4]);
}