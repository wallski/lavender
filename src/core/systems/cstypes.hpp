#pragma once
#include <cstdint>

namespace cstypes {

    constexpr float tick_interval = 1.0f / 64.0f;
    constexpr float tick_rate = 64.0f;

    constexpr int time_to_ticks(float time) noexcept {
        return static_cast<int>(time / tick_interval + (time >= 0.0f ? 0.5f : -0.5f));
    }

    constexpr float ticks_to_time(int ticks) noexcept {
        return static_cast<float>(ticks) * tick_interval;
    }

    namespace command_buttons {
        constexpr std::uintptr_t in_attack = 1ull << 0;
        constexpr std::uintptr_t in_jump = 1ull << 1;
        constexpr std::uintptr_t in_duck = 1ull << 2;
        constexpr std::uintptr_t in_forward = 1ull << 3;
        constexpr std::uintptr_t in_back = 1ull << 4;
        constexpr std::uintptr_t in_use = 1ull << 5;
        constexpr std::uintptr_t in_cancel = 1ull << 6;
        constexpr std::uintptr_t in_left = 1ull << 7;
        constexpr std::uintptr_t in_right = 1ull << 8;
        constexpr std::uintptr_t in_moveleft = 1ull << 9;
        constexpr std::uintptr_t in_moveright = 1ull << 10;
        constexpr std::uintptr_t in_attack2 = 1ull << 11;
        constexpr std::uintptr_t in_run = 1ull << 12;
        constexpr std::uintptr_t in_reload = 1ull << 13;
        constexpr std::uintptr_t in_alt1 = 1ull << 14;
        constexpr std::uintptr_t in_alt2 = 1ull << 15;
        constexpr std::uintptr_t in_score = 1ull << 16;
        constexpr std::uintptr_t in_sprint = 1ull << 17;
        constexpr std::uintptr_t in_walk = 1ull << 18;
        constexpr std::uintptr_t in_zoom = 1ull << 19;
        constexpr std::uintptr_t in_weapon1 = 1ull << 20;
        constexpr std::uintptr_t in_weapon2 = 1ull << 21;
        constexpr std::uintptr_t in_bullrush = 1ull << 22;
        constexpr std::uintptr_t in_grenade1 = 1ull << 23;
        constexpr std::uintptr_t in_grenade2 = 1ull << 24;
        constexpr std::uintptr_t in_attack3 = 1ull << 25;
    }

    namespace entity_flags {
        constexpr std::uint32_t on_ground = 1u << 0;
        constexpr std::uint32_t ducking = 1u << 1;
        constexpr std::uint32_t water_jump = 1u << 2;
        constexpr std::uint32_t on_train = 1u << 3;
        constexpr std::uint32_t in_rain = 1u << 4;
        constexpr std::uint32_t frozen = 1u << 5;
        constexpr std::uint32_t at_controls = 1u << 6;
        constexpr std::uint32_t client = 1u << 7;
        constexpr std::uint32_t fake_client = 1u << 8;
        constexpr std::uint32_t in_water = 1u << 9;
        constexpr std::uint32_t fly = 1u << 10;
        constexpr std::uint32_t swim = 1u << 11;
        constexpr std::uint32_t conveyor = 1u << 12;
        constexpr std::uint32_t npc = 1u << 13;
        constexpr std::uint32_t godmode = 1u << 14;
        constexpr std::uint32_t notarget = 1u << 15;
        constexpr std::uint32_t aim_target = 1u << 16;
        constexpr std::uint32_t partial_ground = 1u << 17;
        constexpr std::uint32_t static_prop = 1u << 18;
        constexpr std::uint32_t graphed = 1u << 19;
        constexpr std::uint32_t grenade = 1u << 20;
        constexpr std::uint32_t step_movement = 1u << 21;
        constexpr std::uint32_t dont_touch = 1u << 22;
        constexpr std::uint32_t base_velocity = 1u << 23;
        constexpr std::uint32_t world_brush = 1u << 24;
        constexpr std::uint32_t object = 1u << 25;
        constexpr std::uint32_t kill_me = 1u << 26;
        constexpr std::uint32_t on_fire = 1u << 27;
        constexpr std::uint32_t dissolving = 1u << 28;
        constexpr std::uint32_t trans_ragdoll = 1u << 29;
        constexpr std::uint32_t unblockable_by_player = 1u << 30;
    }

    enum class weapon_type : std::uint32_t {
        knife = 0,
        pistol = 1,
        smg = 2,
        rifle = 3,
        shotgun = 4,
        sniper = 5,
        lmg = 6,
        c4 = 7,
        taser = 8,
        grenade = 9,
        stackable_item = 10,
        fists = 11,
        breach_charge = 12,
        bump_mine = 13,
        tablet = 14,
        melee = 15,
        shield = 16,
        zone_repulsor = 17,
        unknown = 18
    };

