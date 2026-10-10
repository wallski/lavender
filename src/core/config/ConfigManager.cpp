#include "pch.h"
#include "ConfigManager.h"
#include "../features/visuals/chams/Chams.h"
#include "../features/visuals/chams/Materials.h"
#include "../features/visuals/esp/Esp.h"
#include "../features/visuals/removals/Removals.h"
#include "../features/visuals/skybox/Skybox.h"
#include "../features/visuals/bombtimer/BombTimer.h"
#include "../features/combat/legit/triggerbot/Triggerbot.h"
#include "../features/combat/legit/aim/Aim.h"
#include "../features/combat/rage/Rage.h"
#include "../../render/Renderer.h"
#include "../../render/Style.h"
#include <ShlObj.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>

#pragma comment(lib, "shell32.lib")

namespace ConfigManager {

    static std::string g_directory;
    static std::string g_currentName = "default";

    static std::string GetDocumentsPath() {
        PWSTR widePath = nullptr;
        HRESULT hr = SHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_DEFAULT, nullptr, &widePath);

        if (SUCCEEDED(hr) && widePath) {
            char narrow[MAX_PATH] = {};
            WideCharToMultiByte(CP_UTF8, 0, widePath, -1, narrow, MAX_PATH, nullptr, nullptr);
            CoTaskMemFree(widePath);
            if (narrow[0] != '\0') {
                return std::string(narrow);
            }
        }

        char userProfile[MAX_PATH] = {};
        DWORD len = GetEnvironmentVariableA("USERPROFILE", userProfile, MAX_PATH);
        if (len > 0 && len < MAX_PATH) {
            return std::string(userProfile) + "\\Documents";
        }

        return ".";
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
        const std::string docs = GetDocumentsPath();
        g_directory = docs + "\\lavender\\configs";

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

        f << "esp.enabled=" << (Esp::IsEnabled() ? 1 : 0) << "\n";
        f << "esp.team_check=" << (Esp::IsTeamCheck() ? 1 : 0) << "\n";
        f << "esp.box_enabled=" << (Esp::IsBoxEnabled() ? 1 : 0) << "\n";
        f << "esp.box_style=" << Esp::GetBoxStyle() << "\n";
        f << "esp.box_fill=" << (Esp::IsBoxFillEnabled() ? 1 : 0) << "\n";
        f << "esp.box_outline=" << (Esp::IsBoxOutlineEnabled() ? 1 : 0) << "\n";
        f << "esp.box_corner_length=" << Esp::GetBoxCornerLength() << "\n";
        const float* vc = Esp::GetBoxVisibleColor();
        f << "esp.box_visible_r=" << vc[0] << "\n";
        f << "esp.box_visible_g=" << vc[1] << "\n";
        f << "esp.box_visible_b=" << vc[2] << "\n";
        f << "esp.box_visible_a=" << vc[3] << "\n";
        const float* oc = Esp::GetBoxOccludedColor();
        f << "esp.box_occluded_r=" << oc[0] << "\n";
        f << "esp.box_occluded_g=" << oc[1] << "\n";
        f << "esp.box_occluded_b=" << oc[2] << "\n";
        f << "esp.box_occluded_a=" << oc[3] << "\n";
        const float* fc = Esp::GetBoxFillColor();
        f << "esp.box_fill_r=" << fc[0] << "\n";
        f << "esp.box_fill_g=" << fc[1] << "\n";
        f << "esp.box_fill_b=" << fc[2] << "\n";
        f << "esp.box_fill_a=" << fc[3] << "\n";

