#include "pch.h"
#include "Lagcomp.h"
#include "../../../config/OffsetConfig.h"
#include "../../memory/PatternScan.h"
#include "../../systems/schemas/Schemas.h"
#include "../../cvar/CVar.h"
#include "../../math/hash.hpp"
#include <unordered_set>

namespace Lagcomp {

    static uintptr_t g_setMeshGroup = 0;
    static uintptr_t g_setSkeleton = 0;

    static std::unordered_map<uintptr_t, std::deque<Record>> g_records;
    static std::shared_mutex g_mtx;

    static uintptr_t ResolveRelCall(uintptr_t instr) {
        int32_t rel = 0;
        __try { rel = *reinterpret_cast<int32_t*>(instr + 1); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
        return instr + 5 + static_cast<intptr_t>(rel);
    }

    static bool SafeReadPtr(uintptr_t addr, uintptr_t* out) {
        __try { *out = *reinterpret_cast<uintptr_t*>(addr); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        return true;
    }

    static bool SafeReadF32(uintptr_t addr, float* out) {
        __try { *out = *reinterpret_cast<float*>(addr); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        return true;
    }

    static bool SafeReadI32(uintptr_t addr, int* out) {
        __try { *out = *reinterpret_cast<int*>(addr); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        return true;
    }

    static bool SafeReadVec3(uintptr_t addr, Vec3* out) {
        __try { *out = *reinterpret_cast<Vec3*>(addr); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        return true;
    }

    static uintptr_t GetGlobalVars() {
        uintptr_t ptr = OffsetConfig::globals.dwGlobalVars;
        if (!ptr) return 0;
        uintptr_t gv = 0;
        if (!SafeReadPtr(ptr, &gv)) return 0;
        if (gv < 0x10000) return 0;
        return gv;
    }

    bool Initialize() {
        uintptr_t mg = Memory::FindPattern("client.dll", "E8 ? ? ? ? 8B 85 2C 85 01 00");
        if (mg) g_setMeshGroup = ResolveRelCall(mg);

        uintptr_t sk = Memory::FindPattern("client.dll", "E8 ? ? ? ? 40 84 ED 74 17");
        if (sk) g_setSkeleton = ResolveRelCall(sk);

        printf("[Lagcomp] ready (meshGroup=%d skeleton=%d)\n",
            g_setMeshGroup != 0, g_setSkeleton != 0);
        fflush(stdout);

        return true;
    }

    void Shutdown() {
        std::unique_lock lock(g_mtx);
        g_records.clear();
    }

    static bool CaptureRecord(uintptr_t pawn, Record& out) {
        const uint32_t gsnOff = SCHEMA("C_BaseEntity", "m_pGameSceneNode"_hash);
        const uint32_t modelStateOff = SCHEMA("CSkeletonInstance", "m_modelState"_hash);
        const uint32_t simTimeOff = SCHEMA("C_BaseEntity", "m_flSimulationTime"_hash);
        const uint32_t absOriginOff = SCHEMA("CGameSceneNode", "m_vecAbsOrigin"_hash);
        const uint32_t absRotOff = SCHEMA("CGameSceneNode", "m_angAbsRotation"_hash);

        if (!gsnOff || !modelStateOff || !simTimeOff) return false;

        out.pawn = pawn;

        uintptr_t gsn = 0;
        if (!SafeReadPtr(pawn + gsnOff, &gsn)) return false;
        if (gsn < 0x10000) return false;
        out.gameSceneNode = gsn;

        uintptr_t boneCache = 0;
        if (!SafeReadPtr(gsn + modelStateOff + 0x80, &boneCache)) return false;
        if (boneCache < 0x10000) return false;

        int boneCount = 0;
        if (!SafeReadI32(gsn + modelStateOff + 0x8c, &boneCount)) return false;
        if (boneCount <= 0) return false;
        if (boneCount > kMaxBones) boneCount = kMaxBones;

        float simTime = 0.0f;
        if (!SafeReadF32(pawn + simTimeOff, &simTime)) return false;
        if (simTime <= 0.0f) return false;
        out.simTime = simTime;

        Vec3 absOrigin{};
        if (absOriginOff && SafeReadVec3(gsn + absOriginOff, &absOrigin)) {
            out.origin = absOrigin;
        }

        Vec3 absRot{};
        if (absRotOff && SafeReadVec3(gsn + absRotOff, &absRot)) {
            out.rotation = absRot;
        }

        out.tick = static_cast<int>(0.5f + simTime / kTickInterval);
        out.boneCache = boneCache;
        out.boneCount = boneCount;

        uintptr_t globalVars = GetGlobalVars();
        if (!globalVars) return false;

        float backupTime = 0.0f;
        int backupTick = 0;
        SafeReadF32(globalVars + 0x30, &backupTime);
        SafeReadI32(globalVars + 0x44, &backupTick);

        __try {
            *reinterpret_cast<float*>(globalVars + 0x30) = simTime;
            *reinterpret_cast<int*>(globalVars + 0x44) = out.tick;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }

        if (g_setMeshGroup) {
            __try { Memory::Call<void>(g_setMeshGroup, gsn, static_cast<uintptr_t>(0xfffff)); }
            __except (EXCEPTION_EXECUTE_HANDLER) {}
        }
        if (g_setSkeleton) {
            __try { Memory::Call<void>(g_setSkeleton, gsn, static_cast<uintptr_t>(0x100)); }
            __except (EXCEPTION_EXECUTE_HANDLER) {}
        }

        __try {
            *reinterpret_cast<float*>(globalVars + 0x30) = backupTime;
            *reinterpret_cast<int*>(globalVars + 0x44) = backupTick;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}

        uintptr_t freshCache = 0;
        if (!SafeReadPtr(gsn + modelStateOff + 0x80, &freshCache)) return false;
        if (freshCache < 0x10000) return false;
        out.boneCache = freshCache;

        __try {
            for (int i = 0; i < boneCount; i++) {
                out.bones[i] = *reinterpret_cast<BoneData*>(freshCache + i * sizeof(BoneData));
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }

        out.valid = true;
        return true;
    }

    void Run() {
        uintptr_t localPawn = CombatShared::GetLocalPawn();
        if (!localPawn) {
            std::unique_lock lock(g_mtx);
            g_records.clear();
            return;
        }

        const auto enemies = CombatShared::GetEnemiesCached();

        std::unordered_set<uintptr_t> active;
        active.reserve(enemies.size());

        std::unique_lock lock(g_mtx);

        for (const auto& e : enemies) {
            if (!e.pawn) continue;
            active.insert(e.pawn);

            float simTime = 0.0f;
            if (!SafeReadF32(e.pawn + SCHEMA("C_BaseEntity", "m_flSimulationTime"_hash), &simTime)) continue;

            int simTick = static_cast<int>(0.5f + simTime / kTickInterval);

            auto& records = g_records[e.pawn];
            if (!records.empty() && records.front().tick >= simTick) continue;

            Record rec{};
            if (CaptureRecord(e.pawn, rec)) {
                records.emplace_front(std::move(rec));
                while (static_cast<int>(records.size()) > kMaxRecords) records.pop_back();
            }
        }

        for (auto it = g_records.begin(); it != g_records.end(); ) {
            if (active.find(it->first) == active.end()) {
                it = g_records.erase(it);
            }
            else {
                ++it;
            }
        }
    }

    float GetMaxUnlag() {
        float serverLimit = CVar::GetFloat("sv_maxunlag", 0.2f);
        float playerLimit = CVar::GetFloat("sv_maxunlag_player", 0.0f);
        if (playerLimit > 0.0f && playerLimit < serverLimit) return playerLimit;
        return serverLimit;
    }

    std::vector<Record*> GetValidRecords(uintptr_t pawn) {
        std::vector<Record*> out;

        uintptr_t globalVars = GetGlobalVars();
        if (!globalVars) return out;

        float currentTime = 0.0f;
        if (!SafeReadF32(globalVars + 0x30, &currentTime)) return out;

        const float maxUnlag = GetMaxUnlag();
        const float cutoff = currentTime - maxUnlag;

        std::shared_lock lock(g_mtx);

        auto it = g_records.find(pawn);
        if (it == g_records.end()) return out;

        out.reserve(it->second.size());

        for (auto& r : it->second) {
            if (!r.valid) continue;
            if (r.simTime < cutoff) continue;
            out.push_back(&r);
        }

        return out;
    }

    void Apply(Record& r) {
        if (!r.valid || r.isApplied || !r.boneCache) return;

        __try {
            for (int i = 0; i < r.boneCount; i++) {
                r.bonesBackup[i] = *reinterpret_cast<BoneData*>(r.boneCache + i * sizeof(BoneData));
            }
            for (int i = 0; i < r.boneCount; i++) {
                *reinterpret_cast<BoneData*>(r.boneCache + i * sizeof(BoneData)) = r.bones[i];
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { return; }

        r.isApplied = true;
    }

    void Restore(Record& r) {
        if (!r.valid || !r.isApplied || !r.boneCache) return;

        __try {
            for (int i = 0; i < r.boneCount; i++) {
                *reinterpret_cast<BoneData*>(r.boneCache + i * sizeof(BoneData)) = r.bonesBackup[i];
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { return; }

        r.isApplied = false;
    }
}