    enum class move_type : std::uint8_t {
        none = 0,
        walk = 1,
        run = 2,
        fly = 3,
        fly_gravity = 4,
        vphysics = 5,
        push_physics = 6,
        noclip = 7,
        ladder = 8,
        observer = 9,
        custom = 10
    };

    enum class item_definition_index : std::uint16_t {
        weapon_none = 0,
        weapon_deagle = 1,
        weapon_elite = 2,
        weapon_fiveseven = 3,
        weapon_glock = 4,
        weapon_ak47 = 7,
        weapon_aug = 8,
        weapon_awp = 9,
        weapon_famas = 10,
        weapon_g3sg1 = 11,
        weapon_galilar = 13,
        weapon_m249 = 14,
        weapon_m4a1 = 16,
        weapon_mac10 = 17,
        weapon_p90 = 19,
        weapon_mp5sd = 23,
        weapon_ump45 = 24,
        weapon_xm1014 = 25,
        weapon_bizon = 26,
        weapon_mag7 = 27,
        weapon_negev = 28,
        weapon_sawedoff = 29,
        weapon_tec9 = 30,
        weapon_taser = 31,
        weapon_hkp2000 = 32,
        weapon_mp7 = 33,
        weapon_mp9 = 34,
        weapon_nova = 35,
        weapon_p250 = 36,
        weapon_shield = 37,
        weapon_scar20 = 38,
        weapon_sg556 = 39,
        weapon_ssg_08 = 40,
        weapon_knife_gg = 41,
        weapon_knife_t = 42,
        weapon_flashbang = 43,
        weapon_hegrenade = 44,
        weapon_smokegrenade = 45,
        weapon_molotov = 46,
        weapon_decoy = 47,
        weapon_incgrenade = 48,
        weapon_c4 = 49,
        weapon_knife_ct = 59,
        weapon_m4a1_silencer = 60,
        weapon_usp_silencer = 61,
        weapon_cz75a = 63,
        weapon_revolver = 64
    };

    namespace bone_ids {
        constexpr std::uint32_t pelvis = 1;
        constexpr std::uint32_t spine_1 = 2;
        constexpr std::uint32_t spine_2 = 3;
        constexpr std::uint32_t spine_3 = 4;
        constexpr std::uint32_t spine_4 = 5;
        constexpr std::uint32_t neck = 6;
        constexpr std::uint32_t head = 7;
        constexpr std::uint32_t left_clavicle = 8;
        constexpr std::uint32_t left_shoulder = 9;
        constexpr std::uint32_t left_elbow = 10;
        constexpr std::uint32_t left_hand = 11;
        constexpr std::uint32_t right_clavicle = 12;
        constexpr std::uint32_t right_shoulder = 13;
        constexpr std::uint32_t right_elbow = 14;
        constexpr std::uint32_t right_hand = 15;
        constexpr std::uint32_t left_hip = 16;
        constexpr std::uint32_t left_knee = 17;
        constexpr std::uint32_t left_foot = 18;
        constexpr std::uint32_t right_hip = 19;
        constexpr std::uint32_t right_knee = 20;
        constexpr std::uint32_t right_foot = 21;
    }

    struct tick_fraction {
        int tick{};
        float frac{};

        tick_fraction() = default;
        constexpr tick_fraction(int t, float f) noexcept : tick(t), frac(f) {}

        static tick_fraction from_value(float value) noexcept {
            const float ticks_f = value / tick_interval;
            const int t = static_cast<int>(ticks_f);
            return { t, ticks_f - static_cast<float>(t) };
        }

        float to_value() const noexcept {
            return static_cast<float>(tick) + frac;
        }

        tick_fraction subtract(const tick_fraction& o) const noexcept {
            float f = frac - o.frac;
            int t = tick - o.tick;
            while (f < 0.0f) { f += 1.0f; t -= 1; }
            while (f >= 1.0f) { f -= 1.0f; t += 1; }
            return { t, f };
        }

        tick_fraction subtract_value(float value) const noexcept {
            return subtract(from_value(value));
        }

        tick_fraction add(const tick_fraction& o) const noexcept {
            float f = frac + o.frac;
            int t = tick + o.tick;
            while (f >= 1.0f) { f -= 1.0f; t += 1; }
            while (f < 0.0f) { f += 1.0f; t -= 1; }
            return { t, f };
        }
    };

} // namespace cstypes