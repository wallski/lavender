#include "pch.h"
#include "Style.h"

namespace Style {
    ImVec4 Bg = ImVec4(0.055f, 0.055f, 0.065f, 0.96f);
    ImVec4 Panel = ImVec4(0.075f, 0.075f, 0.088f, 0.72f);
    ImVec4 PanelHover = ImVec4(0.105f, 0.105f, 0.122f, 0.85f);
    ImVec4 Border = ImVec4(0.140f, 0.140f, 0.165f, 0.55f);
    ImVec4 Text = ImVec4(0.925f, 0.925f, 0.945f, 1.00f);
    ImVec4 TextMuted = ImVec4(0.545f, 0.545f, 0.600f, 0.95f);
    ImVec4 TextDim = ImVec4(0.380f, 0.380f, 0.430f, 0.90f);
    ImVec4 Accent = ImVec4(0.659f, 0.333f, 0.969f, 1.00f);
    ImVec4 AccentDim = ImVec4(0.514f, 0.260f, 0.756f, 1.00f);

    static ImFont* g_fontRegular = nullptr;
    static ImFont* g_fontMedium = nullptr;

    ImFont* Regular() { return g_fontRegular; }
    ImFont* Medium() { return g_fontMedium; }

    void SetAccent(float r, float g, float b) {
        Accent = ImVec4(r, g, b, 1.0f);
        AccentDim = ImVec4(r * 0.78f, g * 0.78f, b * 0.78f, 1.0f);
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
        }
        if (!mediumPath.empty()) {
            g_fontMedium = io.Fonts->AddFontFromFileTTF(mediumPath.c_str(), 15.0f, &cfg);
        }

        if (!g_fontRegular) {
            const char* segoe = "C:\\Windows\\Fonts\\segoeui.ttf";
            std::ifstream test(segoe);
            if (test.is_open()) {
                g_fontRegular = io.Fonts->AddFontFromFileTTF(segoe, 15.0f, &cfg);
            }
        }
        if (!g_fontRegular) {
            g_fontRegular = io.Fonts->AddFontDefault();
        }
        if (!g_fontMedium) g_fontMedium = g_fontRegular;
    }

    void Apply() {
        ImGuiStyle& s = ImGui::GetStyle();

        s.WindowPadding = ImVec2(Padding, Padding);
        s.FramePadding = ImVec2(10, 5);
        s.ItemSpacing = ImVec2(8, 4);
        s.ItemInnerSpacing = ImVec2(6, 5);
        s.ScrollbarSize = 6.0f;
        s.GrabMinSize = 8.0f;

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
        c[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
        c[ImGuiCol_PopupBg] = ImVec4(0.085f, 0.085f, 0.100f, 0.98f);
        c[ImGuiCol_Border] = Border;
        c[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
        c[ImGuiCol_FrameBg] = ImVec4(0.110f, 0.110f, 0.130f, 1.0f);
        c[ImGuiCol_FrameBgHovered] = ImVec4(0.140f, 0.140f, 0.165f, 1.0f);
        c[ImGuiCol_FrameBgActive] = ImVec4(0.165f, 0.165f, 0.195f, 1.0f);
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
        c[ImGuiCol_Button] = ImVec4(0.110f, 0.110f, 0.130f, 1.0f);
        c[ImGuiCol_ButtonHovered] = ImVec4(0.150f, 0.150f, 0.180f, 1.0f);
        c[ImGuiCol_ButtonActive] = Accent;
        c[ImGuiCol_Header] = PanelHover;
        c[ImGuiCol_HeaderHovered] = ImVec4(0.150f, 0.150f, 0.180f, 1.0f);
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