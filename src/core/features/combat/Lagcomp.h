#pragma once
#include <cstdint>
#include <array>
#include <vector>
#include <deque>
#include <unordered_map>
#include <shared_mutex>
#include "CombatShared.h"

namespace Lagcomp {

    constexpr int kMaxBones = 128;
    constexpr int kMaxRecords = 16;
    constexpr float kTickInterval = 0.015625f;

    struct Record {
        uintptr_t pawn = 0;
        uintptr_t gameSceneNode = 0;
        uintptr_t boneCache = 0;
        int boneCount = 0;

        std::array<BoneData, kMaxBones> bones{};
        std::array<BoneData, kMaxBones> bonesBackup{};

        Vec3 origin{};
        Vec3 rotation{};
        float simTime = 0.0f;
        int tick = 0;

        bool valid = false;
        bool wasValid = false;
        bool isApplied = false;
    };

    struct RenderSnapshot {
        std::array<Vec3, 22> bonePositions{};
        int boneCount = 0;
        int tick = 0;
    };

    bool Initialize();
    void Shutdown();

    void Run();

    Record* GetOldestValid(uintptr_t pawn);
    Record* GetOldestWasValid(uintptr_t pawn);
    std::vector<Record*> GetValidRecords(uintptr_t pawn);
    std::vector<RenderSnapshot> GetRenderSnapshots(uintptr_t pawn);

    void Apply(Record& r);
    void Restore(Record& r);

    float GetMaxUnlag();

    bool IsEnabled();
    void SetEnabled(bool enabled);

    int GetMaxTicks();
    void SetMaxTicks(int ticks);
}