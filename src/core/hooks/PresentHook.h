#pragma once
#include "VmtHook.h"
#include <d3d11.h>
#include <dxgi.h>
#include <dxgi1_2.h>
#include <vector>

namespace PresentHook {
    bool Initialize();
    void Shutdown();
    LONG GetPresentCount();
    LONG GetPresent1Count();
    size_t GetHookCount();
}