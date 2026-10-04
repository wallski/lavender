#pragma once
#include <cstdint>
#include <vector>
#include <string>

struct Vec3 {
    float x, y, z;
    Vec3() : x(0), y(0), z(0) {}
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    Vec3 operator+(const Vec3& o) const { return { x + o.x, y + o.y, z + o.z }; }
    Vec3 operator-(const Vec3& o) const { return { x - o.x, y - o.y, z - o.z }; }
    Vec3 operator*(float s) const { return { x * s, y * s, z * s }; }
    float Dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }
    float Length() const { return sqrtf(x * x + y * y + z * z); }
    float Length2D() const { return sqrtf(x * x + y * y); }
    Vec3 Normalized() const { float l = Length(); return l > 0.001f ? (*this) * (1.0f / l) : Vec3(); }
};

struct Quat {
    float x, y, z, w;
};

struct BoneData {
    Vec3  position;
    float _pad0;
    Quat  rotation;
};

namespace CombatShared {

    void  Initialize();
    void  Shutdown();

    bool  WorldToScreen(const Vec3& world, Vec3& screen);

    uintptr_t GetLocalPawn();
    uintptr_t GetLocalController();
    uintptr_t GetEntityByHandle(uint32_t handle);
    uintptr_t GetEntityByIndex(int index);
    const char* GetSchemaName(uintptr_t entity);

    bool  GetEyePosition(uintptr_t pawn, Vec3& out);
    bool  GetBoneCache(uintptr_t entity, uintptr_t& cachePtr, int& count);
    bool  GetBonePosition(uintptr_t entity, int boneId, Vec3& out);

    Vec3  CalculateAngle(const Vec3& from, const Vec3& to);
    float AngleDistance(const Vec3& a, const Vec3& b);
    void  NormalizeAngles(Vec3& angles);

    struct Enemy {
        uintptr_t controller;
        uintptr_t pawn;
        int       health;
        int       team;
        Vec3      origin;
        Vec3      headPos;
        Vec3      neckPos;
        Vec3      chestPos;
        Vec3      stomachPos;
        Vec3      pelvisPos;
        Vec3      leftArmPos;
        Vec3      rightArmPos;
        Vec3      leftLegPos;
        Vec3      rightLegPos;
        bool      visible;
        std::string name;
        std::string weaponName;
        int       ammo;
        int       maxAmmo;
        bool      hasWeapon;
    };

    std::vector<Enemy> GetEnemies();

    void BeginFrame();
    const std::vector<Enemy>& GetEnemiesCached();

    bool  IsVisible(const Vec3& start, const Vec3& end, uintptr_t skipEntity, uintptr_t targetEntity);
}