        f << "esp.health_enabled=" << (Esp::IsHealthEnabled() ? 1 : 0) << "\n";
        f << "esp.health_position=" << Esp::GetHealthPosition() << "\n";
        f << "esp.health_style=" << Esp::GetHealthStyle() << "\n";
        const float* hc = Esp::GetHealthColor();
        f << "esp.health_color_r=" << hc[0] << "\n";
        f << "esp.health_color_g=" << hc[1] << "\n";
        f << "esp.health_color_b=" << hc[2] << "\n";
        f << "esp.health_color_a=" << hc[3] << "\n";
        const float* hl = Esp::GetHealthLowColor();
        f << "esp.health_low_r=" << hl[0] << "\n";
        f << "esp.health_low_g=" << hl[1] << "\n";
        f << "esp.health_low_b=" << hl[2] << "\n";
        f << "esp.health_low_a=" << hl[3] << "\n";
        const float* hh = Esp::GetHealthHighColor();
        f << "esp.health_high_r=" << hh[0] << "\n";
        f << "esp.health_high_g=" << hh[1] << "\n";
        f << "esp.health_high_b=" << hh[2] << "\n";
        f << "esp.health_high_a=" << hh[3] << "\n";
        f << "esp.health_show_value=" << (Esp::IsHealthShowValue() ? 1 : 0) << "\n";
        f << "esp.health_thickness=" << Esp::GetHealthThickness() << "\n";

        f << "esp.ammo_enabled=" << (Esp::IsAmmoEnabled() ? 1 : 0) << "\n";
        f << "esp.ammo_position=" << Esp::GetAmmoPosition() << "\n";
        const float* ac = Esp::GetAmmoColor();
        f << "esp.ammo_color_r=" << ac[0] << "\n";
        f << "esp.ammo_color_g=" << ac[1] << "\n";
        f << "esp.ammo_color_b=" << ac[2] << "\n";
        f << "esp.ammo_color_a=" << ac[3] << "\n";
        f << "esp.ammo_show_value=" << (Esp::IsAmmoShowValue() ? 1 : 0) << "\n";
        f << "esp.ammo_thickness=" << Esp::GetAmmoThickness() << "\n";

        f << "esp.name_enabled=" << (Esp::IsNameEnabled() ? 1 : 0) << "\n";
        f << "esp.name_position=" << Esp::GetNamePosition() << "\n";
        const float* nc = Esp::GetNameColor();
        f << "esp.name_color_r=" << nc[0] << "\n";
        f << "esp.name_color_g=" << nc[1] << "\n";
        f << "esp.name_color_b=" << nc[2] << "\n";
        f << "esp.name_color_a=" << nc[3] << "\n";
        const float* noc = Esp::GetNameOccludedColor();
        f << "esp.name_occluded_r=" << noc[0] << "\n";
        f << "esp.name_occluded_g=" << noc[1] << "\n";
        f << "esp.name_occluded_b=" << noc[2] << "\n";
        f << "esp.name_occluded_a=" << noc[3] << "\n";

        f << "esp.skeleton_enabled=" << (Esp::IsSkeletonEnabled() ? 1 : 0) << "\n";
        const float* sc = Esp::GetSkeletonColor();
        f << "esp.skeleton_color_r=" << sc[0] << "\n";
        f << "esp.skeleton_color_g=" << sc[1] << "\n";
        f << "esp.skeleton_color_b=" << sc[2] << "\n";
        f << "esp.skeleton_color_a=" << sc[3] << "\n";
        const float* soc = Esp::GetSkeletonOccludedColor();
        f << "esp.skeleton_occluded_r=" << soc[0] << "\n";
        f << "esp.skeleton_occluded_g=" << soc[1] << "\n";
        f << "esp.skeleton_occluded_b=" << soc[2] << "\n";
        f << "esp.skeleton_occluded_a=" << soc[3] << "\n";
        f << "esp.skeleton_thickness=" << Esp::GetSkeletonThickness() << "\n";
        f << "esp.skeleton_debug=" << (Esp::IsSkeletonDebug() ? 1 : 0) << "\n";

