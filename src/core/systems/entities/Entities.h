#pragma once
#include <cstdint>
#include <vector>
#include <shared_mutex>
#include <array>

namespace systems {

    class entities {
    public:
        enum class type : std::uint8_t {
            unknown,
            player,
            item,
            projectile
        };

        struct cached {
            std::uintptr_t ptr{};
            std::uint32_t  schema_hash{};
            std::int16_t   index{};
            type           type{};
        };

        void on_add_entity(std::uintptr_t entity, std::uint32_t handle);
        void on_remove_entity(std::uintptr_t entity, std::uint32_t handle);
        void force_update();

        [[nodiscard]] bool exists(std::uintptr_t entity) const;
        [[nodiscard]] const char* get_schema_name(std::uintptr_t entity) const;
        [[nodiscard]] std::uintptr_t get_by_index(int index);
        [[nodiscard]] std::uintptr_t lookup(std::uint32_t handle) const;
        [[nodiscard]] std::vector<cached> get_by_type(type type) const;

        [[nodiscard]] bool is_empty() const;

    private:
        [[nodiscard]] type classify_entity(std::uint32_t schema_hash) const;

        std::vector<cached> m_cached{};
        mutable std::shared_mutex m_cache_mtx{};
        std::array<std::uintptr_t, 32> m_cached_list_entries{};
        std::uintptr_t m_cached_entity_list{};
    };

}