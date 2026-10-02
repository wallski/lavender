#include "pch.h"
#include "CombatShared.h"
#include "../../../config/OffsetConfig.h"
#include "../../../core/memory/PatternScan.h"
#include <cmath>

namespace CombatShared {

    static uintptr_t g_traceRayFn = 0;
    static uintptr_t g_traceFilterInitFn = 0;
    static volatile LONG g_includeTeammates = 0;

    static uintptr_t ReadPtr(uintptr_t addr) {
        __try { return *(uintptr_t*)addr; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
    }

    static int ReadInt(uintptr_t addr) {
        __try { return *(int*)addr; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
    }

    static uint32_t ReadU32(uintptr_t addr) {
        __try { return *(uint32_t*)addr; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
    }

    static bool ReadBool(uintptr_t addr) {
        __try { return *(bool*)addr; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }

    static float ReadFloat(uintptr_t addr) {
        __try { return *(float*)addr; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return 0.0f; }
    }

    static Vec3 ReadVec3(uintptr_t addr) {
        __try { return *(Vec3*)addr; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return Vec3(); }
    }

    void Initialize() {
        g_traceRayFn = 0;
        g_traceFilterInitFn = Memory::FindPattern("client.dll",
            "48 89 5C 24 ?? 48 89 74 24 ?? 57 48 83 EC ?? 0F B6 41 ?? 33 FF 24");

        printf("[CombatShared] trace=disabled filter_init=0x%llX\n",
            (unsigned long long)g_traceFilterInitFn);
        fflush(stdout);
    }

    void Shutdown() {}

    void SetIncludeTeammates(bool v) {
        InterlockedExchange(&g_includeTeammates, v ? 1 : 0);
    }

    uintptr_t GetLocalPawn() {
        if (!OffsetConfig::globals.dwLocalPlayerPawn) return 0;
        return ReadPtr(OffsetConfig::globals.dwLocalPlayerPawn);
    }

    uintptr_t GetLocalController() {
        if (!OffsetConfig::globals.dwLocalPlayerController) return 0;
        return ReadPtr(OffsetConfig::globals.dwLocalPlayerController);
    }

    uintptr_t GetEntityByHandle(uint32_t handle) {
        if (!handle || handle == 0xffffffff) return 0;
        uintptr_t entityList = ReadPtr(OffsetConfig::globals.dwEntityList);
        if (!entityList) return 0;
        uintptr_t listEntry = ReadPtr(entityList + ((handle & 0x7fff) >> 9) * 8 + 0x10);
        if (!listEntry || listEntry == 0xffffffffffffffffULL) return 0;
        uintptr_t entity = ReadPtr(listEntry + (handle & 0x1ff) * 112);
        if (!entity || entity == 0xffffffffffffffffULL || entity < 0x10000) return 0;
        return entity;
    }

    uintptr_t GetEntityByIndex(int index) {
        uintptr_t entityList = ReadPtr(OffsetConfig::globals.dwEntityList);
        if (!entityList) return 0;
        uintptr_t listEntry = ReadPtr(entityList + ((uintptr_t)(index >> 9) * 8) + 0x10);
        if (!listEntry) return 0;
        uintptr_t entity = ReadPtr(listEntry + (uintptr_t)(index & 0x1ff) * 112);
        if (!entity || entity < 0x10000) return 0;
        return entity;
    }

    const char* GetSchemaName(uintptr_t entity) {
        __try {
            uintptr_t identity = *(uintptr_t*)(entity + 0x10);
            if (!identity) return nullptr;
            uintptr_t entityClass = *(uintptr_t*)(identity + 0x8);
            if (!entityClass) return nullptr;
            uintptr_t classInfo = *(uintptr_t*)(entityClass + 0x58);
            if (!classInfo) return nullptr;
            return *(const char**)(classInfo + 0x8);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return nullptr;
        }
    }

    bool GetEyePosition(uintptr_t pawn, Vec3& out) {
        if (!pawn) return false;
        uintptr_t sceneNode = ReadPtr(pawn + OffsetConfig::fields.m_pGameSceneNode);
        if (!sceneNode) return false;
        Vec3 origin = ReadVec3(sceneNode + OffsetConfig::fields.m_vecAbsOrigin);
        Vec3 viewOffset = ReadVec3(pawn + OffsetConfig::fields.m_vecViewOffset);
        out = origin + viewOffset;
        return true;
    }

    bool GetBoneCache(uintptr_t entity, uintptr_t& cachePtr, int& count) {
        cachePtr = 0;
        count = 0;
        if (!entity) return false;
        uintptr_t sceneNode = ReadPtr(entity + OffsetConfig::fields.m_pGameSceneNode);
        if (!sceneNode) return false;
        uintptr_t modelState = sceneNode + OffsetConfig::fields.m_modelState;
        cachePtr = ReadPtr(modelState + 0x80);
        count = ReadInt(modelState + 0x8c);
        if (!cachePtr || count <= 0) return false;
        if (count > 128) count = 128;
        return true;
    }

    bool GetBonePosition(uintptr_t entity, int boneId, Vec3& out) {
        uintptr_t cache = 0;
        int count = 0;
        if (!GetBoneCache(entity, cache, count)) return false;
        if (boneId < 0 || boneId >= count) return false;
        __try {
            out = *(Vec3*)(cache + boneId * sizeof(BoneData));
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    void NormalizeAngles(Vec3& a) {
        while (a.x > 89.0f) a.x -= 180.0f;
        while (a.x < -89.0f) a.x += 180.0f;
        while (a.y > 180.0f) a.y -= 360.0f;
        while (a.y < -180.0f) a.y += 360.0f;
        a.z = 0.0f;
    }

    Vec3 CalculateAngle(const Vec3& from, const Vec3& to) {
        Vec3 delta = to - from;
        float hyp = sqrtf(delta.x * delta.x + delta.y * delta.y);
        Vec3 angles;
        angles.x = -atan2f(delta.z, hyp) * (180.0f / 3.14159265359f);
        angles.y = atan2f(delta.y, delta.x) * (180.0f / 3.14159265359f);
        angles.z = 0.0f;
        return angles;
    }

    float AngleDistance(const Vec3& a, const Vec3& b) {
        Vec3 d = a - b;
        NormalizeAngles(d);
        return sqrtf(d.x * d.x + d.y * d.y);
    }

    bool WorldToScreen(const Vec3& world, Vec3& screen) {
        uintptr_t matrixPtr = OffsetConfig::globals.dwViewMatrix;
        if (!matrixPtr) return false;

        __try {
            float* m = (float*)matrixPtr;

            float x_ndc = m[0] * world.x + m[1] * world.y + m[2] * world.z + m[3];
            float y_ndc = m[4] * world.x + m[5] * world.y + m[6] * world.z + m[7];
            float w = m[12] * world.x + m[13] * world.y + m[14] * world.z + m[15];

            if (w < 0.001f) return false;

            RECT rect;
            HWND hwnd = GetForegroundWindow();
            if (!hwnd || !GetClientRect(hwnd, &rect)) return false;

            float sw = (float)(rect.right - rect.left);
            float sh = (float)(rect.bottom - rect.top);
            float invW = 1.0f / w;

            screen.x = sw * 0.5f * (1.0f + x_ndc * invW);
            screen.y = sh * 0.5f * (1.0f - y_ndc * invW);
            screen.z = 0.0f;
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    bool IsVisible(const Vec3& start, const Vec3& end, uintptr_t skipEntity, uintptr_t targetEntity) {
        (void)start; (void)end; (void)skipEntity; (void)targetEntity;
        return true;
    }

    std::vector<Enemy> GetEnemies() {
        std::vector<Enemy> out;

        uintptr_t localPawn = GetLocalPawn();
        if (!localPawn) return out;

        int localTeam = ReadInt(localPawn + OffsetConfig::fields.m_iTeamNum);
        const bool includeTeammates = (g_includeTeammates != 0);

        for (int i = 0; i < 2048; i++) {
            uintptr_t entity = GetEntityByIndex(i);
            if (!entity) continue;

            const char* name = GetSchemaName(entity);
            if (!name) continue;
            if (strcmp(name, "C_CSPlayerPawn") != 0) continue;
            if (entity == localPawn) continue;

            int health = ReadInt(entity + OffsetConfig::fields.m_iHealth);
            if (health <= 0) continue;

            int team = ReadInt(entity + OffsetConfig::fields.m_iTeamNum);
            if (!includeTeammates && team == localTeam) continue;

            Enemy e{};
            e.pawn = entity;
            e.controller = 0;
            e.health = health;
            e.team = team;

            uintptr_t sceneNode = ReadPtr(entity + OffsetConfig::fields.m_pGameSceneNode);
            if (sceneNode) {
                e.origin = ReadVec3(sceneNode + OffsetConfig::fields.m_vecAbsOrigin);
            }

            GetBonePosition(entity, 7, e.headPos);
            GetBonePosition(entity, 6, e.neckPos);
            GetBonePosition(entity, 3, e.chestPos);
            GetBonePosition(entity, 2, e.stomachPos);
            GetBonePosition(entity, 1, e.pelvisPos);
            GetBonePosition(entity, 9, e.leftArmPos);
            GetBonePosition(entity, 13, e.rightArmPos);
            GetBonePosition(entity, 17, e.leftLegPos);
            GetBonePosition(entity, 20, e.rightLegPos);

            Vec3 eye;
            if (GetEyePosition(localPawn, eye)) {
                e.visible = IsVisible(eye, e.headPos, localPawn, entity);
            }

            out.push_back(e);
        }

        return out;
    }
}