        f << "esp.fov_circle_enabled=" << (Esp::IsFovCircleEnabled() ? 1 : 0) << "\n";
        const float* fcc = Esp::GetFovCircleColor();
        f << "esp.fov_circle_color_r=" << fcc[0] << "\n";
        f << "esp.fov_circle_color_g=" << fcc[1] << "\n";
        f << "esp.fov_circle_color_b=" << fcc[2] << "\n";
        f << "esp.fov_circle_color_a=" << fcc[3] << "\n";

        f << "removals.flash=" << (Removals::IsFlashEnabled() ? 1 : 0) << "\n";
        f << "removals.smoke=" << (Removals::IsSmokeEnabled() ? 1 : 0) << "\n";

        f << "bombtimer.enabled=" << (BombTimer::IsEnabled() ? 1 : 0) << "\n";
        f << "bombtimer.defuse=" << (BombTimer::IsDefuseEnabled() ? 1 : 0) << "\n";
        f << "bombtimer.site=" << (BombTimer::IsSiteEnabled() ? 1 : 0) << "\n";
        f << "bombtimer.scale=" << BombTimer::GetScale() << "\n";
        const float* btc = BombTimer::GetColor();
        f << "bombtimer.color_r=" << btc[0] << "\n";
        f << "bombtimer.color_g=" << btc[1] << "\n";
        f << "bombtimer.color_b=" << btc[2] << "\n";
        f << "bombtimer.color_a=" << btc[3] << "\n";
        const float* btdc = BombTimer::GetDefuseColor();
        f << "bombtimer.defuse_color_r=" << btdc[0] << "\n";
        f << "bombtimer.defuse_color_g=" << btdc[1] << "\n";
        f << "bombtimer.defuse_color_b=" << btdc[2] << "\n";
        f << "bombtimer.defuse_color_a=" << btdc[3] << "\n";

        f << "world.skybox_enabled=" << (Skybox::IsEnabled() ? 1 : 0) << "\n";
        f << "world.skybox_index=" << Skybox::GetSelectedIndex() << "\n";

        f << "trigger.enabled=" << (Triggerbot::IsEnabled() ? 1 : 0) << "\n";
        f << "trigger.key=" << Triggerbot::GetKey() << "\n";
        f << "trigger.team_check=" << (Triggerbot::GetTeamCheck() ? 1 : 0) << "\n";
        f << "trigger.hitboxes=" << (unsigned int)Triggerbot::GetHitboxes() << "\n";
        f << "trigger.delay=" << Triggerbot::GetDelayMs() << "\n";

        f << "aim.enabled=" << (Aim::IsEnabled() ? 1 : 0) << "\n";
        f << "aim.key=" << Aim::GetKey() << "\n";
        f << "aim.team_check=" << (Aim::IsTeamCheck() ? 1 : 0) << "\n";
        f << "aim.hitboxes=" << (unsigned int)Aim::GetHitboxes() << "\n";
        f << "aim.fov=" << Aim::GetFov() << "\n";
        f << "aim.smooth=" << Aim::GetSmooth() << "\n";
        f << "aim.on_accuracy=" << (Aim::IsOnAccuracy() ? 1 : 0) << "\n";
        f << "aim.accuracy_speed=" << Aim::GetAccuracySpeed() << "\n";

        f << "rage.enabled=" << (Rage::IsEnabled() ? 1 : 0) << "\n";
        f << "rage.auto_shoot=" << (Rage::IsAutoShoot() ? 1 : 0) << "\n";
        f << "rage.team_check=" << (Rage::IsTeamCheck() ? 1 : 0) << "\n";
        f << "rage.hitboxes=" << (unsigned int)Rage::GetHitboxes() << "\n";
        f << "rage.fov=" << Rage::GetFov() << "\n";
        f << "rage.min_damage=" << Rage::GetMinDamage() << "\n";
        f << "rage.silent=" << (Rage::IsSilent() ? 1 : 0) << "\n";
        f << "rage.no_spread=" << (Rage::IsNoSpread() ? 1 : 0) << "\n";
        f << "rage.auto_scope=" << (Rage::IsAutoScope() ? 1 : 0) << "\n";

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

