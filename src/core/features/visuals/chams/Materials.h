#pragma once
#include <cstdint>

namespace Materials {

    enum class Id : int {
        None = 0,
        Matte,
        Flat,
        Glow,
        Outlines,
        Bloom,
        MatteIgnoreZ,
        FlatIgnoreZ,
        GlowIgnoreZ,
        OutlinesIgnoreZ,
        BloomIgnoreZ,
        Count
    };

    bool Initialize();
    void Shutdown();

    uintptr_t Compile(const char* kv, const char* name);

    uintptr_t Get(Id id);
    const char* GetName(Id id);
    int GetCount();
}