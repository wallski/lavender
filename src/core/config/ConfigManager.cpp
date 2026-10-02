#include "pch.h"
#include "ConfigManager.h"
#include "../features/visuals/chams/Chams.h"
#include "../features/visuals/chams/Materials.h"
#include "../features/visuals/removals/Removals.h"
#include "../features/visuals/skybox/Skybox.h"
#include "../features/combat/legit/triggerbot/Triggerbot.h"
#include "../../render/Renderer.h"
#include "../../render/Style.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace ConfigManager {

    static std::string g_directory;
    static std::string g_currentName = "default";

    static std::string GetDllDirectory() {
        HMODULE hm = NULL;
        GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCSTR)&GetDllDirectory, &hm);
        char path[MAX_PATH] = { 0 };
        GetModuleFileNameA(hm, path, MAX_PATH);
        std::string s(path);
        size_t pos = s.find_last_of("\\/");
        return (pos == std::string::npos) ? s : s.substr(0, pos);
    }

    static std::string PathFor(const std::string& name) {
        return g_directory + "\\" + name + ".lav";
    }

    static bool ParseBool(const std::string& s) {
        return s == "1" || s == "true";
    }

    static float ParseFloat(const std::string& s, float fallback) {
        try { return std::stof(s); }
        catch (...) { return fallback; }
    }

    static int ParseInt(const std::string& s, int fallback) {
        try { return std::stoi(s); }
        catch (...) { return fallback; }
    }

    bool Initialize() {
        g_directory = GetDllDirectory() + "\\configs";
        std::error_code ec;
        std::filesystem::create_directories(g_directory, ec);
        return true;
    }

    void Shutdown() {}

    bool Exists(const std::string& name) {
        if (name.empty()) return false;
        std::error_code ec;
        return std::filesystem::exists(PathFor(name), ec);
    }

    std::vector<std::string> List() {
        std::vector<std::string> out;
        std::error_code ec;
        if (!std::filesystem::exists(g_directory, ec)) return out;
        for (const auto& entry : std::filesystem::directory_iterator(g_directory, ec)) {
            if (ec) break;
            if (!entry.is_regular_file(ec)) continue;
            if (entry.path().extension() != ".lav") continue;
            out.push_back(entry.path().stem().string());
        }
        std::sort(out.begin(), out.end());
        return out;
    }

    bool Save(const std::string& name) {
        if (name.empty()) return false;

        std::ofstream f(PathFor(name), std::ios::trunc);
        if (!f.is_open()) return false;

        f << "chams.enabled=" << (Chams::IsEnabled() ? 1 : 0) << "\n";
        f << "chams.material=" << (int)Chams::GetMaterial() << "\n";
        f << "chams.occluded=" << (Chams::IsOccluded() ? 1 : 0) << "\n";
        f << "chams.team_check=" << (Chams::IsTeamCheck() ? 1 : 0) << "\n";
        const float* c = Chams::GetColor();
        f << "chams.color_r=" << c[0] << "\n";
        f << "chams.color_g=" << c[1] << "\n";
        f << "chams.color_b=" << c[2] << "\n";
        f << "chams.color_a=" << c[3] << "\n";

        f << "removals.flash=" << (Removals::IsFlashEnabled() ? 1 : 0) << "\n";

        f << "world.skybox_enabled=" << (Skybox::IsEnabled() ? 1 : 0) << "\n";
        f << "world.skybox_index=" << Skybox::GetSelectedIndex() << "\n";

        f << "trigger.enabled=" << (Triggerbot::IsEnabled() ? 1 : 0) << "\n";
        f << "trigger.key=" << Triggerbot::GetKey() << "\n";
        f << "trigger.team_check=" << (Triggerbot::GetTeamCheck() ? 1 : 0) << "\n";
        f << "trigger.hitboxes=" << (unsigned int)Triggerbot::GetHitboxes() << "\n";
        f << "trigger.delay=" << Triggerbot::GetDelayMs() << "\n";

        f << "menu.key=" << Renderer::GetMenuKey() << "\n";
        f << "menu.accent_r=" << Style::Accent.x << "\n";
        f << "menu.accent_g=" << Style::Accent.y << "\n";
        f << "menu.accent_b=" << Style::Accent.z << "\n";

        g_currentName = name;
        return true;
    }

    bool Load(const std::string& name) {
        std::ifstream f(PathFor(name));
        if (!f.is_open()) return false;

        bool chamsEnabled = false;
        int chamsMaterial = (int)Materials::Id::Flat;
        bool chamsOccluded = false;
        bool chamsTeamCheck = false;
        float chamsColor[4] = { 0.0f, 1.0f, 0.4f, 1.0f };
        bool removalsFlash = false;
        bool skyboxEnabled = false;
        int skyboxIndex = 0;
        bool triggerEnabled = false;
        int triggerKey = VK_XBUTTON2;
        bool triggerTeamCheck = true;
        uint32_t triggerHitboxes = 1;
        int triggerDelay = 60;
        int menuKey = VK_DELETE;
        float accent[3] = { Style::Accent.x, Style::Accent.y, Style::Accent.z };

        std::string line;
        while (std::getline(f, line)) {
            size_t start = line.find_first_not_of(" \t\r\n");
            if (start == std::string::npos) continue;
            size_t end = line.find_last_not_of(" \t\r\n");
            line = line.substr(start, end - start + 1);
            if (line.empty() || line[0] == '#' || line[0] == ';') continue;

            auto eq = line.find('=');
            if (eq == std::string::npos) continue;

            std::string key = line.substr(0, eq);
            std::string val = line.substr(eq + 1);

            if (key == "chams.enabled") chamsEnabled = ParseBool(val);
            else if (key == "chams.material") chamsMaterial = ParseInt(val, chamsMaterial);
            else if (key == "chams.occluded") chamsOccluded = ParseBool(val);
            else if (key == "chams.team_check") chamsTeamCheck = ParseBool(val);
            else if (key == "chams.color_r") chamsColor[0] = ParseFloat(val, chamsColor[0]);
            else if (key == "chams.color_g") chamsColor[1] = ParseFloat(val, chamsColor[1]);
            else if (key == "chams.color_b") chamsColor[2] = ParseFloat(val, chamsColor[2]);
            else if (key == "chams.color_a") chamsColor[3] = ParseFloat(val, chamsColor[3]);
            else if (key == "removals.flash") removalsFlash = ParseBool(val);
            else if (key == "world.skybox_enabled") skyboxEnabled = ParseBool(val);
            else if (key == "world.skybox_index") skyboxIndex = ParseInt(val, skyboxIndex);
            else if (key == "trigger.enabled") triggerEnabled = ParseBool(val);
            else if (key == "trigger.key") triggerKey = ParseInt(val, triggerKey);
            else if (key == "trigger.team_check") triggerTeamCheck = ParseBool(val);
            else if (key == "trigger.hitboxes") triggerHitboxes = (uint32_t)ParseInt(val, (int)triggerHitboxes);
            else if (key == "trigger.delay") triggerDelay = ParseInt(val, triggerDelay);
            else if (key == "menu.key") menuKey = ParseInt(val, menuKey);
            else if (key == "menu.accent_r") accent[0] = ParseFloat(val, accent[0]);
            else if (key == "menu.accent_g") accent[1] = ParseFloat(val, accent[1]);
            else if (key == "menu.accent_b") accent[2] = ParseFloat(val, accent[2]);
        }

        Chams::SetEnabled(chamsEnabled);
        Chams::SetMaterial((Materials::Id)chamsMaterial);
        Chams::SetOccluded(chamsOccluded);
        Chams::SetTeamCheck(chamsTeamCheck);
        Chams::SetColor(chamsColor);
        Removals::SetFlashEnabled(removalsFlash);
        Skybox::SetEnabled(skyboxEnabled);
        Skybox::SetSelectedIndex(skyboxIndex);
        Triggerbot::SetEnabled(triggerEnabled);
        Triggerbot::SetKey(triggerKey);
        Triggerbot::SetTeamCheck(triggerTeamCheck);
        Triggerbot::SetHitboxes(triggerHitboxes);
        Triggerbot::SetDelayMs(triggerDelay);
        Renderer::SetMenuKey(menuKey);
        Style::SetAccent(accent[0], accent[1], accent[2]);
        Style::Apply();

        g_currentName = name;
        return true;
    }

    bool Delete(const std::string& name) {
        if (name.empty()) return false;
        std::error_code ec;
        return std::filesystem::remove(PathFor(name), ec);
    }

    const std::string& GetCurrentName() { return g_currentName; }
    const std::string& GetDirectory() { return g_directory; }
}