        bool espEnabled = true;
        bool espTeamCheck = true;
        bool espBoxEnabled = true;
        int espBoxStyle = 1;
        bool espBoxFill = false;
        bool espBoxOutline = true;
        float espBoxCornerLength = 8.0f;
        float espBoxVisible[4] = { 0.659f, 0.333f, 0.969f, 1.00f };
        float espBoxOccluded[4] = { 1.000f, 0.400f, 0.400f, 1.00f };
        float espBoxFillColor[4] = { 0.659f, 0.333f, 0.969f, 0.25f };

        bool espHealthEnabled = false;
        int espHealthPosition = 0;
        int espHealthStyle = 0;
        float espHealthColor[4] = { 0.659f, 0.333f, 0.969f, 1.00f };
        float espHealthLow[4] = { 1.00f, 0.25f, 0.25f, 1.00f };
        float espHealthHigh[4] = { 0.30f, 1.00f, 0.30f, 1.00f };
        bool espHealthShowValue = true;
        float espHealthThickness = 3.0f;

        bool espAmmoEnabled = false;
        int espAmmoPosition = 2;
        float espAmmoColor[4] = { 1.00f, 1.00f, 1.00f, 1.00f };
        bool espAmmoShowValue = true;
        float espAmmoThickness = 3.0f;

        bool espNameEnabled = false;
        int espNamePosition = 0;
        float espNameColor[4] = { 1.00f, 1.00f, 1.00f, 1.00f };
        float espNameOccluded[4] = { 0.850f, 0.500f, 0.500f, 1.00f };

        bool espSkeletonEnabled = false;
        float espSkeletonColor[4] = { 0.659f, 0.333f, 0.969f, 1.00f };
        float espSkeletonOccluded[4] = { 1.000f, 0.400f, 0.400f, 1.00f };
        float espSkeletonThickness = 1.5f;
        bool espSkeletonDebug = false;

        bool espFovCircleEnabled = false;
        float espFovCircleColor[4] = { 0.659f, 0.333f, 0.969f, 0.85f };

        bool removalsFlash = false;
        bool removalsSmoke = false;

        bool bombTimerEnabled = false;
        bool bombTimerDefuse = true;
        bool bombTimerSite = true;
        float bombTimerScale = 1.0f;
        float bombTimerColor[4] = { 1.0f, 0.25f, 0.25f, 1.0f };
        float bombTimerDefuseColor[4] = { 0.25f, 0.85f, 0.35f, 1.0f };

        bool skyboxEnabled = false;
        int skyboxIndex = 0;

        bool triggerEnabled = false;
        int triggerKey = VK_XBUTTON2;
        bool triggerTeamCheck = true;
        uint32_t triggerHitboxes = 1;
        int triggerDelay = 60;

        bool aimEnabled = false;
        int aimKey = VK_XBUTTON1;
        bool aimTeamCheck = true;
        uint32_t aimHitboxes = 1;
        float aimFov = 5.0f;
        int aimSmooth = 3;
        bool aimOnAccuracy = false;
        float aimAccuracySpeed = 5.0f;

        bool rageEnabled = false;
        bool rageAutoShoot = true;
        bool rageTeamCheck = true;
        uint32_t rageHitboxes = 0x1F;
        float rageFov = 15.0f;
        int rageMinDamage = 1;
        bool rageSilent = true;
        bool rageNoSpread = false;
        bool rageAutoScope = true;

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

