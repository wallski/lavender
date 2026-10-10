#pragma once
#include <cstdint>
#include <mutex>
#include <atomic>
#include "../../math/math.hpp"

namespace systems {

    class view {
    public:
        struct projection {
            math::vector2 screen{};
            float w{};
            bool on_screen{};
        };

        void update(std::uintptr_t view);
        void update_matrix();

        [[nodiscard]] math::vector2 project(const math::vector3& world_pos);
        [[nodiscard]] projection project_full(const math::vector3& world_pos) const;
        [[nodiscard]] bool projection_valid(const math::vector2& screen_pos) const {
            return screen_pos.x != k_invalid && screen_pos.y != k_invalid;
        }

        [[nodiscard]] bool has_camera() const { return this->m_matrix_valid.load(std::memory_order_acquire); }
        [[nodiscard]] math::vector3 origin() const { return this->m_origin; }
        [[nodiscard]] math::vector3 angles() const { return this->m_angles; }
        [[nodiscard]] float fov() const { return this->m_fov; }
        [[nodiscard]] math::matrix4x4 matrix() const;

        static constexpr float k_invalid = 0xdead;

    private:
        math::matrix4x4 m_matrix{};
        mutable std::mutex m_matrix_mtx{};
        std::atomic<bool> m_matrix_valid{};
        math::vector3 m_origin{};
        math::vector3 m_angles{};
        float m_fov{ k_invalid };
    };

}