#include "pch.h"
#include "InputHelper.h"
#include "../memory/PatternScan.h"
#include "../features/combat/legit/triggerbot/UsercmdLayout.h"

namespace InputHelper {

    static uintptr_t g_stringCopy = 0;
    static uintptr_t g_serializeMoveCrc = 0;
    static uintptr_t g_subtickMoveAlloc = 0;
    static uintptr_t g_utlVectorPush = 0;
    static uintptr_t g_buttonStateAlloc = 0;

    static constexpr uintptr_t kMessageImplOffset = 0x10;

    static uintptr_t ResolveCallTarget(uintptr_t callInstr) {
        int32_t rel = 0;
        __try { rel = *reinterpret_cast<int32_t*>(callInstr + 1); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
        return callInstr + 5 + (intptr_t)rel;
    }

    static uintptr_t SafeReadPtr(uintptr_t addr) {
        __try { return *reinterpret_cast<uintptr_t*>(addr); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
    }

    bool Initialize() {
        {
            uintptr_t call = Memory::FindPattern("client.dll",
                "E8 ? ? ? ? 0F 10 45 88");
            if (call) g_stringCopy = ResolveCallTarget(call);
        }
        {
            g_serializeMoveCrc = Memory::FindPattern("client.dll",
                "48 89 5C 24 ? 55 56 57 48 83 EC 30 49 8B C0 48 8B FA 48 8B F1 48 8B 09 F6 C1 03");
        }
        {
            uintptr_t alloc = Memory::FindPattern("client.dll",
                "48 8B 54 CA 08 8D 41 01 89 47 08 EB 16 48 8B 0F E8 ? ? ? ? 48 8B D0 48 8B CF");
            if (alloc) g_subtickMoveAlloc = ResolveCallTarget(alloc + 16);
        }
        {
            uintptr_t push = Memory::FindPattern("client.dll",
                "E8 ? ? ? ? 4C 8B D0 45 8B 4A 10");
            if (push) g_utlVectorPush = ResolveCallTarget(push);
        }
        {
            uintptr_t alloc = Memory::FindPattern("client.dll",
                "48 8B 54 CA 08 8D 41 01 89 47 08 EB 16 48 8B 0F E8 ? ? ? ? 48 8B D0 48 8B CF");
            if (alloc) g_buttonStateAlloc = ResolveCallTarget(alloc + 16);
        }

        if (!g_stringCopy || !g_serializeMoveCrc) {
            printf("[input-helper] crc sigs missing\n");
            fflush(stdout);
        }

        return g_stringCopy != 0 && g_serializeMoveCrc != 0;
    }

    void Shutdown() {
        g_stringCopy = 0;
        g_serializeMoveCrc = 0;
        g_subtickMoveAlloc = 0;
        g_utlVectorPush = 0;
        g_buttonStateAlloc = 0;
    }

    uintptr_t EnsureButtons(proto::base_usercmd_pb* base) {
        if (!base) return 0;

        uintptr_t existing = 0;
        __try { existing = reinterpret_cast<uintptr_t>(base->m_buttons_pb); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }

        if (existing >= 0x10000) {
            return existing + kMessageImplOffset;
        }

        if (!g_buttonStateAlloc) return 0;

        uintptr_t rawBase = reinterpret_cast<uintptr_t>(base) - kMessageImplOffset;
        uintptr_t arenaBits = SafeReadPtr(rawBase + 0x08);
        uintptr_t arena = arenaBits & ~0x3ull;
        if (arenaBits & 1) arena = SafeReadPtr(arena);

        void* rawButtons = nullptr;
        __try { rawButtons = Memory::Call<void*>(g_buttonStateAlloc, arena); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
        if (!rawButtons) return 0;

        __try { base->m_buttons_pb = reinterpret_cast<proto::in_button_state_pb*>(rawButtons); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }

        return reinterpret_cast<uintptr_t>(rawButtons) + kMessageImplOffset;
    }

    void RecalculateCrc(UsercmdLayout* cmd) {
        if (!g_stringCopy || !g_serializeMoveCrc) return;
        if (!cmd) return;

        uintptr_t baseWrapper = reinterpret_cast<uintptr_t>(cmd->pb.base);
        if (baseWrapper < 0x10000) return;

        proto::base_usercmd_pb* base =
            reinterpret_cast<proto::base_usercmd_pb*>(baseWrapper + kMessageImplOffset);

        uint64_t bs1 = 0, bs2 = 0, bs3 = 0;
        float pitch = 0.0f, yaw = 0.0f, roll = 0.0f;

        __try {
            const proto::in_button_state_pb* btns =
                proto::impl_ptr<proto::in_button_state_pb>(base->m_buttons_pb);
            if (btns) {
                bs1 = btns->m_buttonstate1;
                bs2 = btns->m_buttonstate2;
                bs3 = btns->m_buttonstate3;
            }

            const proto::msg_qangle* va =
                proto::impl_ptr<proto::msg_qangle>(base->m_viewangles);
            if (va) {
                pitch = va->m_x;
                yaw = va->m_y;
                roll = va->m_z;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { return; }

        alignas(16) uint8_t buf[64] = {};
        uint8_t* p = buf;

        uint8_t btnSize = 0;
        if (bs1) btnSize = (uint8_t)(btnSize + 9);
        if (bs2) btnSize = (uint8_t)(btnSize + 9);
        if (bs3) btnSize = (uint8_t)(btnSize + 9);

        if (btnSize > 0) {
            *p++ = 0x1a;
            *p++ = btnSize;
            if (bs1) { *p++ = 0x09; memcpy(p, &bs1, 8); p += 8; }
            if (bs2) { *p++ = 0x11; memcpy(p, &bs2, 8); p += 8; }
            if (bs3) { *p++ = 0x19; memcpy(p, &bs3, 8); p += 8; }
        }

        uint8_t vaSize = 0;
        if (pitch != 0.0f) vaSize = (uint8_t)(vaSize + 5);
        if (yaw != 0.0f)   vaSize = (uint8_t)(vaSize + 5);
        if (roll != 0.0f)  vaSize = (uint8_t)(vaSize + 5);

        if (vaSize > 0) {
            *p++ = 0x22;
            *p++ = vaSize;
            if (pitch != 0.0f) { *p++ = 0x0d; memcpy(p, &pitch, 4); p += 4; }
            if (yaw != 0.0f) { *p++ = 0x15; memcpy(p, &yaw, 4);   p += 4; }
            if (roll != 0.0f) { *p++ = 0x1d; memcpy(p, &roll, 4);  p += 4; }
        }

        int total = (int)(p - buf);

        base->m_has_bits.set(0x1u);

        uintptr_t rawMsg = reinterpret_cast<uintptr_t>(base) - kMessageImplOffset;
        uintptr_t arenaRaw = SafeReadPtr(rawMsg + 0x08);
        uintptr_t arena = arenaRaw & ~0x3ull;
        if (arenaRaw & 1) arena = SafeReadPtr(arena);

        alignas(16) uint8_t msg[0x18] = {};
        Memory::Call<void>(g_stringCopy, (uintptr_t)msg, (uintptr_t)buf, total);
        Memory::Call<void>(g_serializeMoveCrc,
            (uintptr_t)&base->m_move_crc, (uintptr_t)msg, arena);
    }

    proto::subtick_move_step* AcquireSubtickStep(
        proto::repeated_ptr_field<proto::subtick_move_step>* moves)
    {
        if (!moves) return nullptr;
        if (reinterpret_cast<uintptr_t>(moves) < 0x10000) return nullptr;

        auto* rep = moves->m_rep;
        if (rep) {
            if (reinterpret_cast<uintptr_t>(rep) >= 0x10000 &&
                moves->m_current_size >= 0 &&
                moves->m_current_size < rep->allocated_size)
            {
                void* element = rep->elements[moves->m_current_size];
                if (element && reinterpret_cast<uintptr_t>(element) >= 0x10000) {
                    moves->m_current_size++;
                    auto* step = proto::impl_ptr<proto::subtick_move_step>(element);
                    *step = {};
                    return step;
                }
            }
        }

        if (!g_subtickMoveAlloc || !g_utlVectorPush) return nullptr;

        void* moveStep = nullptr;
        __try { moveStep = Memory::Call<void*>(g_subtickMoveAlloc, moves->m_arena); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
        if (!moveStep) return nullptr;

        __try {
            Memory::Call<void>(g_utlVectorPush,
                reinterpret_cast<uintptr_t>(moves),
                reinterpret_cast<uintptr_t>(moveStep));
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }

        return proto::impl_ptr<proto::subtick_move_step>(moveStep);
    }
}