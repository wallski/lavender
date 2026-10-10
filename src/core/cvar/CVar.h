#pragma once
#include <cstdint>
#include <cstddef>
#include <type_traits>
#include <Windows.h>

union CVValue {
    bool b;
    short s;
    int i;
    int64_t i64;
    float f;
    double d;
    const char* str;
};

class CConVar {
public:
    const char* m_name;
    const void* m_default_value_ptr;
    const void* m_min_value;
    const void* m_max_value;
    const char* m_description;
    int16_t m_type;
    char _pad_01[0x2];
    uint32_t m_change_count;
    uint64_t m_flags;
    char _pad_02[0x20];
    CVValue m_value;
    char _value_tail[0x8];

    template <typename T>
    T get() const {
        if constexpr (std::is_same_v<T, bool>) return m_value.b;
        else if constexpr (std::is_same_v<T, short>) return m_value.s;
        else if constexpr (std::is_same_v<T, int>) return m_value.i;
        else if constexpr (std::is_same_v<T, int64_t>) return m_value.i64;
        else if constexpr (std::is_same_v<T, float>) return m_value.f;
        else return T{};
    }

    template <typename T>
    T Get() const { return this->get<T>(); }

    void Set(bool v) { m_value.b = v; }
    void Set(short v) { m_value.s = v; }
    void Set(int v) { m_value.i = v; }
    void Set(int64_t v) { m_value.i64 = v; }
    void Set(float v) { m_value.f = v; }
};

class CEngineCVar {
public:
    struct Container {
        CConVar* m_cvar;
        uint16_t m_generation;
        uint16_t m_next_index;
        uint32_t m_links;
    };

    char _pad0[0x4A];
    uint16_t m_allocation_count;
    char _pad1[0x4];
    Container* m_container;
    uint16_t m_head;

    CConVar* Find(uint32_t name_hash);
    bool UnlockAll();
};

static_assert(offsetof(CConVar, m_flags) == 0x30);
static_assert(offsetof(CConVar, m_value) == 0x58);
static_assert(sizeof(CEngineCVar::Container) == 0x10);
static_assert(offsetof(CEngineCVar, m_container) == 0x50);
static_assert(offsetof(CEngineCVar, m_head) == 0x58);

namespace CVar {

    constexpr uint32_t Hash(const char* s, uint32_t h = 0x811C9DC5u) {
        return *s ? Hash(s + 1, (h ^ static_cast<uint32_t>(static_cast<uint8_t>(*s))) * 0x01000193u) : h;
    }

    bool Initialize();
    void Shutdown();

    CEngineCVar* GetInterface();
    CConVar* Find(const char* name);

    bool GetBool(const char* name, bool fallback = false);
    int GetInt(const char* name, int fallback = 0);
    float GetFloat(const char* name, float fallback = 0.0f);

    void SetBool(const char* name, bool value);
    void SetInt(const char* name, int value);
    void SetFloat(const char* name, float value);

    void DumpMatching(const char* substring);
}

#define CONVAR(name) ::CVar::Find(name)