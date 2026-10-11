#pragma once
#include <cstdint>

namespace ThirdPerson {

    bool Initialize();
    void Shutdown();

    bool IsEnabled();
    void SetEnabled(bool enabled);

    int GetKey();
    void SetKey(int key);

    float GetDistance();
    void SetDistance(float distance);

    float GetHullSize();
    void SetHullSize(float size);

}