#include "pch.h"
#include "config/OffsetConfig.h"
#include "core/hooks/PresentHook.h"
#include "core/interfaces/InterfaceManager.h"
#include "render/Renderer.h"
#include <thread>

static const int TARGET_SLOTS[] = { 20, 21, 24, 25, 26, 28, 29, 30, 31, 34, 46, 47, 48, 51, 52, 54, 55, 56, 57, 85 };
static const int NUM_TARGETS = 20;

static volatile LONG64 g_totalCalls[NUM_TARGETS] = { 0 };
static volatile LONG64 g_pawnVtableMatches[NUM_TARGETS] = { 0 };
static volatile LONG64 g_localPawnHealthHits[NUM_TARGETS] = { 0 };

static void* g_originalFns[NUM_TARGETS] = { 0 };
static void** g_patchedVtable = nullptr;
static uint8_t* g_stubBuffer = nullptr;

static uintptr_t g_localPawnVtable = 0;
static uintptr_t g_localPawnAddr = 0;
static int       g_localPawnHealth = 0;

static bool __TryReadVtable(void* obj, void*** outVtable) {
    __try { *outVtable = *reinterpret_cast<void***>(obj); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static void SnapshotLocalPawn() {
    uintptr_t dwLocalPlayerPawn = OffsetConfig::globals.dwLocalPlayerPawn;
    if (!dwLocalPlayerPawn) {
        printf("[Probe] dwLocalPlayerPawn is 0\n"); fflush(stdout);
        return;
    }

    HANDLE hSelf = GetCurrentProcess();
    SIZE_T br = 0;
    uintptr_t pawn = 0;
    if (!ReadProcessMemory(hSelf, (LPCVOID)dwLocalPlayerPawn, &pawn, sizeof(pawn), &br) || !pawn) {
        printf("[Probe] could not read local pawn pointer\n"); fflush(stdout);
        return;
    }
    g_localPawnAddr = pawn;

    uintptr_t vtable = 0;
    if (ReadProcessMemory(hSelf, (LPCVOID)pawn, &vtable, sizeof(vtable), &br)) {
        g_localPawnVtable = vtable;
    }

    int health = 0;
    if (ReadProcessMemory(hSelf, (LPCVOID)(pawn + OffsetConfig::fields.m_iHealth), &health, 4, &br)) {
        g_localPawnHealth = health;
    }

    int team = 0;
    ReadProcessMemory(hSelf, (LPCVOID)(pawn + OffsetConfig::fields.m_iTeamNum), &team, 4, &br);

    printf("[Probe] === Local pawn snapshot ===\n"); fflush(stdout);
    printf("[Probe] dwLocalPlayerPawn global = 0x%llX\n", (unsigned long long)dwLocalPlayerPawn); fflush(stdout);
    printf("[Probe] pawn pointer             = 0x%llX\n", (unsigned long long)pawn); fflush(stdout);
    printf("[Probe] pawn vtable              = 0x%llX\n", (unsigned long long)vtable); fflush(stdout);
    printf("[Probe] pawn health              = %d\n", health); fflush(stdout);
    printf("[Probe] pawn team                = %d\n", team); fflush(stdout);
    printf("[Probe] ============================\n\n"); fflush(stdout);
}

static void CommonHandler(int slotIdx, void* arg1) {
    InterlockedIncrement64(&g_totalCalls[slotIdx]);

    if (!arg1) return;
    uintptr_t p = (uintptr_t)arg1;
    if (p < 0x10000 || p > 0x7FFFFFFFFFFF) return;

    HANDLE hSelf = GetCurrentProcess();
    uintptr_t vt = 0;
    SIZE_T br = 0;
    if (!ReadProcessMemory(hSelf, (LPCVOID)p, &vt, sizeof(vt), &br)) return;

    if (vt == g_localPawnVtable) {
        InterlockedIncrement64(&g_pawnVtableMatches[slotIdx]);

        int h = 0;
        if (ReadProcessMemory(hSelf, (LPCVOID)(p + OffsetConfig::fields.m_iHealth), &h, 4, &br)) {
            if (h > 0 && h <= 200) {
                InterlockedIncrement64(&g_localPawnHealthHits[slotIdx]);
            }
        }
    }
}

extern "C" void H00(void* a1) { CommonHandler(0, a1); }
extern "C" void H01(void* a1) { CommonHandler(1, a1); }
extern "C" void H02(void* a1) { CommonHandler(2, a1); }
extern "C" void H03(void* a1) { CommonHandler(3, a1); }
extern "C" void H04(void* a1) { CommonHandler(4, a1); }
extern "C" void H05(void* a1) { CommonHandler(5, a1); }
extern "C" void H06(void* a1) { CommonHandler(6, a1); }
extern "C" void H07(void* a1) { CommonHandler(7, a1); }
extern "C" void H08(void* a1) { CommonHandler(8, a1); }
extern "C" void H09(void* a1) { CommonHandler(9, a1); }
extern "C" void H10(void* a1) { CommonHandler(10, a1); }
extern "C" void H11(void* a1) { CommonHandler(11, a1); }
extern "C" void H12(void* a1) { CommonHandler(12, a1); }
extern "C" void H13(void* a1) { CommonHandler(13, a1); }
extern "C" void H14(void* a1) { CommonHandler(14, a1); }
extern "C" void H15(void* a1) { CommonHandler(15, a1); }
extern "C" void H16(void* a1) { CommonHandler(16, a1); }
extern "C" void H17(void* a1) { CommonHandler(17, a1); }
extern "C" void H18(void* a1) { CommonHandler(18, a1); }
extern "C" void H19(void* a1) { CommonHandler(19, a1); }

static void(*g_handlers[NUM_TARGETS])(void*) = {
    H00,H01,H02,H03,H04,H05,H06,H07,H08,H09,
    H10,H11,H12,H13,H14,H15,H16,H17,H18,H19
};

static void WriteStub(uint8_t* buf, int slotIdx) {
    uint8_t* p = buf;
    *p++ = 0x48; *p++ = 0x83; *p++ = 0xEC; *p++ = 0x48;
    *p++ = 0x48; *p++ = 0x89; *p++ = 0x4C; *p++ = 0x24; *p++ = 0x20;
    *p++ = 0x48; *p++ = 0x89; *p++ = 0x54; *p++ = 0x24; *p++ = 0x28;
    *p++ = 0x4C; *p++ = 0x89; *p++ = 0x44; *p++ = 0x24; *p++ = 0x30;
    *p++ = 0x4C; *p++ = 0x89; *p++ = 0x4C; *p++ = 0x24; *p++ = 0x38;
    *p++ = 0x48; *p++ = 0x8B; *p++ = 0x4C; *p++ = 0x24; *p++ = 0x28;
    *p++ = 0x48; *p++ = 0xB8;
    *(uint64_t*)p = (uint64_t)g_handlers[slotIdx]; p += 8;
    *p++ = 0xFF; *p++ = 0xD0;
    *p++ = 0x48; *p++ = 0x8B; *p++ = 0x4C; *p++ = 0x24; *p++ = 0x20;
    *p++ = 0x48; *p++ = 0x8B; *p++ = 0x54; *p++ = 0x24; *p++ = 0x28;
    *p++ = 0x4C; *p++ = 0x8B; *p++ = 0x44; *p++ = 0x24; *p++ = 0x30;
    *p++ = 0x4C; *p++ = 0x8B; *p++ = 0x4C; *p++ = 0x24; *p++ = 0x38;
    *p++ = 0x48; *p++ = 0x83; *p++ = 0xC4; *p++ = 0x48;
    *p++ = 0x48; *p++ = 0xB8;
    *(uint64_t*)p = (uint64_t)&g_originalFns[slotIdx]; p += 8;
    *p++ = 0x48; *p++ = 0x8B; *p++ = 0x00;
    *p++ = 0xFF; *p++ = 0xE0;
}

static bool InstallProbe(void* instance) {
    void** vtable = nullptr;
    if (!__TryReadVtable(instance, &vtable) || !vtable) return false;

    const size_t STUB_SIZE = 128;
    g_stubBuffer = (uint8_t*)VirtualAlloc(nullptr, STUB_SIZE * NUM_TARGETS,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!g_stubBuffer) return false;

    DWORD oldProtect = 0;
    if (!VirtualProtect(vtable, 128 * 8, PAGE_EXECUTE_READWRITE, &oldProtect)) return false;

    for (int i = 0; i < NUM_TARGETS; i++) {
        int slot = TARGET_SLOTS[i];
        void* originalFn = nullptr;
        __try { originalFn = vtable[slot]; }
        __except (EXCEPTION_EXECUTE_HANDLER) { continue; }
        if (!originalFn) continue;

        g_originalFns[i] = originalFn;
        uint8_t* stubAddr = g_stubBuffer + (i * STUB_SIZE);
        WriteStub(stubAddr, i);
        vtable[slot] = (void*)stubAddr;
    }

    VirtualProtect(vtable, 128 * 8, oldProtect, &oldProtect);
    g_patchedVtable = vtable;
    return true;
}

static void RestoreProbe() {
    if (!g_patchedVtable) return;
    DWORD oldProtect = 0;
    if (!VirtualProtect(g_patchedVtable, 128 * 8, PAGE_EXECUTE_READWRITE, &oldProtect)) return;
    for (int i = 0; i < NUM_TARGETS; i++) {
        if (g_originalFns[i]) g_patchedVtable[TARGET_SLOTS[i]] = g_originalFns[i];
    }
    VirtualProtect(g_patchedVtable, 128 * 8, oldProtect, &oldProtect);
    g_patchedVtable = nullptr;
    printf("[Probe] Vtable restored.\n"); fflush(stdout);
}

void MainThread(HMODULE hModule) {
    if (!AllocConsole()) return;
    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);
    SetForegroundWindow(GetConsoleWindow());

    printf("[Lavender] Initializing...\n"); fflush(stdout);
    if (!OffsetConfig::Load()) return;
    printf("[Lavender] Offsets loaded.\n"); fflush(stdout);

    PresentHook::Initialize();
    std::this_thread::sleep_for(std::chrono::milliseconds(2500));

    SnapshotLocalPawn();

    void* sceneSystem = InterfaceManager::Get<void>("scenesystem.dll", "SceneSystem_002");
    if (!sceneSystem) { printf("[Probe] no SceneSystem\n"); fflush(stdout); return; }
    if (!InstallProbe(sceneSystem)) { printf("[Probe] install failed\n"); fflush(stdout); return; }

    printf("[Probe] Facing bots for 8 seconds...\n\n"); fflush(stdout);
    for (int s = 0; s < 8; s++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        printf("[Probe] t=%d\n", s + 1); fflush(stdout);
    }

    printf("\n[Probe] ======= Local-pawn-vtable matches per slot =======\n"); fflush(stdout);
    printf("[Probe] slot | total    | vtableMatch | healthValid\n"); fflush(stdout);
    for (int i = 0; i < NUM_TARGETS; i++) {
        printf("[Probe] %4d | %-8lld | %-11lld | %-11lld\n",
            TARGET_SLOTS[i],
            (long long)g_totalCalls[i],
            (long long)g_pawnVtableMatches[i],
            (long long)g_localPawnHealthHits[i]);
    }
    printf("[Probe] =================================================\n\n"); fflush(stdout);

    RestoreProbe();

    while (!Renderer::ShouldUnload()) {
        if (GetAsyncKeyState(VK_END) & 1) Renderer::RequestUnload();
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    PresentHook::Shutdown();
    fclose(f);
    FreeConsole();
    FreeLibraryAndExitThread(hModule, 0);
}

BOOL WINAPI DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        std::thread(MainThread, hModule).detach();
    }
    return TRUE;
}