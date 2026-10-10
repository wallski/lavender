#include "pch.h"
#include "Local.h"
#include "../entities/Entities.h"
#include "../schemas/Schemas.h"
#include "../systems.hpp"
#include "../../cvar/CVar.h"
#include "../../../config/OffsetConfig.h"
#include "../../math/hash.hpp"

namespace systems {

    static std::uintptr_t SafeReadPtr(std::uintptr_t addr) {
        __try { return *reinterpret_cast<std::uintptr_t*>(addr); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
    }

    static std::uint32_t SafeReadU32(std::uintptr_t addr) {
        __try { return *reinterpret_cast<std::uint32_t*>(addr); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
        return 0;
    }

    static int SafeReadInt(std::uintptr_t addr) {
        __try { return *reinterpret_cast<int*>(addr); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
        return 0;
    }

    static std::uint8_t SafeReadU8(std::uintptr_t addr) {
        __try { return *reinterpret_cast<std::uint8_t*>(addr); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
        return 0;
    }

    static bool SafeReadBool(std::uintptr_t addr) {
        __try { return *reinterpret_cast<bool*>(addr); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        return false;
    }

    static float SafeReadFloat(std::uintptr_t addr) {
        __try { return *reinterpret_cast<float*>(addr); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return 0.0f; }
        return 0.0f;
    }

    void local::update() {
        const auto local_controller = SafeReadPtr(OffsetConfig::globals.dwLocalPlayerController);
        if (!local_controller) {
            this->reset();
            return;
        }

        const auto pawn_handle = SafeReadU32(local_controller + SCHEMA("CBasePlayerController", "m_hPawn"_hash));
        if (!pawn_handle) {
            this->reset();
            return;
        }

        const auto pawn = g_entities.lookup(pawn_handle);
        if (!pawn) {
            this->reset();
            return;
        }

        snapshot s{};
        s.controller = local_controller;
        s.pawn = pawn;
        s.team = SafeReadInt(pawn + SCHEMA("C_BaseEntity", "m_iTeamNum"_hash));

        const auto health = SafeReadInt(pawn + SCHEMA("C_BaseEntity", "m_iHealth"_hash));
        const auto life_state = SafeReadU8(pawn + SCHEMA("C_BaseEntity", "m_lifeState"_hash));
        s.is_alive = health > 0 && life_state == 0;

        if (s.is_alive) {
            s.view_team = s.team;
        }
        else {
            const auto observer_pawn_handle = SafeReadU32(local_controller + SCHEMA("CCSPlayerController", "m_hObserverPawn"_hash));
            if (observer_pawn_handle) {
                const auto observer_pawn = g_entities.lookup(observer_pawn_handle);
                if (observer_pawn) {
                    const auto observer_services = SafeReadPtr(observer_pawn + SCHEMA("C_BasePlayerPawn", "m_pObserverServices"_hash));
                    if (observer_services) {
                        const auto observer_target_handle = SafeReadU32(observer_services + SCHEMA("CPlayer_ObserverServices", "m_hObserverTarget"_hash));
                        if (observer_target_handle) {
                            const auto observer_target = g_entities.lookup(observer_target_handle);
                            if (observer_target) {
                                s.observer_pawn = observer_target;
                                s.view_team = SafeReadInt(observer_target + SCHEMA("C_BaseEntity", "m_iTeamNum"_hash));

                                const auto observer_target_controller_handle = SafeReadU32(observer_target + SCHEMA("C_BasePlayerPawn", "m_hController"_hash));
                                if (observer_target_controller_handle) {
                                    const auto observer_target_controller = g_entities.lookup(observer_target_controller_handle);
                                    if (observer_target_controller) {
                                        s.observer_controller = observer_target_controller;
                                    }
                                }
                            }
                        }
                    }
                }
            }

            if (!s.observer_pawn) {
                s.view_team = s.team;
            }
        }

        {
            const auto game_type = CVar::GetInt("game_type");
            const auto game_mode = CVar::GetInt("game_mode");
            const auto is_ffa = (game_type == 1 && game_mode == 2) || (game_type == 2 && game_mode == 0);

            s.is_team_mode = !is_ffa;
            this->m_is_deathmatch.store(game_type == 1 && game_mode == 2);
        }

        {
            const auto game_rules = SafeReadPtr(OffsetConfig::globals.dwGameRules);
            const auto global_vars = SafeReadPtr(OffsetConfig::globals.dwGlobalVars);

            auto cinematic{ false };
            auto freezetime{ false };

            if (game_rules && global_vars) {
                if (SafeReadBool(game_rules + SCHEMA("C_CSGameRules", "m_bTeamIntroPeriod"_hash))) {
                    cinematic = true;
                }
                else if (SafeReadInt(game_rules + SCHEMA("C_CSGameRules", "m_gamePhase"_hash)) >= 4) {
                    cinematic = true;
                }

                if (SafeReadBool(game_rules + SCHEMA("C_CSGameRules", "m_bFreezePeriod"_hash))) {
                    freezetime = true;
                }
                else {
                    const auto round_start_time = SafeReadFloat(game_rules + SCHEMA("C_CSGameRules", "m_fRoundStartTime"_hash));
                    const auto current_time = SafeReadFloat(global_vars + 0x30);
                    if (round_start_time > current_time) freezetime = true;
                }
            }

            this->m_is_in_cinematic.store(cinematic);
            this->m_is_in_time_freeze.store(freezetime);
        }

        {
            std::unique_lock lock(this->m_mtx);
            this->m_snapshot = s;
        }
    }

    void local::reset() {
        {
            std::unique_lock lock(this->m_mtx);
            this->m_snapshot = {};
        }
        this->m_is_deathmatch.store(false);
        this->m_is_in_cinematic.store(false);
        this->m_is_in_time_freeze.store(false);
    }

}