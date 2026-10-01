#include "pch.h"
#include "Renderer.h"
#include "Style.h"
#include "Widgets.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

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
    static bool                    g_mouseReleased = false;
    static float                   g_alpha = 0.0f;
    static int                     g_activeTab = 0;
    static int                     g_menuKey = VK_INSERT;
    static bool                    g_unloadRequested = false;
    static bool                    g_readyToUnload = false;

    static LRESULT CALLBACK hkWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        if (g_menuOpen) {
            ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
        }
        return CallWindowProcA(g_originalWndProc, hWnd, msg, wParam, lParam);
    }

    static void SetMouseReleased(bool released) {
        if (released == g_mouseReleased) return;
        g_mouseReleased = released;

        if (released) {
            while (ShowCursor(TRUE) < 0) {}
            ClipCursor(nullptr);
        }
        else {
            while (ShowCursor(FALSE) >= 0) {}
        }

        HMODULE sdl = GetModuleHandleA("SDL3.dll");
        if (!sdl) sdl = LoadLibraryA("SDL3.dll");
        if (sdl) {
            using SetRelMouseFn = int(*)(int);
            static SetRelMouseFn setRelMouse = nullptr;
            if (!setRelMouse) {
                setRelMouse = (SetRelMouseFn)GetProcAddress(sdl, "SDL_SetRelativeMouseMode");
            }
            if (setRelMouse) {
                setRelMouse(released ? 0 : 1);
            }
        }
    }

    static void CreateRenderTarget() {
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

    static void DrawLegitTab() {
        Widgets::SectionHeader("Aim");
        Widgets::TextMuted("Coming in a later iteration.");
    }

    static void DrawPlayersTab() {
        Widgets::SectionHeader("Chams");
        Widgets::TextMuted("Coming in a later iteration.");
    }

    static void DrawWorldTab() {
        Widgets::SectionHeader("Skybox");
        Widgets::TextMuted("Coming in a later iteration.");
    }

    static void DrawMiscTab() {
        Widgets::SectionHeader("Menu");
        Widgets::Keybind("Menu key", &g_menuKey);

        float current[4] = { Style::Accent.x, Style::Accent.y, Style::Accent.z, 1.0f };
        if (Widgets::ColorPicker("Accent color", current)) {
            Style::SetAccent(current[0], current[1], current[2]);
            Style::Apply();
        }
    }

    static void DrawConfigTab() {
        Widgets::SectionHeader("Configs");
        Widgets::TextMuted("Coming in a later iteration.");
    }

    static void DrawMenu() {
        float targetAlpha = (g_menuOpen && !g_unloadRequested) ? 1.0f : 0.0f;
        float dt = ImGui::GetIO().DeltaTime;
        float step = dt / Style::FadeDuration;
        if (g_alpha < targetAlpha) g_alpha = ImMin(targetAlpha, g_alpha + step);
        else                       g_alpha = ImMax(targetAlpha, g_alpha - step);

        if (g_alpha < 0.005f) {
            if (g_unloadRequested) g_readyToUnload = true;
            return;
        }

        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, g_alpha);

        ImGuiIO& io = ImGui::GetIO();
        ImGui::SetNextWindowPos(
            ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
            ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(
            ImVec2(Style::WindowWidth, Style::WindowHeight),
            ImGuiCond_FirstUseEver);

        ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoSavedSettings;

        ImGui::Begin("##lavender_root", nullptr, flags);

        ImGui::BeginChild("##sidebar", ImVec2(Style::SidebarWidth, 0), false);
        {
            ImGui::Dummy(ImVec2(0, 6));
            ImGui::PushFont(Style::Medium());
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 14);
            ImGui::PushStyleColor(ImGuiCol_Text, Style::Accent);
            ImGui::TextUnformatted("LAVENDER");
            ImGui::PopStyleColor();
            ImGui::PopFont();
            ImGui::Dummy(ImVec2(0, 14));

            const char* tabs[] = { "Legit", "Players", "World", "Misc", "Config" };
            for (int i = 0; i < 5; i++) {
                Widgets::SidebarTab(tabs[i], &g_activeTab, i);
            }
        }
        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_ChildBg, Style::Bg);
        ImGui::BeginChild("##content", ImVec2(0, 0), false);
        {
            ImGui::Dummy(ImVec2(0, 10));
            switch (g_activeTab) {
            case 0: DrawLegitTab();   break;
            case 1: DrawPlayersTab(); break;
            case 2: DrawWorldTab();   break;
            case 3: DrawMiscTab();    break;
            case 4: DrawConfigTab();  break;
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();

        ImGui::End();
        ImGui::PopStyleVar();
    }

    bool Initialize(IDXGISwapChain* swapChain) {
        g_swapChain = swapChain;

        if (FAILED(swapChain->GetDevice(__uuidof(ID3D11Device), (void**)&g_device))) {
            printf("[Renderer] GetDevice failed\n"); fflush(stdout);
            return false;
        }
        g_device->GetImmediateContext(&g_context);

        DXGI_SWAP_CHAIN_DESC desc;
        swapChain->GetDesc(&desc);
        g_window = desc.OutputWindow;
        printf("[Renderer] Game HWND = 0x%p\n", g_window); fflush(stdout);

        CreateRenderTarget();
        if (!g_renderTarget) {
            printf("[Renderer] CreateRenderTarget failed\n"); fflush(stdout);
            return false;
        }

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
        printf("[Renderer] ImGui initialized\n"); fflush(stdout);
        return true;
    }

    void RenderFrame() {
        if (!g_initialized) return;

        if (GetAsyncKeyState(g_menuKey) & 1) {
            g_menuOpen = !g_menuOpen;
        }

        SetMouseReleased(g_menuOpen);

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        DrawMenu();

        ImGui::Render();
        g_context->OMSetRenderTargets(1, &g_renderTarget, nullptr);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }

    void Shutdown() {
        if (!g_initialized) return;
        g_initialized = false;

        if (g_mouseReleased) SetMouseReleased(false);

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
    int  GetMenuKey() { return g_menuKey; }
}