            else if (key == "esp.enabled") espEnabled = ParseBool(val);
            else if (key == "esp.team_check") espTeamCheck = ParseBool(val);
            else if (key == "esp.box_enabled") espBoxEnabled = ParseBool(val);
            else if (key == "esp.box_style") espBoxStyle = ParseInt(val, espBoxStyle);
            else if (key == "esp.box_fill") espBoxFill = ParseBool(val);
            else if (key == "esp.box_outline") espBoxOutline = ParseBool(val);
            else if (key == "esp.box_corner_length") espBoxCornerLength = ParseFloat(val, espBoxCornerLength);
            else if (key == "esp.box_visible_r") espBoxVisible[0] = ParseFloat(val, espBoxVisible[0]);
            else if (key == "esp.box_visible_g") espBoxVisible[1] = ParseFloat(val, espBoxVisible[1]);
            else if (key == "esp.box_visible_b") espBoxVisible[2] = ParseFloat(val, espBoxVisible[2]);
            else if (key == "esp.box_visible_a") espBoxVisible[3] = ParseFloat(val, espBoxVisible[3]);
            else if (key == "esp.box_occluded_r") espBoxOccluded[0] = ParseFloat(val, espBoxOccluded[0]);
            else if (key == "esp.box_occluded_g") espBoxOccluded[1] = ParseFloat(val, espBoxOccluded[1]);
            else if (key == "esp.box_occluded_b") espBoxOccluded[2] = ParseFloat(val, espBoxOccluded[2]);
            else if (key == "esp.box_occluded_a") espBoxOccluded[3] = ParseFloat(val, espBoxOccluded[3]);
            else if (key == "esp.box_fill_r") espBoxFillColor[0] = ParseFloat(val, espBoxFillColor[0]);
            else if (key == "esp.box_fill_g") espBoxFillColor[1] = ParseFloat(val, espBoxFillColor[1]);
            else if (key == "esp.box_fill_b") espBoxFillColor[2] = ParseFloat(val, espBoxFillColor[2]);
            else if (key == "esp.box_fill_a") espBoxFillColor[3] = ParseFloat(val, espBoxFillColor[3]);

            else if (key == "esp.health_enabled") espHealthEnabled = ParseBool(val);
            else if (key == "esp.health_position") espHealthPosition = ParseInt(val, espHealthPosition);
            else if (key == "esp.health_style") espHealthStyle = ParseInt(val, espHealthStyle);
            else if (key == "esp.health_color_r") espHealthColor[0] = ParseFloat(val, espHealthColor[0]);
            else if (key == "esp.health_color_g") espHealthColor[1] = ParseFloat(val, espHealthColor[1]);
            else if (key == "esp.health_color_b") espHealthColor[2] = ParseFloat(val, espHealthColor[2]);
            else if (key == "esp.health_color_a") espHealthColor[3] = ParseFloat(val, espHealthColor[3]);
            else if (key == "esp.health_low_r") espHealthLow[0] = ParseFloat(val, espHealthLow[0]);
            else if (key == "esp.health_low_g") espHealthLow[1] = ParseFloat(val, espHealthLow[1]);
            else if (key == "esp.health_low_b") espHealthLow[2] = ParseFloat(val, espHealthLow[2]);
            else if (key == "esp.health_low_a") espHealthLow[3] = ParseFloat(val, espHealthLow[3]);
            else if (key == "esp.health_high_r") espHealthHigh[0] = ParseFloat(val, espHealthHigh[0]);
            else if (key == "esp.health_high_g") espHealthHigh[1] = ParseFloat(val, espHealthHigh[1]);
            else if (key == "esp.health_high_b") espHealthHigh[2] = ParseFloat(val, espHealthHigh[2]);
            else if (key == "esp.health_high_a") espHealthHigh[3] = ParseFloat(val, espHealthHigh[3]);
            else if (key == "esp.health_show_value") espHealthShowValue = ParseBool(val);
            else if (key == "esp.health_thickness") espHealthThickness = ParseFloat(val, espHealthThickness);

