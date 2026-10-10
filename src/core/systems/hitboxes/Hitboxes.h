#pragma once
#include <cstdint>
#include <array>
#include "../../math/math.hpp"

namespace systems {

    class hitboxes {
    public:
        struct entry {
            int index{ -1 };
            int bone{ -1 };
            math::vector3 mins{};
            math::vector3 maxs{};
            float radius{};
            std::uint8_t shape_type{};
            bool translation_only{};
        };

        struct set {
            std::array<entry, 20> entries{};
            int count{};

            [[nodiscard]] const entry* begin() const { return this->entries.data(); }
            [[nodiscard]] const entry* end() const { return this->entries.data() + this->count; }
        };

        [[nodiscard]] set query(std::uintptr_t game_scene_node, bool seh = false);
        [[nodiscard]] int hitgroup_from_hitbox(int hitbox);
        [[nodiscard]] const char* hitgroup_to_name(int hitgroup);
    };

}