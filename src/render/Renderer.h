#pragma once
#include <d3d11.h>
#include <dxgi.h>

namespace Renderer {
    bool Initialize(IDXGISwapChain* swapChain);
    void Shutdown();
    void RenderFrame();
    bool IsMenuOpen();
    void ToggleMenu();
    void OnResizeBuffers();
}