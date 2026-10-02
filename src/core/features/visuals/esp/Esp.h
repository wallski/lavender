#pragma once
#include <cstdint>

namespace Esp {

    void Render();
    void Shutdown();

    bool IsEnabled();
    void SetEnabled(bool enabled);

    bool IsTeamCheck();
    void SetTeamCheck(bool enabled);

    bool IsBoxEnabled();
    void SetBoxEnabled(bool enabled);

    int  GetBoxStyle();
    void SetBoxStyle(int style);

    bool IsBoxFillEnabled();
    void SetBoxFillEnabled(bool enabled);

    bool IsBoxOutlineEnabled();
    void SetBoxOutlineEnabled(bool enabled);

    float GetBoxCornerLength();
    void SetBoxCornerLength(float length);

    float* GetBoxVisibleColor();
    float* GetBoxFillColor();
    void SetBoxVisibleColor(const float color[4]);
    void SetBoxFillColor(const float color[4]);

}