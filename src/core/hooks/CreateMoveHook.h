#pragma once
#include <cstdint>

struct UsercmdLayout;

namespace CreateMoveHook {

    using CallbackFn = void(*)(void* cmd);

    bool Initialize();
    void Shutdown();

    void Register(CallbackFn fn);

    UsercmdLayout* GetCurrentUsercmd();
}