            else if (key == "esp.ammo_enabled") espAmmoEnabled = ParseBool(val);
            else if (key == "esp.ammo_position") espAmmoPosition = ParseInt(val, espAmmoPosition);
            else if (key == "esp.ammo_color_r") espAmmoColor[0] = ParseFloat(val, espAmmoColor[0]);
            else if (key == "esp.ammo_color_g") espAmmoColor[1] = ParseFloat(val, espAmmoColor[1]);
            else if (key == "esp.ammo_color_b") espAmmoColor[2] = ParseFloat(val, espAmmoColor[2]);
            else if (key == "esp.ammo_color_a") espAmmoColor[3] = ParseFloat(val, espAmmoColor[3]);
            else if (key == "esp.ammo_show_value") espAmmoShowValue = ParseBool(val);
            else if (key == "esp.ammo_thickness") espAmmoThickness = ParseFloat(val, espAmmoThickness);

            else if (key == "esp.name_enabled") espNameEnabled = ParseBool(val);
            else if (key == "esp.name_position") espNamePosition = ParseInt(val, espNamePosition);
            else if (key == "esp.name_color_r") espNameColor[0] = ParseFloat(val, espNameColor[0]);
            else if (key == "esp.name_color_g") espNameColor[1] = ParseFloat(val, espNameColor[1]);
            else if (key == "esp.name_color_b") espNameColor[2] = ParseFloat(val, espNameColor[2]);
            else if (key == "esp.name_color_a") espNameColor[3] = ParseFloat(val, espNameColor[3]);
            else if (key == "esp.name_occluded_r") espNameOccluded[0] = ParseFloat(val, espNameOccluded[0]);
            else if (key == "esp.name_occluded_g") espNameOccluded[1] = ParseFloat(val, espNameOccluded[1]);
            else if (key == "esp.name_occluded_b") espNameOccluded[2] = ParseFloat(val, espNameOccluded[2]);
            else if (key == "esp.name_occluded_a") espNameOccluded[3] = ParseFloat(val, espNameOccluded[3]);

            else if (key == "esp.skeleton_enabled") espSkeletonEnabled = ParseBool(val);
            else if (key == "esp.skeleton_color_r") espSkeletonColor[0] = ParseFloat(val, espSkeletonColor[0]);
            else if (key == "esp.skeleton_color_g") espSkeletonColor[1] = ParseFloat(val, espSkeletonColor[1]);
            else if (key == "esp.skeleton_color_b") espSkeletonColor[2] = ParseFloat(val, espSkeletonColor[2]);
            else if (key == "esp.skeleton_color_a") espSkeletonColor[3] = ParseFloat(val, espSkeletonColor[3]);
            else if (key == "esp.skeleton_occluded_r") espSkeletonOccluded[0] = ParseFloat(val, espSkeletonOccluded[0]);
            else if (key == "esp.skeleton_occluded_g") espSkeletonOccluded[1] = ParseFloat(val, espSkeletonOccluded[1]);
            else if (key == "esp.skeleton_occluded_b") espSkeletonOccluded[2] = ParseFloat(val, espSkeletonOccluded[2]);
            else if (key == "esp.skeleton_occluded_a") espSkeletonOccluded[3] = ParseFloat(val, espSkeletonOccluded[3]);
            else if (key == "esp.skeleton_thickness") espSkeletonThickness = ParseFloat(val, espSkeletonThickness);
            else if (key == "esp.skeleton_debug") espSkeletonDebug = ParseBool(val);

            else if (key == "esp.fov_circle_enabled") espFovCircleEnabled = ParseBool(val);
            else if (key == "esp.fov_circle_color_r") espFovCircleColor[0] = ParseFloat(val, espFovCircleColor[0]);
            else if (key == "esp.fov_circle_color_g") espFovCircleColor[1] = ParseFloat(val, espFovCircleColor[1]);
            else if (key == "esp.fov_circle_color_b") espFovCircleColor[2] = ParseFloat(val, espFovCircleColor[2]);
            else if (key == "esp.fov_circle_color_a") espFovCircleColor[3] = ParseFloat(val, espFovCircleColor[3]);

