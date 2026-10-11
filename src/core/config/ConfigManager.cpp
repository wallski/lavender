#include "pch.h"
#include "ConfigManager.h"
#include "../features/visuals/chams/Chams.h"
#include "../features/visuals/chams/Materials.h"
#include "../features/visuals/esp/Esp.h"
#include "../features/visuals/removals/Removals.h"
#include "../features/visuals/skybox/Skybox.h"
#include "../features/visuals/bombtimer/BombTimer.h"
#include "../features/visuals/thirdperson/ThirdPerson.h"
#include "../features/visuals/backtrack/Backtrack.h"
#include "../features/combat/Lagcomp.h"
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

    static const char* kRageGroupKeys[] = { "pistol", "smg", "rifle", "shotgun", "sniper", "lmg" };

    struct RageGroupData {
        bool enabled = false;
        bool autoShoot = true;
        bool silent = true;
        bool noSpread = false;
        bool teamCheck = true;
        bool backtrack = true;
        int minDamage = 1;
        uint32_t hitboxes = 0x1F;
    };

    struct ChamsData {
        bool enabled = false;
        int material = 2;
        bool occluded = false;
        bool teamCheck = false;
        float color[4] = { 0.0f, 1.0f, 0.4f, 1.0f };
    };

    struct EspData {
        bool enabled = true;
        bool teamCheck = true;
        bool boxEnabled = true;
        int boxStyle = 1;
        bool boxFill = false;
        bool boxOutline = true;
        float boxCornerLength = 8.0f;
        float boxVisible[4] = { 0.659f, 0.333f, 0.969f, 1.00f };
        float boxOccluded[4] = { 1.000f, 0.400f, 0.400f, 1.00f };
        float boxFillColor[4] = { 0.659f, 0.333f, 0.969f, 0.25f };

        bool healthEnabled = false;
        int healthPosition = 0;
        int healthStyle = 0;
        float healthColor[4] = { 0.659f, 0.333f, 0.969f, 1.00f };
        float healthLow[4] = { 1.00f, 0.25f, 0.25f, 1.00f };
        float healthHigh[4] = { 0.30f, 1.00f, 0.30f, 1.00f };
        bool healthShowValue = true;
        float healthThickness = 3.0f;

        bool ammoEnabled = false;
        int ammoPosition = 2;
        float ammoColor[4] = { 1.00f, 1.00f, 1.00f, 1.00f };
        bool ammoShowValue = true;
        float ammoThickness = 3.0f;

        bool nameEnabled = false;
        int namePosition = 0;
        float nameColor[4] = { 1.00f, 1.00f, 1.00f, 1.00f };
        float nameOccluded[4] = { 0.850f, 0.500f, 0.500f, 1.00f };

        bool skeletonEnabled = false;
        float skeletonColor[4] = { 0.659f, 0.333f, 0.969f, 1.00f };
        float skeletonOccluded[4] = { 1.000f, 0.400f, 0.400f, 1.00f };
        float skeletonThickness = 1.5f;
        bool skeletonDebug = false;

        bool fovCircleEnabled = false;
        float fovCircleColor[4] = { 0.659f, 0.333f, 0.969f, 0.85f };
    };

    struct BacktrackData {
        bool enabled = false;
        bool teamCheck = true;
        float visibleColor[4] = { 0.659f, 0.333f, 0.969f, 1.00f };
        float occludedColor[4] = { 1.000f, 0.400f, 0.400f, 1.00f };
        float thickness = 1.5f;
    };

    struct MiscData {
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

        bool thirdPersonEnabled = false;
        int thirdPersonKey = VK_MBUTTON;
        float thirdPersonDistance = 85.0f;

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

        int menuKey = VK_DELETE;
        float accent[3] = { 0.659f, 0.333f, 0.969f };
    };

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

    static bool TryParseRageKey(const std::string& key, const std::string& val, RageGroupData* groups) {
        for (int i = 0; i < (int)Rage::Group::Count; i++) {
            const std::string prefix = std::string("rage.") + kRageGroupKeys[i] + ".";
            if (key.rfind(prefix, 0) != 0) continue;

            const std::string suffix = key.substr(prefix.size());
            auto& rg = groups[i];

            if (suffix == "enabled") rg.enabled = ParseBool(val);
            else if (suffix == "auto_shoot") rg.autoShoot = ParseBool(val);
            else if (suffix == "silent") rg.silent = ParseBool(val);
            else if (suffix == "no_spread") rg.noSpread = ParseBool(val);
            else if (suffix == "team_check") rg.teamCheck = ParseBool(val);
            else if (suffix == "backtrack") rg.backtrack = ParseBool(val);
            else if (suffix == "min_damage") rg.minDamage = ParseInt(val, rg.minDamage);
            else if (suffix == "hitboxes") rg.hitboxes = (uint32_t)ParseInt(val, (int)rg.hitboxes);
            return true;
        }
        return false;
    }

    static void ParseChamsKey(const std::string& key, const std::string& val, ChamsData& d) {
        if (key == "chams.enabled") d.enabled = ParseBool(val);
        else if (key == "chams.material") d.material = ParseInt(val, d.material);
        else if (key == "chams.occluded") d.occluded = ParseBool(val);
        else if (key == "chams.team_check") d.teamCheck = ParseBool(val);
        else if (key == "chams.color_r") d.color[0] = ParseFloat(val, d.color[0]);
        else if (key == "chams.color_g") d.color[1] = ParseFloat(val, d.color[1]);
        else if (key == "chams.color_b") d.color[2] = ParseFloat(val, d.color[2]);
        else if (key == "chams.color_a") d.color[3] = ParseFloat(val, d.color[3]);
    }

    static void ParseBacktrackKey(const std::string& key, const std::string& val, BacktrackData& d) {
        if (key == "backtrack.enabled") d.enabled = ParseBool(val);
        else if (key == "backtrack.team_check") d.teamCheck = ParseBool(val);
        else if (key == "backtrack.color_r") d.visibleColor[0] = ParseFloat(val, d.visibleColor[0]);
        else if (key == "backtrack.color_g") d.visibleColor[1] = ParseFloat(val, d.visibleColor[1]);
        else if (key == "backtrack.color_b") d.visibleColor[2] = ParseFloat(val, d.visibleColor[2]);
        else if (key == "backtrack.color_a") d.visibleColor[3] = ParseFloat(val, d.visibleColor[3]);
        else if (key == "backtrack.occluded_r") d.occludedColor[0] = ParseFloat(val, d.occludedColor[0]);
        else if (key == "backtrack.occluded_g") d.occludedColor[1] = ParseFloat(val, d.occludedColor[1]);
        else if (key == "backtrack.occluded_b") d.occludedColor[2] = ParseFloat(val, d.occludedColor[2]);
        else if (key == "backtrack.occluded_a") d.occludedColor[3] = ParseFloat(val, d.occludedColor[3]);
        else if (key == "backtrack.thickness") d.thickness = ParseFloat(val, d.thickness);
    }

    static bool ParseEspKey(const std::string& key, const std::string& val, EspData& d) {
        if (key == "esp.enabled") d.enabled = ParseBool(val);
        else if (key == "esp.team_check") d.teamCheck = ParseBool(val);
        else if (key == "esp.box_enabled") d.boxEnabled = ParseBool(val);
        else if (key == "esp.box_style") d.boxStyle = ParseInt(val, d.boxStyle);
        else if (key == "esp.box_fill") d.boxFill = ParseBool(val);
        else if (key == "esp.box_outline") d.boxOutline = ParseBool(val);
        else if (key == "esp.box_corner_length") d.boxCornerLength = ParseFloat(val, d.boxCornerLength);
        else if (key == "esp.box_visible_r") d.boxVisible[0] = ParseFloat(val, d.boxVisible[0]);
        else if (key == "esp.box_visible_g") d.boxVisible[1] = ParseFloat(val, d.boxVisible[1]);
        else if (key == "esp.box_visible_b") d.boxVisible[2] = ParseFloat(val, d.boxVisible[2]);
        else if (key == "esp.box_visible_a") d.boxVisible[3] = ParseFloat(val, d.boxVisible[3]);
        else if (key == "esp.box_occluded_r") d.boxOccluded[0] = ParseFloat(val, d.boxOccluded[0]);
        else if (key == "esp.box_occluded_g") d.boxOccluded[1] = ParseFloat(val, d.boxOccluded[1]);
        else if (key == "esp.box_occluded_b") d.boxOccluded[2] = ParseFloat(val, d.boxOccluded[2]);
        else if (key == "esp.box_occluded_a") d.boxOccluded[3] = ParseFloat(val, d.boxOccluded[3]);
        else if (key == "esp.box_fill_r") d.boxFillColor[0] = ParseFloat(val, d.boxFillColor[0]);
        else if (key == "esp.box_fill_g") d.boxFillColor[1] = ParseFloat(val, d.boxFillColor[1]);
        else if (key == "esp.box_fill_b") d.boxFillColor[2] = ParseFloat(val, d.boxFillColor[2]);
        else if (key == "esp.box_fill_a") d.boxFillColor[3] = ParseFloat(val, d.boxFillColor[3]);

        else if (key == "esp.health_enabled") d.healthEnabled = ParseBool(val);
        else if (key == "esp.health_position") d.healthPosition = ParseInt(val, d.healthPosition);
        else if (key == "esp.health_style") d.healthStyle = ParseInt(val, d.healthStyle);
        else if (key == "esp.health_color_r") d.healthColor[0] = ParseFloat(val, d.healthColor[0]);
        else if (key == "esp.health_color_g") d.healthColor[1] = ParseFloat(val, d.healthColor[1]);
        else if (key == "esp.health_color_b") d.healthColor[2] = ParseFloat(val, d.healthColor[2]);
        else if (key == "esp.health_color_a") d.healthColor[3] = ParseFloat(val, d.healthColor[3]);
        else if (key == "esp.health_low_r") d.healthLow[0] = ParseFloat(val, d.healthLow[0]);
        else if (key == "esp.health_low_g") d.healthLow[1] = ParseFloat(val, d.healthLow[1]);
        else if (key == "esp.health_low_b") d.healthLow[2] = ParseFloat(val, d.healthLow[2]);
        else if (key == "esp.health_low_a") d.healthLow[3] = ParseFloat(val, d.healthLow[3]);
        else if (key == "esp.health_high_r") d.healthHigh[0] = ParseFloat(val, d.healthHigh[0]);
        else if (key == "esp.health_high_g") d.healthHigh[1] = ParseFloat(val, d.healthHigh[1]);
        else if (key == "esp.health_high_b") d.healthHigh[2] = ParseFloat(val, d.healthHigh[2]);
        else if (key == "esp.health_high_a") d.healthHigh[3] = ParseFloat(val, d.healthHigh[3]);
        else if (key == "esp.health_show_value") d.healthShowValue = ParseBool(val);
        else if (key == "esp.health_thickness") d.healthThickness = ParseFloat(val, d.healthThickness);

        else if (key == "esp.ammo_enabled") d.ammoEnabled = ParseBool(val);
        else if (key == "esp.ammo_position") d.ammoPosition = ParseInt(val, d.ammoPosition);
        else if (key == "esp.ammo_color_r") d.ammoColor[0] = ParseFloat(val, d.ammoColor[0]);
        else if (key == "esp.ammo_color_g") d.ammoColor[1] = ParseFloat(val, d.ammoColor[1]);
        else if (key == "esp.ammo_color_b") d.ammoColor[2] = ParseFloat(val, d.ammoColor[2]);
        else if (key == "esp.ammo_color_a") d.ammoColor[3] = ParseFloat(val, d.ammoColor[3]);
        else if (key == "esp.ammo_show_value") d.ammoShowValue = ParseBool(val);
        else if (key == "esp.ammo_thickness") d.ammoThickness = ParseFloat(val, d.ammoThickness);

        else if (key == "esp.name_enabled") d.nameEnabled = ParseBool(val);
        else if (key == "esp.name_position") d.namePosition = ParseInt(val, d.namePosition);
        else if (key == "esp.name_color_r") d.nameColor[0] = ParseFloat(val, d.nameColor[0]);
        else if (key == "esp.name_color_g") d.nameColor[1] = ParseFloat(val, d.nameColor[1]);
        else if (key == "esp.name_color_b") d.nameColor[2] = ParseFloat(val, d.nameColor[2]);
        else if (key == "esp.name_color_a") d.nameColor[3] = ParseFloat(val, d.nameColor[3]);
        else if (key == "esp.name_occluded_r") d.nameOccluded[0] = ParseFloat(val, d.nameOccluded[0]);
        else if (key == "esp.name_occluded_g") d.nameOccluded[1] = ParseFloat(val, d.nameOccluded[1]);
        else if (key == "esp.name_occluded_b") d.nameOccluded[2] = ParseFloat(val, d.nameOccluded[2]);
        else if (key == "esp.name_occluded_a") d.nameOccluded[3] = ParseFloat(val, d.nameOccluded[3]);

        else if (key == "esp.skeleton_enabled") d.skeletonEnabled = ParseBool(val);
        else if (key == "esp.skeleton_color_r") d.skeletonColor[0] = ParseFloat(val, d.skeletonColor[0]);
        else if (key == "esp.skeleton_color_g") d.skeletonColor[1] = ParseFloat(val, d.skeletonColor[1]);
        else if (key == "esp.skeleton_color_b") d.skeletonColor[2] = ParseFloat(val, d.skeletonColor[2]);
        else if (key == "esp.skeleton_color_a") d.skeletonColor[3] = ParseFloat(val, d.skeletonColor[3]);
        else if (key == "esp.skeleton_occluded_r") d.skeletonOccluded[0] = ParseFloat(val, d.skeletonOccluded[0]);
        else if (key == "esp.skeleton_occluded_g") d.skeletonOccluded[1] = ParseFloat(val, d.skeletonOccluded[1]);
        else if (key == "esp.skeleton_occluded_b") d.skeletonOccluded[2] = ParseFloat(val, d.skeletonOccluded[2]);
        else if (key == "esp.skeleton_occluded_a") d.skeletonOccluded[3] = ParseFloat(val, d.skeletonOccluded[3]);
        else if (key == "esp.skeleton_thickness") d.skeletonThickness = ParseFloat(val, d.skeletonThickness);
        else if (key == "esp.skeleton_debug") d.skeletonDebug = ParseBool(val);

        else if (key == "esp.fov_circle_enabled") d.fovCircleEnabled = ParseBool(val);
        else if (key == "esp.fov_circle_color_r") d.fovCircleColor[0] = ParseFloat(val, d.fovCircleColor[0]);
        else if (key == "esp.fov_circle_color_g") d.fovCircleColor[1] = ParseFloat(val, d.fovCircleColor[1]);
        else if (key == "esp.fov_circle_color_b") d.fovCircleColor[2] = ParseFloat(val, d.fovCircleColor[2]);
        else if (key == "esp.fov_circle_color_a") d.fovCircleColor[3] = ParseFloat(val, d.fovCircleColor[3]);
        else return false;

        return true;
    }

    static bool ParseMiscKey(const std::string& key, const std::string& val, MiscData& d) {
        if (key == "removals.flash") d.removalsFlash = ParseBool(val);
        else if (key == "removals.smoke") d.removalsSmoke = ParseBool(val);

        else if (key == "bombtimer.enabled") d.bombTimerEnabled = ParseBool(val);
        else if (key == "bombtimer.defuse") d.bombTimerDefuse = ParseBool(val);
        else if (key == "bombtimer.site") d.bombTimerSite = ParseBool(val);
        else if (key == "bombtimer.scale") d.bombTimerScale = ParseFloat(val, d.bombTimerScale);
        else if (key == "bombtimer.color_r") d.bombTimerColor[0] = ParseFloat(val, d.bombTimerColor[0]);
        else if (key == "bombtimer.color_g") d.bombTimerColor[1] = ParseFloat(val, d.bombTimerColor[1]);
        else if (key == "bombtimer.color_b") d.bombTimerColor[2] = ParseFloat(val, d.bombTimerColor[2]);
        else if (key == "bombtimer.color_a") d.bombTimerColor[3] = ParseFloat(val, d.bombTimerColor[3]);
        else if (key == "bombtimer.defuse_color_r") d.bombTimerDefuseColor[0] = ParseFloat(val, d.bombTimerDefuseColor[0]);
        else if (key == "bombtimer.defuse_color_g") d.bombTimerDefuseColor[1] = ParseFloat(val, d.bombTimerDefuseColor[1]);
        else if (key == "bombtimer.defuse_color_b") d.bombTimerDefuseColor[2] = ParseFloat(val, d.bombTimerDefuseColor[2]);
        else if (key == "bombtimer.defuse_color_a") d.bombTimerDefuseColor[3] = ParseFloat(val, d.bombTimerDefuseColor[3]);

        else if (key == "world.skybox_enabled") d.skyboxEnabled = ParseBool(val);
        else if (key == "world.skybox_index") d.skyboxIndex = ParseInt(val, d.skyboxIndex);

        else if (key == "thirdperson.enabled") d.thirdPersonEnabled = ParseBool(val);
        else if (key == "thirdperson.key") d.thirdPersonKey = ParseInt(val, d.thirdPersonKey);
        else if (key == "thirdperson.distance") d.thirdPersonDistance = ParseFloat(val, d.thirdPersonDistance);

        else if (key == "trigger.enabled") d.triggerEnabled = ParseBool(val);
        else if (key == "trigger.key") d.triggerKey = ParseInt(val, d.triggerKey);
        else if (key == "trigger.team_check") d.triggerTeamCheck = ParseBool(val);
        else if (key == "trigger.hitboxes") d.triggerHitboxes = (uint32_t)ParseInt(val, (int)d.triggerHitboxes);
        else if (key == "trigger.delay") d.triggerDelay = ParseInt(val, d.triggerDelay);

        else if (key == "aim.enabled") d.aimEnabled = ParseBool(val);
        else if (key == "aim.key") d.aimKey = ParseInt(val, d.aimKey);
        else if (key == "aim.team_check") d.aimTeamCheck = ParseBool(val);
        else if (key == "aim.hitboxes") d.aimHitboxes = (uint32_t)ParseInt(val, (int)d.aimHitboxes);
        else if (key == "aim.fov") d.aimFov = ParseFloat(val, d.aimFov);
        else if (key == "aim.smooth") d.aimSmooth = ParseInt(val, d.aimSmooth);
        else if (key == "aim.on_accuracy") d.aimOnAccuracy = ParseBool(val);
        else if (key == "aim.accuracy_speed") d.aimAccuracySpeed = ParseFloat(val, d.aimAccuracySpeed);

        else if (key == "lagcomp.max_ticks") d.menuKey = d.menuKey;

        else if (key == "menu.key") d.menuKey = ParseInt(val, d.menuKey);
        else if (key == "menu.accent_r") d.accent[0] = ParseFloat(val, d.accent[0]);
        else if (key == "menu.accent_g") d.accent[1] = ParseFloat(val, d.accent[1]);
        else if (key == "menu.accent_b") d.accent[2] = ParseFloat(val, d.accent[2]);
        else return false;

        return true;
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

        f << "backtrack.enabled=" << (Backtrack::IsEnabled() ? 1 : 0) << "\n";
        f << "backtrack.team_check=" << (Backtrack::IsTeamCheck() ? 1 : 0) << "\n";
        const float* bvc = Backtrack::GetVisibleColor();
        f << "backtrack.color_r=" << bvc[0] << "\n";
        f << "backtrack.color_g=" << bvc[1] << "\n";
        f << "backtrack.color_b=" << bvc[2] << "\n";
        f << "backtrack.color_a=" << bvc[3] << "\n";
        const float* boc = Backtrack::GetOccludedColor();
        f << "backtrack.occluded_r=" << boc[0] << "\n";
        f << "backtrack.occluded_g=" << boc[1] << "\n";
        f << "backtrack.occluded_b=" << boc[2] << "\n";
        f << "backtrack.occluded_a=" << boc[3] << "\n";
        f << "backtrack.thickness=" << Backtrack::GetThickness() << "\n";

        f << "lagcomp.max_ticks=" << Lagcomp::GetMaxTicks() << "\n";

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

        f << "thirdperson.enabled=" << (ThirdPerson::IsEnabled() ? 1 : 0) << "\n";
        f << "thirdperson.key=" << ThirdPerson::GetKey() << "\n";
        f << "thirdperson.distance=" << ThirdPerson::GetDistance() << "\n";

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

        for (int i = 0; i < (int)Rage::Group::Count; i++) {
            const Rage::Group g = (Rage::Group)i;
            const std::string prefix = std::string("rage.") + kRageGroupKeys[i] + ".";

            f << prefix << "enabled=" << (Rage::IsEnabled(g) ? 1 : 0) << "\n";
            f << prefix << "auto_shoot=" << (Rage::IsAutoShoot(g) ? 1 : 0) << "\n";
            f << prefix << "silent=" << (Rage::IsSilent(g) ? 1 : 0) << "\n";
            f << prefix << "no_spread=" << (Rage::IsNoSpread(g) ? 1 : 0) << "\n";
            f << prefix << "team_check=" << (Rage::IsTeamCheck(g) ? 1 : 0) << "\n";
            f << prefix << "backtrack=" << (Rage::IsBacktrack(g) ? 1 : 0) << "\n";
            f << prefix << "min_damage=" << Rage::GetMinDamage(g) << "\n";
            f << prefix << "hitboxes=" << (unsigned int)Rage::GetHitboxes(g) << "\n";
        }

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

        ChamsData chams;
        EspData esp;
        BacktrackData backtrackData;
        MiscData misc;
        int lagcompMaxTicks = Lagcomp::GetMaxTicks();
        RageGroupData rageGroups[(int)Rage::Group::Count];

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

            if (key == "lagcomp.max_ticks") {
                lagcompMaxTicks = ParseInt(val, lagcompMaxTicks);
            }
            else if (key.rfind("chams.", 0) == 0) {
                ParseChamsKey(key, val, chams);
            }
            else if (key.rfind("esp.", 0) == 0) {
                ParseEspKey(key, val, esp);
            }
            else if (key.rfind("backtrack.", 0) == 0) {
                ParseBacktrackKey(key, val, backtrackData);
            }
            else if (TryParseRageKey(key, val, rageGroups)) {
                continue;
            }
            else {
                ParseMiscKey(key, val, misc);
            }
        }

        Chams::SetEnabled(chams.enabled);
        Chams::SetMaterial((Materials::Id)chams.material);
        Chams::SetOccluded(chams.occluded);
        Chams::SetTeamCheck(chams.teamCheck);
        Chams::SetColor(chams.color);

        Esp::SetEnabled(esp.enabled);
        Esp::SetTeamCheck(esp.teamCheck);
        Esp::SetBoxEnabled(esp.boxEnabled);
        Esp::SetBoxStyle(esp.boxStyle);
        Esp::SetBoxFillEnabled(esp.boxFill);
        Esp::SetBoxOutlineEnabled(esp.boxOutline);
        Esp::SetBoxCornerLength(esp.boxCornerLength);
        Esp::SetBoxVisibleColor(esp.boxVisible);
        Esp::SetBoxOccludedColor(esp.boxOccluded);
        Esp::SetBoxFillColor(esp.boxFillColor);

        Esp::SetHealthEnabled(esp.healthEnabled);
        Esp::SetHealthPosition(esp.healthPosition);
        Esp::SetHealthStyle(esp.healthStyle);
        Esp::SetHealthColor(esp.healthColor);
        Esp::SetHealthLowColor(esp.healthLow);
        Esp::SetHealthHighColor(esp.healthHigh);
        Esp::SetHealthShowValue(esp.healthShowValue);
        Esp::SetHealthThickness(esp.healthThickness);

        Esp::SetAmmoEnabled(esp.ammoEnabled);
        Esp::SetAmmoPosition(esp.ammoPosition);
        Esp::SetAmmoColor(esp.ammoColor);
        Esp::SetAmmoShowValue(esp.ammoShowValue);
        Esp::SetAmmoThickness(esp.ammoThickness);

        Esp::SetNameEnabled(esp.nameEnabled);
        Esp::SetNamePosition(esp.namePosition);
        Esp::SetNameColor(esp.nameColor);
        Esp::SetNameOccludedColor(esp.nameOccluded);

        Esp::SetSkeletonEnabled(esp.skeletonEnabled);
        Esp::SetSkeletonColor(esp.skeletonColor);
        Esp::SetSkeletonOccludedColor(esp.skeletonOccluded);
        Esp::SetSkeletonThickness(esp.skeletonThickness);
        Esp::SetSkeletonDebug(esp.skeletonDebug);

        Esp::SetFovCircleEnabled(esp.fovCircleEnabled);
        Esp::SetFovCircleColor(esp.fovCircleColor);

        Backtrack::SetEnabled(backtrackData.enabled);
        Backtrack::SetTeamCheck(backtrackData.teamCheck);
        Backtrack::SetVisibleColor(backtrackData.visibleColor);
        Backtrack::SetOccludedColor(backtrackData.occludedColor);
        Backtrack::SetThickness(backtrackData.thickness);

        Lagcomp::SetMaxTicks(lagcompMaxTicks);

        Removals::SetFlashEnabled(misc.removalsFlash);
        Removals::SetSmokeEnabled(misc.removalsSmoke);

        BombTimer::SetEnabled(misc.bombTimerEnabled);
        BombTimer::SetDefuseEnabled(misc.bombTimerDefuse);
        BombTimer::SetSiteEnabled(misc.bombTimerSite);
        BombTimer::SetScale(misc.bombTimerScale);
        BombTimer::SetColor(misc.bombTimerColor);
        BombTimer::SetDefuseColor(misc.bombTimerDefuseColor);

        Skybox::SetEnabled(misc.skyboxEnabled);
        Skybox::SetSelectedIndex(misc.skyboxIndex);

        ThirdPerson::SetEnabled(misc.thirdPersonEnabled);
        ThirdPerson::SetKey(misc.thirdPersonKey);
        ThirdPerson::SetDistance(misc.thirdPersonDistance);

        Triggerbot::SetEnabled(misc.triggerEnabled);
        Triggerbot::SetKey(misc.triggerKey);
        Triggerbot::SetTeamCheck(misc.triggerTeamCheck);
        Triggerbot::SetHitboxes(misc.triggerHitboxes);
        Triggerbot::SetDelayMs(misc.triggerDelay);

        Aim::SetEnabled(misc.aimEnabled);
        Aim::SetKey(misc.aimKey);
        Aim::SetTeamCheck(misc.aimTeamCheck);
        Aim::SetHitboxes(misc.aimHitboxes);
        Aim::SetFov(misc.aimFov);
        Aim::SetSmooth(misc.aimSmooth);
        Aim::SetOnAccuracy(misc.aimOnAccuracy);
        Aim::SetAccuracySpeed(misc.aimAccuracySpeed);

        for (int i = 0; i < (int)Rage::Group::Count; i++) {
            const Rage::Group g = (Rage::Group)i;
            const auto& rg = rageGroups[i];

            Rage::SetEnabled(g, rg.enabled);
            Rage::SetAutoShoot(g, rg.autoShoot);
            Rage::SetSilent(g, rg.silent);
            Rage::SetNoSpread(g, rg.noSpread);
            Rage::SetTeamCheck(g, rg.teamCheck);
            Rage::SetBacktrack(g, rg.backtrack);
            Rage::SetMinDamage(g, rg.minDamage);
            Rage::SetHitboxes(g, rg.hitboxes);
        }

        Renderer::SetMenuKey(misc.menuKey);
        Style::SetAccent(misc.accent[0], misc.accent[1], misc.accent[2]);
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