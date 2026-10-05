#pragma once
#include <cstdint>
#include <cstddef>

namespace proto {

    constexpr std::uintptr_t message_impl_offset{ 0x10 };

    template <typename T>
    inline T* impl_ptr(void* msg) {
        if (!msg) return nullptr;
        return reinterpret_cast<T*>(reinterpret_cast<std::uintptr_t>(msg) + message_impl_offset);
    }

    template <typename T>
    inline const T* impl_ptr(const void* msg) {
        if (!msg) return nullptr;
        return reinterpret_cast<const T*>(reinterpret_cast<std::uintptr_t>(msg) + message_impl_offset);
    }

    struct has_bits {
        std::uint32_t bits[1]{};

        bool test(std::uint32_t mask) const { return (bits[0] & mask) != 0; }
        void set(std::uint32_t mask) { bits[0] |= mask; }
        void clear(std::uint32_t mask) { bits[0] &= ~mask; }
    };

    template <typename T>
    struct repeated_ptr_field {
        void* m_arena;
        int m_current_size;
        int m_total_size;

        struct rep_t {
            int allocated_size;
            void* elements[1];
        };

        rep_t* m_rep;

        bool empty() const { return m_current_size == 0; }
        int size() const { return m_current_size; }

        T* mutable_at(int index) {
            return impl_ptr<T>(m_rep->elements[index]);
        }
    };

    struct msg_qangle {
        has_bits m_has_bits;
        std::uint32_t m_cached_size;
        float m_x;
        float m_y;
        float m_z;

        float x() const { return m_x; }
        float y() const { return m_y; }
        float z() const { return m_z; }
        void set_x(float v) { m_has_bits.set(0x1u); m_x = v; }
        void set_y(float v) { m_has_bits.set(0x2u); m_y = v; }
        void set_z(float v) { m_has_bits.set(0x4u); m_z = v; }
    };

    struct subtick_move_step {
        has_bits m_has_bits;
        std::uint32_t m_cached_size;
        std::uint64_t m_button;
        bool m_pressed;
        float m_when;
        float m_analog_forward_delta;
        float m_analog_left_delta;
        float m_pitch_delta;
        float m_yaw_delta;

        std::uint64_t button() const { return m_button; }
        void set_button(std::uint64_t v) { m_has_bits.set(0x1u); m_button = v; }
        bool pressed() const { return m_pressed; }
        void set_pressed(bool v) { m_has_bits.set(0x2u); m_pressed = v; }
        float when() const { return m_when; }
        void set_when(float v) { m_has_bits.set(0x4u); m_when = v; }
        float analog_forward_delta() const { return m_analog_forward_delta; }
        void set_analog_forward_delta(float v) { m_has_bits.set(0x8u); m_analog_forward_delta = v; }
        float analog_left_delta() const { return m_analog_left_delta; }
        void set_analog_left_delta(float v) { m_has_bits.set(0x10u); m_analog_left_delta = v; }
        float pitch_delta() const { return m_pitch_delta; }
        void set_pitch_delta(float v) { m_has_bits.set(0x20u); m_pitch_delta = v; }
        float yaw_delta() const { return m_yaw_delta; }
        void set_yaw_delta(float v) { m_has_bits.set(0x40u); m_yaw_delta = v; }
    };

    struct in_button_state_pb {
        has_bits m_has_bits;
        std::uint32_t m_cached_size;
        std::uint64_t m_buttonstate1;
        std::uint64_t m_buttonstate2;
        std::uint64_t m_buttonstate3;

        std::uint64_t buttonstate1() const { return m_buttonstate1; }
        std::uint64_t buttonstate2() const { return m_buttonstate2; }
        std::uint64_t buttonstate3() const { return m_buttonstate3; }
        void set_buttonstate1(std::uint64_t v) { m_has_bits.set(0x1u); m_buttonstate1 = v; }
        void set_buttonstate2(std::uint64_t v) { m_has_bits.set(0x2u); m_buttonstate2 = v; }
        void set_buttonstate3(std::uint64_t v) { m_has_bits.set(0x4u); m_buttonstate3 = v; }
    };

    struct base_usercmd_pb {
        has_bits m_has_bits;
        std::uint32_t m_cached_size;
        repeated_ptr_field<subtick_move_step> m_subtick_moves;
        void* m_move_crc;
        in_button_state_pb* m_buttons_pb;
        msg_qangle* m_viewangles;
        void* m_execution_notes;
        std::int32_t m_legacy_command_number;
        std::int32_t m_client_tick;
        float m_forwardmove;
        float m_leftmove;
        float m_upmove;
        std::int32_t m_impulse;
        std::int32_t m_weaponselect;
        std::int32_t m_random_seed;
        std::int32_t m_mousedx;
        std::int32_t m_mousedy;
        std::uint32_t m_prediction_offset_ticks_x256;
        std::uint32_t m_consumed_server_angle_changes;
        std::int32_t m_cmd_flags;
        std::uint32_t m_pawn_entity_handle;

        bool has_buttons_pb() const { return m_has_bits.test(0x2u); }
        in_button_state_pb* mutable_buttons_pb() { m_has_bits.set(0x2u); return impl_ptr<in_button_state_pb>(m_buttons_pb); }

        bool has_viewangles() const { return m_has_bits.test(0x4u); }
        msg_qangle* mutable_viewangles() { m_has_bits.set(0x4u); return impl_ptr<msg_qangle>(m_viewangles); }

        repeated_ptr_field<subtick_move_step>* mutable_subtick_moves() { return &m_subtick_moves; }
        int subtick_moves_size() const { return m_subtick_moves.size(); }
        subtick_move_step* mutable_subtick_moves(int i) { return m_subtick_moves.mutable_at(i); }
    };

    struct csgo_usercmd_pb {
        has_bits m_has_bits;
        std::uint32_t m_cached_size;
        struct input_history_field {
            void* m_arena;
            int m_current_size;
            int m_total_size;
            void* m_rep;
        } m_input_history;
        base_usercmd_pb* m_base;
        bool m_left_hand_desired;
        bool m_is_predicting_body_shot_fx;
        bool m_is_predicting_head_shot_fx;
        bool m_is_predicting_kill_ragdolls;
        std::int32_t m_attack1_start_history_index;
        std::int32_t m_attack2_start_history_index;

        bool has_base() const { return m_has_bits.test(0x1u); }
        base_usercmd_pb* mutable_base() { m_has_bits.set(0x1u); return impl_ptr<base_usercmd_pb>(m_base); }
    };

}