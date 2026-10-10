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
        static const auto val = ::systems::schemas::lookup(class_name, field_hash); \
        return val; \
    }())