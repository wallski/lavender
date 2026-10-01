#include "pch.h"
#include "Style.h"

namespace Style {
    ImVec4 Bg = ImVec4(0.078f, 0.078f, 0.102f, 0.82f);
    ImVec4 Panel = ImVec4(0.055f, 0.055f, 0.075f, 0.50f);
    ImVec4 PanelHover = ImVec4(0.133f, 0.133f, 0.169f, 0.60f);
    ImVec4 Border = ImVec4(0.149f, 0.149f, 0.184f, 0.55f);
    ImVec4 Text = ImVec4(0.910f, 0.910f, 0.925f, 1.00f);
    ImVec4 TextMuted = ImVec4(0.478f, 0.478f, 0.522f, 0.90f);
    ImVec4 Accent = ImVec4(0.545f, 0.361f, 0.965f, 1.00f);
    ImVec4 AccentDim = ImVec4(0.435f, 0.286f, 0.786f, 1.00f);

    static ImFont* g_fontRegular = nullptr;
    static ImFont* g_fontMedium = nullptr;

    ImFont* Regular() { return g_fontRegular; }
    ImFont* Medium() { return g_fontMedium; }

    void SetAccent(float r, float g, float b) {
        Accent = ImVec4(r, g, b, 1.0f);
        AccentDim = ImVec4(r * 0.8f, g * 0.8f, b * 0.8f, 1.0f);
    }

    static std::string FindFontPath(const char* filename) {
        HMODULE hm = NULL;
        GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCSTR)&FindFontPath, &hm);
        char path[MAX_PATH] = { 0 };
        GetModuleFileNameA(hm, path, MAX_PATH);
        std::string dir(path);
        size_t p = dir.find_last_of("\\/");
        if (p != std::string::npos) dir = dir.substr(0, p);

        for (int i = 0; i < 6; i++) {
            std::string candidate = dir + "\\ThirdParty\\fonts\\" + filename;
            std::ifstream test(candidate);
            if (test.is_open()) return candidate;
            size_t pos = dir.find_last_of("\\/");
            if (pos == std::string::npos) break;
            dir = dir.substr(0, pos);
        }
        return "";
    }

    void LoadFonts() {
        ImGuiIO& io = ImGui::GetIO();
        io.Fonts->Clear();

        ImFontConfig cfg;
        cfg.OversampleH = 2;
        cfg.OversampleV = 2;
        cfg.PixelSnapH = true;

        std::string regularPath = FindFontPath("Inter-Regular.ttf");
        std::string mediumPath = FindFontPath("Inter-Medium.ttf");

        if (!regularPath.empty()) {
            g_fontRegular = io.Fonts->AddFontFromFileTTF(regularPath.c_str(), 15.0f, &cfg);
            printf("[Style] Loaded: %s\n", regularPath.c_str());
        }
        if (!mediumPath.empty()) {
            g_fontMedium = io.Fonts->AddFontFromFileTTF(mediumPath.c_str(), 15.0f, &cfg);
            printf("[Style] Loaded: %s\n", mediumPath.c_str());
        }

        if (!g_fontRegular) {
            const char* segoe = "C:\\Windows\\Fonts\\segoeui.ttf";
            std::ifstream test(segoe);
            if (test.is_open()) {
                g_fontRegular = io.Fonts->AddFontFromFileTTF(segoe, 15.0f, &cfg);
                printf("[Style] Fallback font: Segoe UI\n");
            }
        }
        if (!g_fontRegular) {
            g_fontRegular = io.Fonts->AddFontDefault();
            printf("[Style] Fallback font: ImGui default\n");
        }
        if (!g_fontMedium) g_fontMedium = g_fontRegular;

        fflush(stdout);
    }

    void Apply() {
        ImGuiStyle& s = ImGui::GetStyle();

        s.WindowPadding = ImVec2(Padding, Padding);
        s.FramePadding = ImVec2(12, 6);
        s.ItemSpacing = ImVec2(10, 6);
        s.ItemInnerSpacing = ImVec2(8, 6);
        s.ScrollbarSize = 8.0f;
        s.GrabMinSize = 10.0f;

        s.WindowBorderSize = 0.0f;
        s.ChildBorderSize = 0.0f;
        s.PopupBorderSize = 1.0f;
        s.FrameBorderSize = 0.0f;

        s.WindowRounding = Rounding;
        s.ChildRounding = RoundingSm;
        s.FrameRounding = RoundingSm;
        s.PopupRounding = RoundingSm;
        s.ScrollbarRounding = RoundingSm;
        s.GrabRounding = RoundingSm;
        s.TabRounding = RoundingSm;

        ImVec4* c = s.Colors;
        c[ImGuiCol_Text] = Text;
        c[ImGuiCol_TextDisabled] = TextMuted;
        c[ImGuiCol_WindowBg] = Bg;
        c[ImGuiCol_ChildBg] = Panel;
        c[ImGuiCol_PopupBg] = ImVec4(0.078f, 0.078f, 0.102f, 0.95f);
        c[ImGuiCol_Border] = Border;
        c[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
        c[ImGuiCol_FrameBg] = PanelHover;
        c[ImGuiCol_FrameBgHovered] = PanelHover;
        c[ImGuiCol_FrameBgActive] = PanelHover;
        c[ImGuiCol_TitleBg] = Bg;
        c[ImGuiCol_TitleBgActive] = Bg;
        c[ImGuiCol_MenuBarBg] = Panel;
        c[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
        c[ImGuiCol_ScrollbarGrab] = Border;
        c[ImGuiCol_ScrollbarGrabHovered] = PanelHover;
        c[ImGuiCol_ScrollbarGrabActive] = Accent;
        c[ImGuiCol_CheckMark] = Accent;
        c[ImGuiCol_SliderGrab] = Accent;
        c[ImGuiCol_SliderGrabActive] = AccentDim;
        c[ImGuiCol_Button] = PanelHover;
        c[ImGuiCol_ButtonHovered] = ImVec4(0.18f, 0.18f, 0.22f, 0.65f);
        c[ImGuiCol_ButtonActive] = Accent;
        c[ImGuiCol_Header] = PanelHover;
        c[ImGuiCol_HeaderHovered] = ImVec4(0.18f, 0.18f, 0.22f, 0.65f);
        c[ImGuiCol_HeaderActive] = Accent;
        c[ImGuiCol_Separator] = Border;
        c[ImGuiCol_ResizeGrip] = ImVec4(0, 0, 0, 0);
        c[ImGuiCol_ResizeGripHovered] = ImVec4(0, 0, 0, 0);
        c[ImGuiCol_ResizeGripActive] = ImVec4(0, 0, 0, 0);
        c[ImGuiCol_PlotLines] = Accent;
        c[ImGuiCol_PlotHistogram] = Accent;
        c[ImGuiCol_TextSelectedBg] = ImVec4(Accent.x, Accent.y, Accent.z, 0.35f);
        c[ImGuiCol_ModalWindowDimBg] = ImVec4(0, 0, 0, 0.4f);
    }
}