#pragma once
#include <cstdint>
#include <shared_mutex>
#include <atomic>

namespace systems {

    class local {
    public:
        struct snapshot {
            std::uintptr_t controller{};
            std::uintptr_t pawn{};
            std::uintptr_t observer_pawn{};
            std::uintptr_t observer_controller{};
            int  team{};
            int  view_team{};
            bool is_alive{};
            bool is_team_mode{};

            [[nodiscard]] std::uintptr_t view_controller() const { return this->is_alive ? this->controller : this->observer_controller; }
            [[nodiscard]] std::uintptr_t view_pawn() const { return this->is_alive ? this->pawn : this->observer_pawn; }
            [[nodiscard]] bool is_valid() const { return this->pawn != 0 || this->observer_pawn != 0; }
            [[nodiscard]] bool is_this_other_team(int other_team) const { return !this->is_team_mode || this->view_team != other_team; }
        };

        void update();

        [[nodiscard]] snapshot get() const {
            std::shared_lock lock(this->m_mtx);
            return this->m_snapshot;
        }

        [[nodiscard]] bool is_in_cinematic()    const { return this->m_is_in_cinematic.load(); }
        [[nodiscard]] bool is_in_time_freeze()  const { return this->m_is_in_time_freeze.load(); }
        [[nodiscard]] bool is_in_deathmatch()   const { return this->m_is_deathmatch.load(); }

        void reset();

    private:
        snapshot m_snapshot{};
        mutable std::shared_mutex m_mtx{};

        std::atomic<bool> m_is_deathmatch{};
        std::atomic<bool> m_is_in_cinematic{};
        std::atomic<bool> m_is_in_time_freeze{};
    };

}