            else if (key == "removals.flash") removalsFlash = ParseBool(val);
            else if (key == "removals.smoke") removalsSmoke = ParseBool(val);

            else if (key == "bombtimer.enabled") bombTimerEnabled = ParseBool(val);
            else if (key == "bombtimer.defuse") bombTimerDefuse = ParseBool(val);
            else if (key == "bombtimer.site") bombTimerSite = ParseBool(val);
            else if (key == "bombtimer.scale") bombTimerScale = ParseFloat(val, bombTimerScale);
            else if (key == "bombtimer.color_r") bombTimerColor[0] = ParseFloat(val, bombTimerColor[0]);
            else if (key == "bombtimer.color_g") bombTimerColor[1] = ParseFloat(val, bombTimerColor[1]);
            else if (key == "bombtimer.color_b") bombTimerColor[2] = ParseFloat(val, bombTimerColor[2]);
            else if (key == "bombtimer.color_a") bombTimerColor[3] = ParseFloat(val, bombTimerColor[3]);
            else if (key == "bombtimer.defuse_color_r") bombTimerDefuseColor[0] = ParseFloat(val, bombTimerDefuseColor[0]);
            else if (key == "bombtimer.defuse_color_g") bombTimerDefuseColor[1] = ParseFloat(val, bombTimerDefuseColor[1]);
            else if (key == "bombtimer.defuse_color_b") bombTimerDefuseColor[2] = ParseFloat(val, bombTimerDefuseColor[2]);
            else if (key == "bombtimer.defuse_color_a") bombTimerDefuseColor[3] = ParseFloat(val, bombTimerDefuseColor[3]);

            else if (key == "world.skybox_enabled") skyboxEnabled = ParseBool(val);
            else if (key == "world.skybox_index") skyboxIndex = ParseInt(val, skyboxIndex);

            else if (key == "trigger.enabled") triggerEnabled = ParseBool(val);
            else if (key == "trigger.key") triggerKey = ParseInt(val, triggerKey);
            else if (key == "trigger.team_check") triggerTeamCheck = ParseBool(val);
            else if (key == "trigger.hitboxes") triggerHitboxes = (uint32_t)ParseInt(val, (int)triggerHitboxes);
            else if (key == "trigger.delay") triggerDelay = ParseInt(val, triggerDelay);

            else if (key == "aim.enabled") aimEnabled = ParseBool(val);
            else if (key == "aim.key") aimKey = ParseInt(val, aimKey);
            else if (key == "aim.team_check") aimTeamCheck = ParseBool(val);
            else if (key == "aim.hitboxes") aimHitboxes = (uint32_t)ParseInt(val, (int)aimHitboxes);
            else if (key == "aim.fov") aimFov = ParseFloat(val, aimFov);
            else if (key == "aim.smooth") aimSmooth = ParseInt(val, aimSmooth);
            else if (key == "aim.on_accuracy") aimOnAccuracy = ParseBool(val);
            else if (key == "aim.accuracy_speed") aimAccuracySpeed = ParseFloat(val, aimAccuracySpeed);

            else if (key == "rage.enabled") rageEnabled = ParseBool(val);
            else if (key == "rage.auto_shoot") rageAutoShoot = ParseBool(val);
            else if (key == "rage.team_check") rageTeamCheck = ParseBool(val);
            else if (key == "rage.hitboxes") rageHitboxes = (uint32_t)ParseInt(val, (int)rageHitboxes);
            else if (key == "rage.fov") rageFov = ParseFloat(val, rageFov);
            else if (key == "rage.min_damage") rageMinDamage = ParseInt(val, rageMinDamage);
            else if (key == "rage.silent") rageSilent = ParseBool(val);
            else if (key == "rage.no_spread") rageNoSpread = ParseBool(val);
            else if (key == "rage.auto_scope") rageAutoScope = ParseBool(val);

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

