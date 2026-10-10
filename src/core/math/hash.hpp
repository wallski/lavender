#pragma once
#include <cstdint>
#include <cstddef>

constexpr std::uint32_t fnv1a_hash(const char* s, std::size_t len) noexcept {
    std::uint32_t h = 0x811C9DC5u;
    for (std::size_t i = 0; i < len; ++i) {
        h ^= static_cast<std::uint32_t>(static_cast<std::uint8_t>(s[i]));
        h *= 0x01000193u;
    }
    return h;
}

inline constexpr std::uint32_t operator"" _hash(const char* s, std::size_t len) noexcept {
    return fnv1a_hash(s, len);
}

namespace fnv1a {

    inline std::uint32_t runtime_hash(const char* s) noexcept {
        std::uint32_t h = 0x811C9DC5u;
        while (*s) {
            h ^= static_cast<std::uint32_t>(static_cast<std::uint8_t>(*s++));
            h *= 0x01000193u;
        }
        return h;
    }

}