#pragma once
#include <cstdint>

struct HasBits { uint32_t bits; };

struct InButtonStatePb {
    HasBits has_bits;
    uint32_t cached_size;
    uint64_t buttonstate1;
    uint64_t buttonstate2;
    uint64_t buttonstate3;
};

struct BaseUsercmdPb {
    HasBits has_bits;
    uint32_t cached_size;
    uint8_t _pad_to_buttons[0x18];
    InButtonStatePb* buttons_pb;
};

struct RepeatedPtrField {
    void* arena;
    int current_size;
    int total_size;
    struct Rep { int allocated_size; void* elements[1]; };
    Rep* rep;
};

struct CsgoUsercmdPb {
    HasBits has_bits;
    uint32_t cached_size;
    RepeatedPtrField input_history;
    BaseUsercmdPb* base;
    bool left_hand_desired;
    bool is_predicting_body_shot_fx;
    bool is_predicting_head_shot_fx;
    bool is_predicting_kill_ragdolls;
    int32_t attack1_start_history_index;
    int32_t attack2_start_history_index;
};

struct InButtonStateCache {
    void* vtable;
    uintptr_t value;
    uintptr_t value_changed;
    uintptr_t value_scroll;
};

struct UsercmdLayout {
    void* vtable;
    intptr_t command_number;
    void* proto_vtable;
    void* proto_arena;
    CsgoUsercmdPb pb;
    InButtonStateCache buttons;
};