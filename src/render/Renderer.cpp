#include "pch.h"
#include "Renderer.h"
#include "Menu.h"
#include "Style.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include "../core/features/visuals/esp/Esp.h"
#include "../core/features/visuals/removals/Removals.h"
#include "../core/features/combat/CombatShared.h"
#include "../core/features/combat/legit/triggerbot/Triggerbot.h"
#include "../core/features/combat/legit/aim/Aim.h"
#include "../config/OffsetConfig.h"

LRESULT WINAPI ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace Renderer {

    static ID3D11Device* g_device = nullptr;
    static ID3D11DeviceContext* g_context = nullptr;
    static ID3D11RenderTargetView* g_renderTarget = nullptr;
    static IDXGISwapChain* g_swapChain = nullptr;
    static HWND                    g_window = nullptr;
    static WNDPROC                 g_originalWndProc = nullptr;
    static bool                    g_initialized = false;
    static bool                    g_menuOpen = true;
    static float                   g_alpha = 0.0f;
    static bool                    g_unloadRequested = false;
    static bool                    g_readyToUnload = false;
    static int                     g_skipFrames = 0;

    static bool g_firstFrame = true;
    static bool g_prevMenuOpen = true;
    static bool g_hasSavedInputSystem = false;
    static uint8_t g_savedInputSystemRelative = 1;

    static bool IsValidPointer(uintptr_t p) {
        return p >= 0x10000 && p <= 0x00007FFFFFFFFFFF;
    }

    static LRESULT CALLBACK hkWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        if (g_menuOpen) {
            ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);

            switch (msg) {
            case WM_MOUSEMOVE:
            case WM_LBUTTONDOWN:
            case WM_LBUTTONUP:
            case WM_LBUTTONDBLCLK:
            case WM_RBUTTONDOWN:
            case WM_RBUTTONUP:
            case WM_RBUTTONDBLCLK:
            case WM_MBUTTONDOWN:
            case WM_MBUTTONUP:
            case WM_MBUTTONDBLCLK:
            case WM_MOUSEWHEEL:
            case WM_MOUSEHWHEEL:
            case WM_XBUTTONDOWN:
            case WM_XBUTTONUP:
            case WM_XBUTTONDBLCLK:
            case WM_INPUT:
            case WM_NCMOUSEMOVE:
            case WM_NCLBUTTONDOWN:
            case WM_NCLBUTTONUP:
            case WM_NCRBUTTONDOWN:
            case WM_NCRBUTTONUP:
            case WM_NCMBUTTONDOWN:
            case WM_NCMBUTTONUP:
                return TRUE;
            }
        }
        return CallWindowProcA(g_originalWndProc, hWnd, msg, wParam, lParam);
    }

    static void SetInputSystemRelative(bool relative) {
        if (!OffsetConfig::globals.dwInputSystem) return;

        __try {
            uintptr_t instance = OffsetConfig::globals.dwInputSystem;
            if (!IsValidPointer(instance)) return;

            void** vtable = *reinterpret_cast<void***>(instance);
            if (!IsValidPointer(reinterpret_cast<uintptr_t>(vtable))) return;

            uintptr_t fnAddr = reinterpret_cast<uintptr_t>(vtable[76]);
            if (!IsValidPointer(fnAddr)) return;

            auto setRelMouse = reinterpret_cast<void(*)(void*, bool)>(fnAddr);
            setRelMouse(reinterpret_cast<void*>(instance), relative);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    static void SaveInputSystemState() {
        if (!OffsetConfig::globals.dwInputSystem) return;
        __try {
            uintptr_t instance = OffsetConfig::globals.dwInputSystem;
            if (!IsValidPointer(instance)) return;
            g_savedInputSystemRelative = *reinterpret_cast<uint8_t*>(instance + 84);
            g_hasSavedInputSystem = true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    static void ForceMouseUnlock() {
        SetInputSystemRelative(false);
        ClipCursor(nullptr);
        while (ShowCursor(TRUE) < 0) {}
        SetCursor(LoadCursor(nullptr, IDC_ARROW));
    }

    static void ForceMouseRestore() {
        SetInputSystemRelative(g_hasSavedInputSystem ? (g_savedInputSystemRelative != 0) : true);

        if (g_window) {
            RECT rect;
            GetWindowRect(g_window, &rect);
            ClipCursor(&rect);
            SetCursorPos((rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2);
        }
        while (ShowCursor(FALSE) >= 0) {}
    }

    static void UpdateMouseState() {
        if (g_firstFrame) {
            g_firstFrame = false;
            g_prevMenuOpen = g_menuOpen;
            if (g_menuOpen) {
                SaveInputSystemState();
                ForceMouseUnlock();
            }
            return;
        }

        if (g_menuOpen != g_prevMenuOpen) {
            if (g_menuOpen) {
                SaveInputSystemState();
                ForceMouseUnlock();
            }
            else {
                ForceMouseRestore();
            }
            g_prevMenuOpen = g_menuOpen;
        }
    }

    static bool IsDeviceAlive() {
        if (!g_device) return false;
        __try {
            return g_device->GetDeviceRemovedReason() == S_OK;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    static void CreateRenderTarget() {
        if (!g_swapChain || !g_device) return;

        ID3D11Texture2D* backBuffer = nullptr;
        HRESULT hr = g_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
        if (SUCCEEDED(hr) && backBuffer) {
            g_device->CreateRenderTargetView(backBuffer, nullptr, &g_renderTarget);
            backBuffer->Release();
        }
    }

    static void DestroyRenderTarget() {
        if (g_renderTarget) {
            g_renderTarget->Release();
            g_renderTarget = nullptr;
        }
    }

    bool Initialize(IDXGISwapChain* swapChain) {
        g_swapChain = swapChain;

        if (FAILED(swapChain->GetDevice(__uuidof(ID3D11Device), (void**)&g_device))) {
            return false;
        }
        g_device->GetImmediateContext(&g_context);

        DXGI_SWAP_CHAIN_DESC desc;
        swapChain->GetDesc(&desc);
        g_window = desc.OutputWindow;

        CreateRenderTarget();
        if (!g_renderTarget) return false;

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.IniFilename = nullptr;

        Style::LoadFonts();
        Style::Apply();

        ImGui_ImplWin32_Init(g_window);
        ImGui_ImplDX11_Init(g_device, g_context);

        g_originalWndProc = (WNDPROC)SetWindowLongPtrA(g_window, GWLP_WNDPROC, (LONG_PTR)hkWndProc);

        g_initialized = true;
        g_alpha = 0.0f;
        return true;
    }

    void RenderFrame() {
        if (!g_initialized) return;
        if (!g_device || !g_context) return;
        if (!IsDeviceAlive()) return;

        if (GetAsyncKeyState(Menu::GetMenuKey()) & 1) {
            g_menuOpen = !g_menuOpen;
        }

        UpdateMouseState();
        CombatShared::BeginFrame();
        Removals::Update();
        Triggerbot::Tick();
        Aim::Tick();

        if (g_skipFrames > 0) {
            g_skipFrames--;
            return;
        }

        if (!g_renderTarget) return;

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        Esp::Render();

        float targetAlpha = (g_menuOpen && !g_unloadRequested) ? 1.0f : 0.0f;
        float dt = ImGui::GetIO().DeltaTime;
        float step = dt / Style::FadeDuration;
        if (g_alpha < targetAlpha) g_alpha = ImMin(targetAlpha, g_alpha + step);
        else                       g_alpha = ImMax(targetAlpha, g_alpha - step);

        if (g_alpha < 0.005f) {
            if (g_unloadRequested) g_readyToUnload = true;
        }
        else {
            Menu::Draw(g_alpha);
        }

        ImGui::Render();

        g_context->OMSetRenderTargets(1, &g_renderTarget, nullptr);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }

    void Shutdown() {
        if (!g_initialized) return;
        g_initialized = false;

        if (!g_prevMenuOpen) {
            ForceMouseRestore();
        }

        if (g_originalWndProc && g_window) {
            SetWindowLongPtrA(g_window, GWLP_WNDPROC, (LONG_PTR)g_originalWndProc);
            g_originalWndProc = nullptr;
        }

        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

        DestroyRenderTarget();
        if (g_context) { g_context->Release(); g_context = nullptr; }
        if (g_device) { g_device->Release();  g_device = nullptr; }
    }

    bool IsMenuOpen() { return g_menuOpen; }
    void ToggleMenu() { g_menuOpen = !g_menuOpen; }
    void RequestUnload() { g_unloadRequested = true; }
    bool ShouldUnload() { return g_readyToUnload; }
    int  GetMenuKey() { return Menu::GetMenuKey(); }
    void SetMenuKey(int key) { Menu::SetMenuKey(key); }

    void OnPreResize() {
        ImGui_ImplDX11_InvalidateDeviceObjects();
        DestroyRenderTarget();
    }

    void OnPostResize() {
        if (!IsDeviceAlive()) return;
        CreateRenderTarget();
        if (g_renderTarget) {
            ImGui_ImplDX11_CreateDeviceObjects();
            g_skipFrames = 3;
        }
    }
}