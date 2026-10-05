#include "pch.h"
#include "CombatShared.h"
#include "../../../config/OffsetConfig.h"
#include "../../../core/memory/PatternScan.h"
#include <cmath>

namespace CombatShared {

    static std::vector<Enemy> g_enemyCache;
    static volatile LONG g_frameId = 0;
    static volatile LONG g_cacheFrame = -1;

    static constexpr uintptr_t kPtrMin = 0x0000000000010000;
    static constexpr uintptr_t kPtrMax = 0x00007FFFFFFFFFFF;
    static constexpr int kMaxEntityIndex = 4096;
    static constexpr int kMaxPlayers = 64;
    static constexpr int kMaxBones = 32;

    static inline bool IsUserPtr(uintptr_t p) {
        return p >= kPtrMin && p <= kPtrMax;
    }

    static bool SafeReadBone(Vec3* dst, uintptr_t src) {
        __try {
            *dst = *reinterpret_cast<Vec3*>(src);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    static bool SafeReadU8(uintptr_t addr, uint8_t* out) {
        *out = 0;
        __try {
            *out = *reinterpret_cast<uint8_t*>(addr);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    static bool SafeReadU64(uintptr_t addr, uint64_t* out) {
        *out = 0;
        __try {
            *out = *reinterpret_cast<uint64_t*>(addr);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    static bool SafeReadI32(uintptr_t addr, int32_t* out) {
        *out = 0;
        __try {
            *out = *reinterpret_cast<int32_t*>(addr);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    static uintptr_t ReadPtr(uintptr_t addr) {
        uint64_t v = 0;
        SafeReadU64(addr, &v);
        return static_cast<uintptr_t>(v);
    }

    static int ReadInt(uintptr_t addr) {
        int32_t v = 0;
        SafeReadI32(addr, &v);
        return static_cast<int>(v);
    }

    static bool ReadBool(uintptr_t addr) {
        uint8_t v = 0;
        SafeReadU8(addr, &v);
        return v != 0;
    }

    static Vec3 ReadVec3(uintptr_t addr) {
        Vec3 v;
        SafeReadBone(&v, addr);
        return v;
    }

    void Initialize() {}

    void Shutdown() {}

    uintptr_t GetLocalPawn() {
        if (!OffsetConfig::globals.dwLocalPlayerPawn) return 0;
        uintptr_t p = ReadPtr(OffsetConfig::globals.dwLocalPlayerPawn);
        if (!IsUserPtr(p)) return 0;

        const uintptr_t sn = ReadPtr(p + OffsetConfig::fields.m_pGameSceneNode);
        if (!IsUserPtr(sn)) return 0;

        return p;
    }

    uintptr_t GetLocalController() {
        if (!OffsetConfig::globals.dwLocalPlayerController) return 0;
        uintptr_t p = ReadPtr(OffsetConfig::globals.dwLocalPlayerController);
        return IsUserPtr(p) ? p : 0;
    }

    uintptr_t GetEntityByHandle(uint32_t handle) {
        if (!handle || handle == 0xffffffff) return 0;
        uintptr_t entityList = ReadPtr(OffsetConfig::globals.dwEntityList);
        if (!IsUserPtr(entityList)) return 0;
        uintptr_t listEntry = ReadPtr(entityList + ((handle & 0x7fff) >> 9) * 8 + 0x10);
        if (!IsUserPtr(listEntry)) return 0;
        uintptr_t entity = ReadPtr(listEntry + (handle & 0x1ff) * 112);
        if (!IsUserPtr(entity)) return 0;
        return entity;
    }

    uintptr_t GetEntityByIndex(int index) {
        uintptr_t entityList = ReadPtr(OffsetConfig::globals.dwEntityList);
        if (!IsUserPtr(entityList)) return 0;
        uintptr_t listEntry = ReadPtr(entityList + ((uintptr_t)(index >> 9) * 8) + 0x10);
        if (!IsUserPtr(listEntry)) return 0;
        uintptr_t entity = ReadPtr(listEntry + (uintptr_t)(index & 0x1ff) * 112);
        if (!IsUserPtr(entity)) return 0;
        return entity;
    }

    const char* GetSchemaName(uintptr_t entity) {
        if (!IsUserPtr(entity)) return nullptr;
        __try {
            uintptr_t identity = *(uintptr_t*)(entity + 0x10);
            if (!IsUserPtr(identity)) return nullptr;
            uintptr_t entityClass = *(uintptr_t*)(identity + 0x8);
            if (!IsUserPtr(entityClass)) return nullptr;
            uintptr_t classInfo = *(uintptr_t*)(entityClass + 0x58);
            if (!IsUserPtr(classInfo)) return nullptr;
            const char* namePtr = *(const char**)(classInfo + 0x8);
            if (!IsUserPtr((uintptr_t)namePtr)) return nullptr;
            return namePtr;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return nullptr;
        }
    }

    bool GetEyePosition(uintptr_t pawn, Vec3& out) {
        if (!IsUserPtr(pawn)) return false;
        uintptr_t sceneNode = ReadPtr(pawn + OffsetConfig::fields.m_pGameSceneNode);
        if (!IsUserPtr(sceneNode)) return false;
        Vec3 origin = ReadVec3(sceneNode + OffsetConfig::fields.m_vecAbsOrigin);
        Vec3 viewOffset = ReadVec3(pawn + OffsetConfig::fields.m_vecViewOffset);
        out = origin + viewOffset;
        return true;
    }

    bool GetBoneCache(uintptr_t entity, uintptr_t& cachePtr, int& count) {
        cachePtr = 0;
        count = 0;
        if (!IsUserPtr(entity)) return false;
        uintptr_t sceneNode = ReadPtr(entity + OffsetConfig::fields.m_pGameSceneNode);
        if (!IsUserPtr(sceneNode)) return false;
        uintptr_t modelState = sceneNode + OffsetConfig::fields.m_modelState;
        cachePtr = ReadPtr(modelState + 0x80);
        count = ReadInt(modelState + 0x8c);
        if (!IsUserPtr(cachePtr) || count <= 0) return false;
        if (count > 128) count = 128;
        return true;
    }

    bool GetBonePosition(uintptr_t entity, int boneId, Vec3& out) {
        uintptr_t cache = 0;
        int count = 0;
        if (!GetBoneCache(entity, cache, count)) return false;
        if (boneId < 0 || boneId >= count) return false;
        return SafeReadBone(&out, cache + boneId * sizeof(BoneData));
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

    static bool ReadTeamRaw(uintptr_t entity, int* out) {
        if (!IsUserPtr(entity)) return false;
        __try {
            *out = *reinterpret_cast<int*>(entity + OffsetConfig::fields.m_iTeamNum);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    static bool ReadHealthRaw(uintptr_t entity, int* out) {
        if (!IsUserPtr(entity)) return false;
        __try {
            *out = *reinterpret_cast<int*>(entity + OffsetConfig::fields.m_iHealth);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    static bool ReadOriginRaw(uintptr_t pawn, float out[3]) {
        if (!IsUserPtr(pawn)) return false;
        __try {
            uintptr_t sceneNode = *reinterpret_cast<uintptr_t*>(
                pawn + OffsetConfig::fields.m_pGameSceneNode);
            if (!IsUserPtr(sceneNode)) return false;
            out[0] = *reinterpret_cast<float*>(sceneNode + OffsetConfig::fields.m_vecAbsOrigin + 0);
            out[1] = *reinterpret_cast<float*>(sceneNode + OffsetConfig::fields.m_vecAbsOrigin + 4);
            out[2] = *reinterpret_cast<float*>(sceneNode + OffsetConfig::fields.m_vecAbsOrigin + 8);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    static bool ReadPawnHandleRaw(uintptr_t controller, uint32_t* out) {
        if (!IsUserPtr(controller)) return false;
        __try {
            *out = *reinterpret_cast<uint32_t*>(controller + OffsetConfig::fields.m_hPlayerPawn);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    static bool SafeReadStringPtr(uintptr_t addr, uintptr_t* out) {
        *out = 0;
        __try {
            *out = *reinterpret_cast<uintptr_t*>(addr);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    static bool SafeReadChar(uintptr_t addr, char* out) {
        *out = '\0';
        __try {
            *out = *reinterpret_cast<const char*>(addr);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    static bool ReadControllerNameRaw(uintptr_t controller, char* out, size_t outSize) {
        out[0] = '\0';
        if (!IsUserPtr(controller)) return false;

        const uintptr_t field = controller + OffsetConfig::fields.m_sSanitizedPlayerName;

        uintptr_t strPtr = 0;
        if (!SafeReadStringPtr(field, &strPtr) || !IsUserPtr(strPtr)) {
            return false;
        }

        size_t i = 0;
        while (i < outSize - 1 && i < 127) {
            char c = '\0';
            if (!SafeReadChar(strPtr + i, &c)) return false;
            if (c == '\0') break;
            out[i] = c;
            i++;
        }
        out[i] = '\0';
        return out[0] != '\0';
    }

    static bool ReadActiveWeaponHandleRaw(uintptr_t pawn, uint32_t* out) {
        if (!IsUserPtr(pawn)) return false;
        __try {
            uintptr_t services = *reinterpret_cast<uintptr_t*>(
                pawn + OffsetConfig::fields.m_pWeaponServices);
            if (!IsUserPtr(services)) return false;
            *out = *reinterpret_cast<uint32_t*>(services + OffsetConfig::fields.m_hActiveWeapon);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    static bool ReadWeaponDataRaw(uintptr_t weapon, char* nameOut, size_t nameSize,
        int* ammo, int* maxAmmo) {
        nameOut[0] = '\0';
        *ammo = 0;
        *maxAmmo = 0;
        if (!IsUserPtr(weapon)) return false;
        __try {
            *ammo = *reinterpret_cast<int*>(weapon + OffsetConfig::fields.m_iClip1);

            uintptr_t vdata = *reinterpret_cast<uintptr_t*>(
                weapon + OffsetConfig::fields.m_nSubclassID + 0x8);
            if (!IsUserPtr(vdata)) return false;

            *maxAmmo = *reinterpret_cast<int*>(vdata + OffsetConfig::fields.m_iMaxClip1);

            const char* sz = *reinterpret_cast<const char**>(vdata + OffsetConfig::fields.m_szName);
            if (!IsUserPtr((uintptr_t)sz)) return false;

            size_t i = 0;
            while (i < nameSize - 1 && sz[i] != '\0') {
                nameOut[i] = sz[i];
                i++;
            }
            nameOut[i] = '\0';
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    struct ControllerRecord {
        uintptr_t entity;
        uintptr_t pawn;
        char name[128];
    };

    static std::vector<Enemy> BuildEnemies() {
        std::vector<Enemy> out;

        uintptr_t localPawn = GetLocalPawn();
        if (!localPawn) return out;

        const bool readWeapon = (OffsetConfig::fields.m_pWeaponServices != 0 &&
            OffsetConfig::fields.m_hActiveWeapon != 0 &&
            OffsetConfig::fields.m_iClip1 != 0 &&
            OffsetConfig::fields.m_iMaxClip1 != 0 &&
            OffsetConfig::fields.m_szName != 0 &&
            OffsetConfig::fields.m_nSubclassID != 0);

        ControllerRecord controllers[kMaxPlayers];
        int ctrlCount = 0;

        for (int i = 0; i < kMaxEntityIndex && ctrlCount < kMaxPlayers; i++) {
            uintptr_t entity = GetEntityByIndex(i);
            if (!entity) continue;

            const char* schema = GetSchemaName(entity);
            if (!schema) continue;
            if (strstr(schema, "PlayerController") == nullptr) continue;

            uint32_t pawnHandle = 0;
            if (!ReadPawnHandleRaw(entity, &pawnHandle)) continue;
            if (!pawnHandle || pawnHandle == 0xffffffff) continue;

            uintptr_t pawn = GetEntityByHandle(pawnHandle);
            if (!IsUserPtr(pawn)) continue;

            controllers[ctrlCount].entity = entity;
            controllers[ctrlCount].pawn = pawn;
            controllers[ctrlCount].name[0] = '\0';
            ReadControllerNameRaw(entity, controllers[ctrlCount].name, sizeof(controllers[ctrlCount].name));
            ctrlCount++;
        }

        for (int i = 0; i < kMaxEntityIndex; i++) {
            uintptr_t entity = GetEntityByIndex(i);
            if (!entity) continue;

            const char* schema = GetSchemaName(entity);
            if (!schema) continue;
            if (strstr(schema, "CSPlayerPawn") == nullptr) continue;
            if (entity == localPawn) continue;

            int health = 0;
            ReadHealthRaw(entity, &health);
            if (health <= 0) continue;

            int team = 0;
            ReadTeamRaw(entity, &team);

            Enemy e{};
            e.pawn = entity;
            e.health = health;
            e.team = team;

            for (int j = 0; j < ctrlCount; j++) {
                if (controllers[j].pawn == entity) {
                    e.controller = controllers[j].entity;
                    if (controllers[j].name[0] != '\0') {
                        e.name.assign(controllers[j].name);
                    }
                    break;
                }
            }

            float origin[3] = { 0.0f, 0.0f, 0.0f };
            if (ReadOriginRaw(entity, origin)) {
                e.origin = Vec3(origin[0], origin[1], origin[2]);
            }

            uintptr_t boneCache = 0;
            int boneCount = 0;
            if (GetBoneCache(entity, boneCache, boneCount)) {
                const int readCount = (boneCount < kMaxBones) ? boneCount : kMaxBones;
                for (int b = 0; b < readCount; b++) {
                    if (!SafeReadBone(&e.bones[b], boneCache + b * sizeof(BoneData))) {
                        e.bones[b] = Vec3();
                    }
                }
                for (int b = readCount; b < kMaxBones; b++) {
                    e.bones[b] = Vec3();
                }
            }

            e.headPos = e.bones[7];
            e.neckPos = e.bones[6];
            e.chestPos = e.bones[3];
            e.stomachPos = e.bones[2];
            e.pelvisPos = e.bones[1];
            e.leftArmPos = e.bones[9];
            e.rightArmPos = e.bones[13];
            e.leftLegPos = e.bones[17];
            e.rightLegPos = e.bones[20];

            if (readWeapon) {
                uint32_t weaponHandle = 0;
                if (ReadActiveWeaponHandleRaw(entity, &weaponHandle) && weaponHandle) {
                    uintptr_t weapon = GetEntityByHandle(weaponHandle);
                    if (weapon) {
                        char wName[64];
                        int ammo = 0;
                        int maxAmmo = 0;
                        if (ReadWeaponDataRaw(weapon, wName, sizeof(wName), &ammo, &maxAmmo)) {
                            e.weaponName.assign(wName);
                            e.ammo = ammo;
                            e.maxAmmo = maxAmmo;
                            e.hasWeapon = true;
                        }
                    }
                }
            }

            Vec3 eye;
            if (GetEyePosition(localPawn, eye)) {
                e.visible = IsVisible(eye, e.headPos, localPawn, entity);
            }

            out.push_back(std::move(e));
        }

        return out;
    }

    void BeginFrame() {
        InterlockedIncrement(&g_frameId);
    }

    const std::vector<Enemy>& GetEnemiesCached() {
        const LONG cur = g_frameId;
        if (cur != g_cacheFrame) {
            g_enemyCache = BuildEnemies();
            InterlockedExchange(&g_cacheFrame, cur);
        }
        return g_enemyCache;
    }
}