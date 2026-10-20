#include "pch.h"
#include "Lagcomp.h"
#include "../../../config/OffsetConfig.h"
#include "../../memory/PatternScan.h"
#include "../../systems/schemas/Schemas.h"
#include "../../cvar/CVar.h"
#include "../../math/hash.hpp"
#include "../../hooks/CreateMoveHook.h"
#include <unordered_set>
#include <cmath>
#include <algorithm>

#define LAGCOMP_VERBOSE_LOG 0

namespace Lagcomp {

    static uintptr_t g_setMeshGroup = 0;
    static uintptr_t g_setSkeleton = 0;
    static uintptr_t g_getNetChannelFn = 0;

    static std::unordered_map<uintptr_t, std::deque<Record>> g_records;
    static std::shared_mutex g_mtx;

    static volatile LONG g_enabled = 1;
    static volatile LONG g_maxTicks = 12;

    static float g_cachedLatency = 0.0f;
    static float g_cachedMaxUnlag = 0.2f;

    void OnCreateMove(void* cmd);

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
        __try {
            out->x = *reinterpret_cast<float*>(addr + 0);
            out->y = *reinterpret_cast<float*>(addr + 4);
            out->z = *reinterpret_cast<float*>(addr + 8);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        return true;
    }

    static uintptr_t ResolveRelCall(uintptr_t instr) {
        int32_t rel = 0;
        __try { rel = *reinterpret_cast<int32_t*>(instr + 1); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
        return instr + 5 + static_cast<intptr_t>(rel);
    }

    static uintptr_t GetGlobalVars() {
        uintptr_t ptr = OffsetConfig::globals.dwGlobalVars;
        if (!ptr) return 0;
        uintptr_t gv = 0;
        if (!SafeReadPtr(ptr, &gv)) return 0;
        if (gv < 0x10000) return 0;
        return gv;
    }

    static float GetCurtime() {
        uintptr_t gv = GetGlobalVars();
        if (!gv) return 0.0f;
        float t = 0.0f;
        if (!SafeReadF32(gv + 0x30, &t)) return 0.0f;
        if (!(t > 0.0f) || !(t < 1000000.0f)) return 0.0f;
        return t;
    }

    static uintptr_t ResolveNetChannel() {
        return Memory::FindPattern("engine2.dll", "4C 8B 05 ? ? ? ? 4D 85 C0 74 10");
    }

    static uintptr_t GetNetChannel() {
        if (!g_getNetChannelFn) return 0;

        using Fn = uintptr_t(*)(int, int);

        uintptr_t channel = 0;
        __try { channel = ((Fn)g_getNetChannelFn)(0, 0); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }

        if (channel < 0x10000 || channel > 0x00007FFFFFFFFFFFULL) return 0;
        return channel;
    }

    static float QueryLatency() {
        uintptr_t channel = GetNetChannel();
        if (!channel) return 0.0f;

        using LatencyFn = float(*)(uintptr_t, int);

        void** vtable = nullptr;
        __try { vtable = *reinterpret_cast<void***>(channel); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return 0.0f; }

        if (!vtable) return 0.0f;

        float latency = 0.0f;
        __try { latency = ((LatencyFn)vtable[10])(channel, 0); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return 0.0f; }

        if (!(latency >= 0.0f) || !(latency <= 1.0f)) return 0.0f;
        return latency;
    }

    static float ComputeMaxUnlag() {
        float serverLimit = CVar::GetFloat("sv_maxunlag", 0.2f);
        float playerLimit = CVar::GetFloat("sv_maxunlag_player", 0.0f);
        if (playerLimit > 0.0f && playerLimit < serverLimit) return playerLimit;
        return serverLimit;
    }

    static void RefreshCache() {
        g_cachedLatency = QueryLatency();
        g_cachedMaxUnlag = ComputeMaxUnlag();
    }

    static bool IsRecordValid(const Record& r, float latency, float maxUnlag) {
        if (!r.valid) return false;

        const float currentTime = GetCurtime();
        if (currentTime <= 0.0f) return false;

        const float budget = maxUnlag - (latency > 0.0f ? latency : 0.0f);
        if (budget <= 0.0f) return false;

        return r.simTime >= currentTime - budget;
    }

    static bool IsRecordValid(const Record& r) {
        return IsRecordValid(r, g_cachedLatency, g_cachedMaxUnlag);
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
        if (!(simTime > 0.0f) || !(simTime < 1000000.0f)) return false;
        out.simTime = simTime;

        if (absOriginOff) {
            SafeReadVec3(gsn + absOriginOff, &out.origin);
        }
        if (absRotOff) {
            SafeReadVec3(gsn + absRotOff, &out.rotation);
        }

        out.tick = static_cast<int>(0.5f + simTime / kTickInterval);
        out.boneCache = boneCache;
        out.boneCount = boneCount;

        uintptr_t globalVars = GetGlobalVars();
        if (!globalVars) return false;

        float backupTime = 0.0f;
        int backupTick = 0;

        __try {
            backupTime = *reinterpret_cast<float*>(globalVars + 0x30);
            backupTick = *reinterpret_cast<int*>(globalVars + 0x44);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}

        __try {
            *reinterpret_cast<float*>(globalVars + 0x30) = simTime;
            *reinterpret_cast<int*>(globalVars + 0x44) = out.tick;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }

        if (g_setMeshGroup) {
            using Fn = void(*)(uintptr_t, uintptr_t);
            __try { ((Fn)g_setMeshGroup)(gsn, static_cast<uintptr_t>(0xfffff)); }
            __except (EXCEPTION_EXECUTE_HANDLER) {}
        }

        if (g_setSkeleton) {
            using Fn = void(*)(uintptr_t, uintptr_t);
            __try { ((Fn)g_setSkeleton)(gsn, static_cast<uintptr_t>(0x100)); }
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

    static void CollectRecords() {
        if (g_enabled == 0) return;

        RefreshCache();

        uintptr_t localPawn = CombatShared::GetLocalPawn();
        if (!localPawn) {
            std::unique_lock lock(g_mtx);
            g_records.clear();
            return;
        }

        const auto enemies = CombatShared::GetEnemiesCached();

        std::unordered_set<uintptr_t> active;
        active.reserve(enemies.size());
        for (const auto& e : enemies) {
            if (e.pawn) active.insert(e.pawn);
        }

        std::unique_lock lock(g_mtx);

        for (auto it = g_records.begin(); it != g_records.end();) {
            if (active.find(it->first) == active.end()) {
                it = g_records.erase(it);
            }
            else {
                ++it;
            }
        }

        for (const auto& e : enemies) {
            if (!e.pawn) continue;

            float simTime = 0.0f;
            if (!SafeReadF32(e.pawn + SCHEMA("C_BaseEntity", "m_flSimulationTime"_hash), &simTime)) continue;
            if (simTime <= 0.0f) continue;

            int simTick = static_cast<int>(0.5f + simTime / kTickInterval);

            auto& records = g_records[e.pawn];

            while (!records.empty() && !IsRecordValid(records.back())) {
                records.pop_back();
            }

            if (!records.empty() && records.front().tick >= simTick) continue;

            Record rec{};
            if (CaptureRecord(e.pawn, rec)) {
                records.emplace_front(std::move(rec));
                while (static_cast<int>(records.size()) > kMaxRecords) {
                    records.pop_back();
                }
            }
        }

        for (auto& entry : g_records) {
            auto& records = entry.second;
            for (auto& rec : records) {
                rec.wasValid = IsRecordValid(rec);
            }
        }
    }

    void OnCreateMove(void* cmd) {
        (void)cmd;
        CollectRecords();

#if LAGCOMP_VERBOSE_LOG
        static ULONGLONG s_lastLog = 0;
        ULONGLONG now = GetTickCount64();
        if (now - s_lastLog >= 1000) {
            s_lastLog = now;

            std::shared_lock lock(g_mtx);
            size_t total = 0;
            int maxSize = 0;
            for (auto& entry : g_records) {
                total += entry.second.size();
                if ((int)entry.second.size() > maxSize) maxSize = (int)entry.second.size();
            }

            printf("[Lagcomp] %zu recs / %zu enemies (max %d) lat=%.1fms bud=%.1fms\n",
                total, g_records.size(), maxSize,
                g_cachedLatency * 1000.0f,
                (g_cachedMaxUnlag - (g_cachedLatency > 0.0f ? g_cachedLatency : 0.0f)) * 1000.0f);
            fflush(stdout);
        }
#endif
    }

    float GetMaxUnlag() {
        return ComputeMaxUnlag();
    }

    std::vector<Record*> GetValidRecords(uintptr_t pawn) {
        std::vector<Record*> out;

        RefreshCache();

        std::shared_lock lock(g_mtx);

        auto it = g_records.find(pawn);
        if (it == g_records.end()) return out;

        out.reserve(it->second.size());

        for (auto& rec : it->second) {
            if (IsRecordValid(rec)) {
                out.push_back(&rec);
            }
        }

        if (out.empty()) return out;

        const int maxTicks = (int)g_maxTicks;
        const int newestTick = out.front()->tick;

        out.erase(
            std::remove_if(out.begin(), out.end(), [newestTick, maxTicks](const Record* rec) {
                return (newestTick - rec->tick) > maxTicks;
                }),
            out.end());

        return out;
    }

    std::vector<RenderSnapshot> GetRenderSnapshots(uintptr_t pawn) {
        std::vector<RenderSnapshot> out;

        RefreshCache();

        std::shared_lock lock(g_mtx);

        auto it = g_records.find(pawn);
        if (it == g_records.end()) return out;

        out.reserve(it->second.size());

        for (auto& rec : it->second) {
            if (!IsRecordValid(rec)) continue;

            RenderSnapshot s;
            s.boneCount = (rec.boneCount < (int)s.bonePositions.size())
                ? rec.boneCount
                : (int)s.bonePositions.size();
            for (int i = 0; i < s.boneCount; i++) {
                s.bonePositions[i] = rec.bones[i].position;
            }
            s.tick = rec.tick;
            out.push_back(std::move(s));
        }

        if (out.empty()) return out;

        const int maxTicks = (int)g_maxTicks;
        const int newestTick = out.front().tick;

        out.erase(
            std::remove_if(out.begin(), out.end(), [newestTick, maxTicks](const RenderSnapshot& s) {
                return (newestTick - s.tick) > maxTicks;
                }),
            out.end());

        return out;
    }

    Record* GetOldestValid(uintptr_t pawn) {
        std::shared_lock lock(g_mtx);

        auto it = g_records.find(pawn);
        if (it == g_records.end() || it->second.empty()) return nullptr;

        for (auto rit = it->second.rbegin(); rit != it->second.rend(); ++rit) {
            if (IsRecordValid(*rit)) return &(*rit);
        }
        return nullptr;
    }

    Record* GetOldestWasValid(uintptr_t pawn) {
        std::shared_lock lock(g_mtx);

        auto it = g_records.find(pawn);
        if (it == g_records.end() || it->second.empty()) return nullptr;

        for (auto rit = it->second.rbegin(); rit != it->second.rend(); ++rit) {
            if (rit->wasValid) return &(*rit);
        }
        return nullptr;
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

    void Run() {
        CollectRecords();
    }

    bool Initialize() {
        uintptr_t mg = Memory::FindPattern("client.dll", "E8 ? ? ? ? 8B 85 2C 85 01 00");
        if (mg) g_setMeshGroup = ResolveRelCall(mg);

        uintptr_t sk = Memory::FindPattern("client.dll", "E8 ? ? ? ? 40 84 ED 74 17");
        if (sk) g_setSkeleton = ResolveRelCall(sk);

        g_getNetChannelFn = ResolveNetChannel();

        printf("[Lagcomp] SCHEMAs: gsn=0x%X modelState=0x%X simTime=0x%X absOrigin=0x%X absRot=0x%X\n",
            SCHEMA("C_BaseEntity", "m_pGameSceneNode"_hash),
            SCHEMA("CSkeletonInstance", "m_modelState"_hash),
            SCHEMA("C_BaseEntity", "m_flSimulationTime"_hash),
            SCHEMA("CGameSceneNode", "m_vecAbsOrigin"_hash),
            SCHEMA("CGameSceneNode", "m_angAbsRotation"_hash));
        fflush(stdout);

        printf("[Lagcomp] ready (meshGroup=%d skeleton=%d netChannel=%d)\n",
            g_setMeshGroup != 0, g_setSkeleton != 0, g_getNetChannelFn != 0);
        fflush(stdout);

        CreateMoveHook::Register(&OnCreateMove);
        return true;
    }

    void Shutdown() {
        std::unique_lock lock(g_mtx);
        g_records.clear();
    }

    bool IsEnabled() { return g_enabled != 0; }
    void SetEnabled(bool enabled) { InterlockedExchange(&g_enabled, enabled ? 1 : 0); }

    int GetMaxTicks() { return (int)g_maxTicks; }
    void SetMaxTicks(int ticks) {
        if (ticks < 1) ticks = 1;
        if (ticks > kMaxRecords) ticks = kMaxRecords;
        InterlockedExchange(&g_maxTicks, ticks);
    }
}