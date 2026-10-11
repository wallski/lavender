#pragma once
#include <cstdint>

namespace Backtrack {

    void Render();

    bool IsEnabled();
    void SetEnabled(bool enabled);

    bool IsTeamCheck();
    void SetTeamCheck(bool enabled);

    float* GetVisibleColor();
    float* GetOccludedColor();
    void SetVisibleColor(const float color[4]);
    void SetOccludedColor(const float color[4]);

    float GetThickness();
    void SetThickness(float thickness);
}