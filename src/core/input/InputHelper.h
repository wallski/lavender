#pragma once
#include <cstdint>
#include "../memory/Proto.h"

struct UsercmdLayout;

namespace InputHelper {

    bool Initialize();
    void Shutdown();

    void RecalculateCrc(UsercmdLayout* cmd);

    uintptr_t EnsureButtons(proto::base_usercmd_pb* base);

    proto::subtick_move_step* AcquireSubtickStep(
        proto::repeated_ptr_field<proto::subtick_move_step>* moves);
}