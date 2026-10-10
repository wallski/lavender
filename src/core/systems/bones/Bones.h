#pragma once
#include <cstdint>
#include <array>
#include "../../math/math.hpp"

namespace systems {

    class bones {
    public:
        struct data {
            math::vector3 position{};
            float scale{};
            math::quaternion rotation{};
        };

        [[nodiscard]] data get(std::uintptr_t entity, std::uint32_t bone_id);
        [[nodiscard]] std::array<data, 27> get_skeleton(std::uintptr_t entity);

    private:
        [[nodiscard]] std::uintptr_t get_bone_cache(std::uintptr_t entity, int* out_count = nullptr);
    };

}