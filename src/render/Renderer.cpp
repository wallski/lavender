#include "pch.h"
#include "Renderer.h"
#include "Style.h"
#include "Widgets.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include "../core/features/visuals/chams/Chams.h"
#include "../core/features/visuals/chams/Materials.h"
#include "../core/features/visuals/removals/Removals.h"
#include "../core/features/visuals/skybox/Skybox.h"
#include "../core/features/combat/CombatShared.h"
#include "../core/features/combat/legit/triggerbot/Triggerbot.h"
#include "../core/config/ConfigManager.h"
#include "../core/hooks/InlineHook.h"
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
    static int                     g_activeTab = 0;
    static int                     g_menuKey = VK_DELETE;
    static bool                    g_unloadRequested = false;
    static bool                    g_readyToUnload = false;

    typedef int (*SDL_SetWindowRelativeMouseMode_t)(void*, int);
    typedef void* (*SDL_GetGrabbedWindow_t)();
    typedef void* (*SDL_GetKeyboardFocus_t)();
    typedef uint32_t(*SDL_GetRelativeMouseState_t)(float*, float*);

    static SDL_SetWindowRelativeMouseMode_t pSDL_SetWindowRelativeMouseMode = nullptr;
    static SDL_GetGrabbedWindow_t           pSDL_GetGrabbedWindow = nullptr;
    static SDL_GetKeyboardFocus_t           pSDL_GetKeyboardFocus = nullptr;

    static InlineHook g_sdlGetRelHook;
    static SDL_GetRelativeMouseState_t o_SDL_GetRelativeMouseState = nullptr;
    static bool g_sdlGetRelHookInstalled = false;

    static bool g_sdlInitialized = false;
    static bool g_firstFrame = true;
    static bool g_prevMenuOpen = true;
    static bool g_hasSavedInputSystem = false;
    static uint8_t g_savedInputSystemRelative = 1;

    static char g_configNameBuf[64] = "default";
    static std::string g_pendingDelete;

    static bool IsValidPointer(uintptr_t p) {
        return p >= 0x10000 && p <= 0x00007FFFFFFFFFFF;
    }

    static uint32_t hkSDL_GetRelativeMouseState(float* x, float* y) {
        if (g_menuOpen) {
            if (x) *x = 0.0f;
            if (y) *y = 0.0f;
            return 0;
        }
        if (o_SDL_GetRelativeMouseState) {
            return o_SDL_GetRelativeMouseState(x, y);
        }
        return 0;
    }

    static LRESULT CALLBACK hkWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        if (g_menuOpen) {
            if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) {
                return TRUE;
            }
        }
        return CallWindowProcA(g_originalWndProc, hWnd, msg, wParam, lParam);
    }

    static void InitSDL() {
        if (g_sdlInitialized) return;
        g_sdlInitialized = true;

        HMODULE sdl = GetModuleHandleA("SDL3.dll");
        if (!sdl) sdl = LoadLibraryA("SDL3.dll");
        if (!sdl) return;

        pSDL_SetWindowRelativeMouseMode = (SDL_SetWindowRelativeMouseMode_t)
            GetProcAddress(sdl, "SDL_SetWindowRelativeMouseMode");
        pSDL_GetGrabbedWindow = (SDL_GetGrabbedWindow_t)
            GetProcAddress(sdl, "SDL_GetGrabbedWindow");
        pSDL_GetKeyboardFocus = (SDL_GetKeyboardFocus_t)
            GetProcAddress(sdl, "SDL_GetKeyboardFocus");

        void* getRelFn = GetProcAddress(sdl, "SDL_GetRelativeMouseState");
        if (getRelFn) {
            o_SDL_GetRelativeMouseState = (SDL_GetRelativeMouseState_t)getRelFn;
            if (g_sdlGetRelHook.Install(getRelFn, (void*)hkSDL_GetRelativeMouseState)) {
                g_sdlGetRelHookInstalled = true;
                o_SDL_GetRelativeMouseState =
                    g_sdlGetRelHook.CallOriginal<SDL_GetRelativeMouseState_t>();
            }
        }
    }

    static void* GetSDLWindow() {
        void* sdlWindow = nullptr;
        if (pSDL_GetGrabbedWindow) sdlWindow = pSDL_GetGrabbedWindow();
        if (!sdlWindow && pSDL_GetKeyboardFocus) sdlWindow = pSDL_GetKeyboardFocus();
        return sdlWindow;
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
        void* sdlWindow = GetSDLWindow();
        if (sdlWindow && pSDL_SetWindowRelativeMouseMode) {
            pSDL_SetWindowRelativeMouseMode(sdlWindow, 0);
        }
        SetInputSystemRelative(false);
        ClipCursor(nullptr);
        while (ShowCursor(TRUE) < 0) {}
        SetCursor(LoadCursor(nullptr, IDC_ARROW));
    }

    static void ForceMouseRestore() {
        void* sdlWindow = GetSDLWindow();
        if (sdlWindow && pSDL_SetWindowRelativeMouseMode) {
            pSDL_SetWindowRelativeMouseMode(sdlWindow, 1);
        }
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
        InitSDL();

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

    static const char* kMaterialNames[] = {
        "None", "Matte", "Flat", "Glow", "Outlines", "Bloom"
    };

    static const char* kHitboxNames[] = {
        "Head", "Neck", "Chest", "Stomach", "Pelvis", "Arms", "Legs"
    };

    static void DrawHitboxDropdown(uint32_t* hitboxes) {
        constexpr float rowH = 30.0f;
        ImGui::PushID("##hitbox_dropdown");

        ImVec2 pos = ImGui::GetCursorScreenPos();
        float avail = ImGui::GetContentRegionAvail().x;

        ImGui::InvisibleButton("##row", ImVec2(avail, rowH));
        bool clicked = ImGui::IsItemClicked();
        bool hovered = ImGui::IsItemHovered();

        ImDrawList* d = ImGui::GetWindowDrawList();
        if (hovered) {
            d->AddRectFilled(pos, ImVec2(pos.x + avail, pos.y + rowH),
                ImGui::GetColorU32(ImVec4(1, 1, 1, 0.030f)), 4.0f);
        }

        ImVec2 labelSize = ImGui::CalcTextSize("Hitboxes");
        d->AddText(ImVec2(pos.x + Style::RowInnerPad, pos.y + rowH * 0.5f - labelSize.y * 0.5f),
            ImGui::GetColorU32(Style::Text), "Hitboxes");

        char preview[128] = "";
        for (int i = 0; i < 7; i++) {
            if (*hitboxes & (1u << i)) {
                if (preview[0]) strcat_s(preview, ", ");
                strcat_s(preview, kHitboxNames[i]);
            }
        }
        if (!preview[0]) strcpy_s(preview, "None");

        ImVec2 ts = ImGui::CalcTextSize(preview);
        d->AddText(ImVec2(pos.x + avail - ts.x - Style::RowInnerPad, pos.y + rowH * 0.5f - ts.y * 0.5f),
            ImGui::GetColorU32(Style::TextMuted), preview);

        if (clicked) ImGui::OpenPopup("##hb_popup");

        if (ImGui::BeginPopup("##hb_popup")) {
            for (int i = 0; i < 7; i++) {
                bool on = (*hitboxes & (1u << i)) != 0;
                if (ImGui::Checkbox(kHitboxNames[i], &on)) {
                    if (on) *hitboxes |= (1u << i);
                    else    *hitboxes &= ~(1u << i);
                    if (*hitboxes == 0) *hitboxes = 1u << i;
                    Triggerbot::SetHitboxes(*hitboxes);
                }
            }
            ImGui::EndPopup();
        }

        ImGui::PopID();
    }

    static void DrawLegitTab() {
        bool enabled = Triggerbot::IsEnabled();
        int key = Triggerbot::GetKey();
        bool teamCheck = Triggerbot::GetTeamCheck();
        uint32_t hitboxes = Triggerbot::GetHitboxes();
        int delay = Triggerbot::GetDelayMs();

        Widgets::BeginCard("TRIGGERBOT", 3);

        if (Widgets::Toggle("Enabled", &enabled)) {
            Triggerbot::SetEnabled(enabled);
        }
        if (Widgets::Keybind("Trigger key", &key)) {
            Triggerbot::SetKey(key);
        }
        if (Widgets::Toggle("Team check", &teamCheck)) {
            Triggerbot::SetTeamCheck(teamCheck);
        }

        Widgets::EndCard();

        Widgets::BeginCard("HITBOXES", 1);
        DrawHitboxDropdown(&hitboxes);
        Widgets::EndCard();

        Widgets::BeginCard("TIMING", 2);
        if (Widgets::SliderInt("On-target ms", &delay, 0, 1000)) {
            Triggerbot::SetDelayMs(delay);
        }
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + Style::RowInnerPad);
        ImGui::SetNextItemWidth(120);
        if (ImGui::InputInt("##delay_input", &delay, 10, 100)) {
            Triggerbot::SetDelayMs(delay);
        }
        Widgets::EndCard();
    }

    static void DrawVisualsTab() {
        bool enabled = Chams::IsEnabled();
        float* cur = Chams::GetColor();
        float color[4] = { cur[0], cur[1], cur[2], cur[3] };
        int mat = (int)Chams::GetMaterial();
        bool occluded = Chams::IsOccluded();
        bool chamsTeamCheck = Chams::IsTeamCheck();

        Widgets::BeginCard("CHAMS", 5);

        if (Widgets::Toggle("Chams", &enabled)) {
            Chams::SetEnabled(enabled);
        }
        if (Widgets::ColorPicker("Color", color)) {
            Chams::SetColor(color);
        }
        if (ImGui::Combo("Material", &mat, kMaterialNames, IM_ARRAYSIZE(kMaterialNames))) {
            Chams::SetMaterial((Materials::Id)mat);
        }
        if (Widgets::Toggle("Occluded", &occluded)) {
            Chams::SetOccluded(occluded);
        }
        if (Widgets::Toggle("Team check", &chamsTeamCheck)) {
            Chams::SetTeamCheck(chamsTeamCheck);
        }

        Widgets::EndCard();
    }

    static void DrawWorldTab() {
        bool enabled = Skybox::IsEnabled();
        int sel = Skybox::GetSelectedIndex();
        const auto& entries = Skybox::GetEntries();

        std::vector<const char*> names;
        names.reserve(entries.size());
        for (const auto& e : entries) names.push_back(e.name.c_str());

        Widgets::BeginCard("SKYBOX", 2);

        if (Widgets::Toggle("Custom skybox", &enabled)) {
            Skybox::SetEnabled(enabled);
        }

        if (!names.empty()) {
            if (ImGui::Combo("Sky", &sel, names.data(), (int)names.size())) {
                Skybox::SetSelectedIndex(sel);
            }
        }

        Widgets::EndCard();
    }

    static void DrawMiscTab() {
        float current[4] = { Style::Accent.x, Style::Accent.y, Style::Accent.z, 1.0f };

        Widgets::BeginCard("MENU", 2);
        Widgets::Keybind("Menu key", &g_menuKey);
        if (Widgets::ColorPicker("Accent color", current)) {
            Style::SetAccent(current[0], current[1], current[2]);
            Style::Apply();
        }
        Widgets::EndCard();

        bool flash = Removals::IsFlashEnabled();

        Widgets::BeginCard("REMOVALS", 1);
        if (Widgets::Toggle("Remove flash", &flash)) {
            Removals::SetFlashEnabled(flash);
        }
        Widgets::EndCard();
    }

    static void DrawConfigTab() {
        const float sideMargin = 20.0f;
        const float totalW = ImGui::GetContentRegionAvail().x;
        const float innerW = totalW - sideMargin * 2;

        ImGui::Dummy(ImVec2(0, 8));

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + sideMargin);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(14, 12));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.10f, 0.10f, 0.12f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.12f, 0.12f, 0.145f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.13f, 0.13f, 0.155f, 1.0f));
        ImGui::SetNextItemWidth(innerW);
        ImGui::InputTextWithHint("##cfg_name", "config name...", g_configNameBuf, sizeof(g_configNameBuf));
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar(2);

        ImGui::Dummy(ImVec2(0, 8));

        const float btnW = (innerW - 8) * 0.5f;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + sideMargin);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 12));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8);

        const ImVec4 accent = Style::Accent;
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(accent.x, accent.y, accent.z, 0.75f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(accent.x, accent.y, accent.z, 0.95f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(accent.x * 0.85f, accent.y * 0.85f, accent.z * 0.85f, 1.0f));
        if (ImGui::Button("Save", ImVec2(btnW, 0))) {
            if (g_configNameBuf[0]) ConfigManager::Save(g_configNameBuf);
        }
        ImGui::PopStyleColor(3);

        ImGui::SameLine(0, 8);

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.13f, 0.13f, 0.15f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.22f, 0.10f, 0.10f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.35f, 0.13f, 0.13f, 1.0f));
        if (ImGui::Button("Delete", ImVec2(btnW, 0))) {
            if (g_configNameBuf[0] && ConfigManager::Exists(g_configNameBuf)) {
                g_pendingDelete = g_configNameBuf;
                ImGui::OpenPopup("Delete Config");
            }
        }
        ImGui::PopStyleColor(3);

        ImGui::PopStyleVar(2);

        if (ImGui::BeginPopupModal("Delete Config", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Delete \"%s\"?", g_pendingDelete.c_str());
            ImGui::Dummy(ImVec2(0, 8));

            if (ImGui::Button("Yes", ImVec2(80, 0))) {
                ConfigManager::Delete(g_pendingDelete);
                g_pendingDelete.clear();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("No", ImVec2(80, 0))) {
                g_pendingDelete.clear();
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::Dummy(ImVec2(0, 18));
        Widgets::SectionHeader("Saved");

        const auto configs = ConfigManager::List();
        const std::string current = ConfigManager::GetCurrentName();

        if (configs.empty()) {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + sideMargin);
            Widgets::TextMuted("no configs saved yet");
        }
        else {
            for (const auto& name : configs) {
                ImGui::PushID(name.c_str());

                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + sideMargin);

                const ImVec2 startPos = ImGui::GetCursorScreenPos();
                ImGui::InvisibleButton("##row", ImVec2(innerW, 40.0f));
                const bool clicked = ImGui::IsItemClicked();
                const bool hovered = ImGui::IsItemHovered();
                const bool isCurrent = (name == current);

                ImDrawList* d = ImGui::GetWindowDrawList();
                const ImVec2 bgMin = startPos;
                const ImVec2 bgMax(startPos.x + innerW, startPos.y + 40.0f);

                ImU32 bg;
                if (isCurrent) {
                    bg = ImGui::GetColorU32(ImVec4(accent.x, accent.y, accent.z, 0.14f));
                }
                else if (hovered) {
                    bg = ImGui::GetColorU32(ImVec4(1, 1, 1, 0.05f));
                }
                else {
                    bg = ImGui::GetColorU32(ImVec4(1, 1, 1, 0.028f));
                }
                d->AddRectFilled(bgMin, bgMax, bg, 8.0f);

                if (isCurrent) {
                    d->AddRectFilled(
                        ImVec2(bgMin.x, bgMin.y + 10),
                        ImVec2(bgMin.x + 3, bgMax.y - 10),
                        ImGui::GetColorU32(accent), 1.5f);
                }

                const ImVec2 ts = ImGui::CalcTextSize(name.c_str());
                const ImU32 textCol = isCurrent
                    ? ImGui::GetColorU32(Style::Text)
                    : ImGui::GetColorU32(ImVec4(Style::Text.x, Style::Text.y, Style::Text.z, 0.85f));
                d->AddText(
                    ImVec2(bgMin.x + 18, bgMin.y + 20.0f - ts.y * 0.5f),
                    textCol, name.c_str());

                if (clicked) {
                    ConfigManager::Load(name);
                    strncpy_s(g_configNameBuf, name.c_str(), sizeof(g_configNameBuf) - 1);
                }

                ImGui::Dummy(ImVec2(0, 6));
                ImGui::PopID();
            }
        }
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
            ImGui::Dummy(ImVec2(0, 16));

            const char* titles[] = { "Legit",   "Visuals",        "World",          "Misc",          "Config" };
            const char* subtitles[] = { "aim assist", "visual effects", "world effects", "miscellaneous", "settings" };

            for (int i = 0; i < 5; i++) {
                Widgets::SidebarItem(titles[i], subtitles[i], &g_activeTab, i);
            }
        }
        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_ChildBg, Style::Panel);
        ImGui::BeginChild("##content", ImVec2(0, 0), false);
        {
            ImGui::Dummy(ImVec2(0, 6));
            switch (g_activeTab) {
            case 0: DrawLegitTab();   break;
            case 1: DrawVisualsTab(); break;
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

        InitSDL();

        g_initialized = true;
        g_alpha = 0.0f;
        return true;
    }

    void RenderFrame() {
        if (!g_initialized) return;

        if (GetAsyncKeyState(g_menuKey) & 1) {
            g_menuOpen = !g_menuOpen;
        }

        UpdateMouseState();
        Removals::Update();
        Triggerbot::Tick();

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

        if (g_sdlGetRelHookInstalled) {
            g_sdlGetRelHook.Remove();
            g_sdlGetRelHookInstalled = false;
        }

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
    int  GetMenuKey() { return g_menuKey; }
    void SetMenuKey(int key) { g_menuKey = key; }
}