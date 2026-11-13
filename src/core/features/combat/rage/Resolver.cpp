#include "pch.h"
#include "Resolver.h"
#include "../CombatShared.h"
#include "../../../../config/OffsetConfig.h"
#include "../../../math/math.hpp"
#include <cmath>
#include <algorithm>

namespace Resolver {
    static float SafeReadFloat(uintptr_t addr) {
        __try { return *reinterpret_cast<float*>(addr); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return 0.0f; }
    }

    static int SafeReadInt(uintptr_t addr) {
        __try { return *reinterpret_cast<int*>(addr); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
    }

    void DetectSide(uintptr_t player, int* side) {
        uintptr_t localPawn = CombatShared::GetLocalPawn();
        if (!localPawn) return;

        Vec3 localOrigin;
        uintptr_t localSceneNode = 0;
        __try { localSceneNode = *reinterpret_cast<uintptr_t*>(localPawn + OffsetConfig::fields.m_pGameSceneNode); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return; }
        if (!localSceneNode) return;
        __try { localOrigin = *reinterpret_cast<Vec3*>(localSceneNode + OffsetConfig::fields.m_vecAbsOrigin); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return; }

        uintptr_t sceneNode = 0;
        __try { sceneNode = *reinterpret_cast<uintptr_t*>(player + OffsetConfig::fields.m_pGameSceneNode); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return; }
        if (!sceneNode) return;

        Vec3 playerOrigin;
        __try { playerOrigin = *reinterpret_cast<Vec3*>(sceneNode + OffsetConfig::fields.m_vecAbsOrigin); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return; }

        Vec3 delta = playerOrigin - localOrigin;
        float yaw = atan2f(delta.y, delta.x) * (180.0f / 3.14159265f);

        int team = SafeReadInt(player + OffsetConfig::fields.m_iTeamNum);

        if (team == 2) *side = 1;
        else *side = -1;
    }

    float BuildServerAbsYaw(uintptr_t player, float angle) {
        float velocityX = 0.0f, velocityY = 0.0f;
        __try {
            velocityX = *reinterpret_cast<float*>(player + 0x3F8);
            velocityY = *reinterpret_cast<float*>(player + 0x3FC);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}

        float speed = sqrtf(velocityX * velocityX + velocityY * velocityY);
        float m_flGoalFeetYaw = angle;

        if (speed > 0.1f) {
            m_flGoalFeetYaw = angle;
        }
        else {
            float lowerBodyYaw = SafeReadFloat(player + 0x4154);
            float delta = angle - lowerBodyYaw;
            while (delta > 180.0f) delta -= 360.0f;
            while (delta < -180.0f) delta += 360.0f;
            m_flGoalFeetYaw = angle - delta * 0.5f;
        }

        return m_flGoalFeetYaw;
    }

    void ResolveAngles(uintptr_t player, Lagcomp::Record* record, int missed_shots, int* side) {
        if (!record) return;

        uintptr_t animState = 0;
        __try { animState = *reinterpret_cast<uintptr_t*>(player + 0x98); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return; }

        float eyeYaw = SafeReadFloat(player + OffsetConfig::fields.m_angEyeAngles + 4);

        float maxRotation = 58.0f;
        if (animState) {
            float maxDesync = SafeReadFloat(animState + 0x104);
            if (maxDesync > 0.0f) maxRotation = maxDesync;
        }

        float bruteYaw = eyeYaw;

        switch (missed_shots % 3) {
        case 0:
            bruteYaw = eyeYaw + (maxRotation * *side);
            break;
        case 1:
            bruteYaw = eyeYaw + (maxRotation * -*side);
            break;
        case 2:
            bruteYaw = eyeYaw;
            break;
        }

        if (record->boneCount > 0) {
            math::quaternion q = math::quaternion::from_euler(math::vector3{ 0.0f, bruteYaw, 0.0f });
            Quat rot;
            rot.x = q.x;
            rot.y = q.y;
            rot.z = q.z;
            rot.w = q.w;

            for (int i = 0; i < record->boneCount; i++) {
                record->bones[i].rotation = rot;
            }
        }
    }
}