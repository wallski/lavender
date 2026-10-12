#pragma once
#include <cstdint>

namespace systems {
    namespace schemas {

        bool Initialize();
        void Shutdown();

        std::uint32_t lookup(const char* class_name, std::uint32_t field_hash);

    }
}

#define SCHEMA(class_name, field_hash) \
    ([]() -> std::uint32_t { \
        static std::uint32_t val = 0; \
        if (val == 0) { \
            val = ::systems::schemas::lookup(class_name, field_hash); \
        } \
        return val; \
    }())