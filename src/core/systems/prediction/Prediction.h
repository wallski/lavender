#pragma once
#include <cstdint>
#include <mutex>
#include <functional>
#include "../../math/math.hpp"

namespace systems {

    class prediction {
    public:
        struct state {
            std::uint32_t flags{};
            math::vector3 networked_velocity{};
            math::vector3 velocity{};
            math::vector3 origin{};
            math::vector3 networked_origin{};
            math::vector3 last_movement_impulses{};
            float surface_friction{};
            float stamina{};
        };

        void capture_prestate(std::uintptr_t local_pawn, std::uintptr_t movement_services);
        bool simulate(void* cmd, const void* local_snapshot, const std::function<void()>& fn);

        [[nodiscard]] const state& pre() const { return this->m_prestate; }

    private:
        state m_prestate{};
        std::mutex m_simulation_mtx{};
    };

}