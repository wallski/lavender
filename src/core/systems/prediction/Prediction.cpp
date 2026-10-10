#include "pch.h"
#include "Prediction.h"
#include "../schemas/Schemas.h"
#include "../../math/hash.hpp"

namespace systems {

    static std::uintptr_t SafeReadPtr(std::uintptr_t addr) {
        __try { return *reinterpret_cast<std::uintptr_t*>(addr); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
    }

    static std::uint32_t SafeReadU32(std::uintptr_t addr) {
        __try { return *reinterpret_cast<std::uint32_t*>(addr); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
        return 0;
    }

    static float SafeReadFloat(std::uintptr_t addr) {
        __try { return *reinterpret_cast<float*>(addr); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return 0.0f; }
        return 0.0f;
    }

    static math::vector3 SafeReadVec3(std::uintptr_t addr) {
        math::vector3 v{};
        __try {
            v.x = *reinterpret_cast<float*>(addr + 0);
            v.y = *reinterpret_cast<float*>(addr + 4);
            v.z = *reinterpret_cast<float*>(addr + 8);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
        return v;
    }

    static bool SafeInvoke(const std::function<void()>* fn) {
        __try {
            (*fn)();
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    void prediction::capture_prestate(std::uintptr_t local_pawn, std::uintptr_t movement_services) {
        if (!local_pawn) {
            this->m_prestate = {};
            return;
        }

        state s{};

        s.flags = SafeReadU32(local_pawn + SCHEMA("C_BaseEntity", "m_fFlags"_hash));

        s.velocity = SafeReadVec3(local_pawn + SCHEMA("C_BaseEntity", "m_vecVelocity"_hash));
        s.networked_velocity = SafeReadVec3(local_pawn + SCHEMA("C_BaseEntity", "m_vecAbsVelocity"_hash));

        const auto game_scene_node = SafeReadPtr(local_pawn + SCHEMA("C_BaseEntity", "m_pGameSceneNode"_hash));
        if (game_scene_node) {
            const auto abs_origin = SafeReadVec3(game_scene_node + SCHEMA("CGameSceneNode", "m_vecAbsOrigin"_hash));
            s.origin = abs_origin;
            s.networked_origin = abs_origin;
        }

        if (movement_services) {
            s.last_movement_impulses = SafeReadVec3(movement_services + SCHEMA("CPlayer_MovementServices", "m_vecLastMovementImpulses"_hash));
            s.surface_friction = SafeReadFloat(movement_services + SCHEMA("CPlayer_MovementServices_Humanoid", "m_flSurfaceFriction"_hash));
            s.stamina = SafeReadFloat(movement_services + SCHEMA("CCSPlayer_MovementServices", "m_flStamina"_hash));
        }

        this->m_prestate = s;
    }

    bool prediction::simulate(void* cmd, const void* local_snapshot, const std::function<void()>& fn) {
        (void)cmd;
        (void)local_snapshot;
        if (!fn) return false;

        std::lock_guard lock(this->m_simulation_mtx);

        const auto saved = this->m_prestate;

        const bool ok = SafeInvoke(&fn);

        this->m_prestate = saved;
        return ok;
    }

}