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

    bool IsHealthEnabled();
    void SetHealthEnabled(bool enabled);

    int  GetHealthPosition();
    void SetHealthPosition(int position);

    int  GetHealthStyle();
    void SetHealthStyle(int style);

    float* GetHealthColor();
    float* GetHealthLowColor();
    float* GetHealthHighColor();
    void SetHealthColor(const float color[4]);
    void SetHealthLowColor(const float color[4]);
    void SetHealthHighColor(const float color[4]);

    bool IsHealthShowValue();
    void SetHealthShowValue(bool enabled);

    float GetHealthThickness();
    void SetHealthThickness(float thickness);

    bool IsAmmoEnabled();
    void SetAmmoEnabled(bool enabled);

    int  GetAmmoPosition();
    void SetAmmoPosition(int position);

    float* GetAmmoColor();
    void SetAmmoColor(const float color[4]);

    bool IsAmmoShowValue();
    void SetAmmoShowValue(bool enabled);

    float GetAmmoThickness();
    void SetAmmoThickness(float thickness);

    bool IsNameEnabled();
    void SetNameEnabled(bool enabled);

    int  GetNamePosition();
    void SetNamePosition(int position);

    float* GetNameColor();
    void SetNameColor(const float color[4]);

    bool IsSkeletonEnabled();
    void SetSkeletonEnabled(bool enabled);

    float* GetSkeletonColor();
    void SetSkeletonColor(const float color[4]);

    float GetSkeletonThickness();
    void SetSkeletonThickness(float thickness);

    bool IsSkeletonDebug();
    void SetSkeletonDebug(bool enabled);

    bool IsFovCircleEnabled();
    void SetFovCircleEnabled(bool enabled);

    float* GetFovCircleColor();
    void SetFovCircleColor(const float color[4]);
}