        Esp::SetEnabled(espEnabled);
        Esp::SetTeamCheck(espTeamCheck);
        Esp::SetBoxEnabled(espBoxEnabled);
        Esp::SetBoxStyle(espBoxStyle);
        Esp::SetBoxFillEnabled(espBoxFill);
        Esp::SetBoxOutlineEnabled(espBoxOutline);
        Esp::SetBoxCornerLength(espBoxCornerLength);
        Esp::SetBoxVisibleColor(espBoxVisible);
        Esp::SetBoxOccludedColor(espBoxOccluded);
        Esp::SetBoxFillColor(espBoxFillColor);

        Esp::SetHealthEnabled(espHealthEnabled);
        Esp::SetHealthPosition(espHealthPosition);
        Esp::SetHealthStyle(espHealthStyle);
        Esp::SetHealthColor(espHealthColor);
        Esp::SetHealthLowColor(espHealthLow);
        Esp::SetHealthHighColor(espHealthHigh);
        Esp::SetHealthShowValue(espHealthShowValue);
        Esp::SetHealthThickness(espHealthThickness);

        Esp::SetAmmoEnabled(espAmmoEnabled);
        Esp::SetAmmoPosition(espAmmoPosition);
        Esp::SetAmmoColor(espAmmoColor);
        Esp::SetAmmoShowValue(espAmmoShowValue);
        Esp::SetAmmoThickness(espAmmoThickness);

        Esp::SetNameEnabled(espNameEnabled);
        Esp::SetNamePosition(espNamePosition);
        Esp::SetNameColor(espNameColor);
        Esp::SetNameOccludedColor(espNameOccluded);

        Esp::SetSkeletonEnabled(espSkeletonEnabled);
        Esp::SetSkeletonColor(espSkeletonColor);
        Esp::SetSkeletonOccludedColor(espSkeletonOccluded);
        Esp::SetSkeletonThickness(espSkeletonThickness);
        Esp::SetSkeletonDebug(espSkeletonDebug);

        Esp::SetFovCircleEnabled(espFovCircleEnabled);
        Esp::SetFovCircleColor(espFovCircleColor);

        Removals::SetFlashEnabled(removalsFlash);
        Removals::SetSmokeEnabled(removalsSmoke);

        BombTimer::SetEnabled(bombTimerEnabled);
        BombTimer::SetDefuseEnabled(bombTimerDefuse);
        BombTimer::SetSiteEnabled(bombTimerSite);
        BombTimer::SetScale(bombTimerScale);
        BombTimer::SetColor(bombTimerColor);
        BombTimer::SetDefuseColor(bombTimerDefuseColor);

        Skybox::SetEnabled(skyboxEnabled);
        Skybox::SetSelectedIndex(skyboxIndex);

        Triggerbot::SetEnabled(triggerEnabled);
        Triggerbot::SetKey(triggerKey);
        Triggerbot::SetTeamCheck(triggerTeamCheck);
        Triggerbot::SetHitboxes(triggerHitboxes);
        Triggerbot::SetDelayMs(triggerDelay);

        Aim::SetEnabled(aimEnabled);
        Aim::SetKey(aimKey);
        Aim::SetTeamCheck(aimTeamCheck);
        Aim::SetHitboxes(aimHitboxes);
        Aim::SetFov(aimFov);
        Aim::SetSmooth(aimSmooth);
        Aim::SetOnAccuracy(aimOnAccuracy);
        Aim::SetAccuracySpeed(aimAccuracySpeed);

        Rage::SetEnabled(rageEnabled);
        Rage::SetAutoShoot(rageAutoShoot);
        Rage::SetTeamCheck(rageTeamCheck);
        Rage::SetHitboxes(rageHitboxes);
        Rage::SetFov(rageFov);
        Rage::SetMinDamage(rageMinDamage);
        Rage::SetSilent(rageSilent);
        Rage::SetNoSpread(rageNoSpread);
        Rage::SetAutoScope(rageAutoScope);

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