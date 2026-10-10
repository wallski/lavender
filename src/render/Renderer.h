#pragma once
#include <d3d11.h>
#include <dxgi.h>

namespace Renderer {
    bool Initialize(IDXGISwapChain* swapChain);
    void Shutdown();
    void RenderFrame();
    bool IsMenuOpen();
    void ToggleMenu();
    void RequestUnload();
    bool ShouldUnload();
    int  GetMenuKey();
    void SetMenuKey(int key);

    void SetLagcompActive(bool active);
}