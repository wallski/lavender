#include "pch.h"
#include "config/OffsetConfig.h"
#include "core/config/ConfigManager.h"
#include "core/hooks/PresentHook.h"
#include "core/interfaces/InterfaceManager.h"
#include "core/memory/PatternScan.h"
#include "core/cvar/CVar.h"
#include "core/features/visuals/chams/Chams.h"
#include "core/features/visuals/chams/Materials.h"
#include "core/features/visuals/esp/Esp.h"
#include "core/features/visuals/skybox/Skybox.h"
#include "core/features/combat/CombatShared.h"
#include "core/features/combat/legit/triggerbot/Triggerbot.h"
#include "render/Renderer.h"
#include <thread>
#include <cstdarg>

static HANDLE g_logHandle = INVALID_HANDLE_VALUE;

static void LogInit(const char* anchor) {
    char path[MAX_PATH] = { 0 };
    HMODULE hm = NULL;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        anchor, &hm);
    GetModuleFileNameA(hm, path, MAX_PATH);
    char* lastSlash = strrchr(path, '\\');
    if (lastSlash) *(lastSlash + 1) = 0;
    strcat_s(path, "lavender_log.txt");
    g_logHandle = CreateFileA(path, GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (g_logHandle == INVALID_HANDLE_VALUE) {
        char tempPath[MAX_PATH] = { 0 };
        DWORD n = GetTempPathA(MAX_PATH, tempPath);
        if (n > 0 && n < MAX_PATH) {
            strcat_s(tempPath, "lavender_log.txt");
            g_logHandle = CreateFileA(tempPath, GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        }
    }
}

static void Log(const char* fmt, ...) {
    char buf[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    printf("%s", buf);
    fflush(stdout);
    if (g_logHandle != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(g_logHandle, buf, (DWORD)strlen(buf), &written, NULL);
    }
}

static void LogClose() {
    if (g_logHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(g_logHandle);
        g_logHandle = INVALID_HANDLE_VALUE;
    }
}

static volatile LONG64 g_sortCallCount = 0;
static uintptr_t g_sortFn = 0;
static uint8_t* g_stub = nullptr;
static uint8_t g_originalBytes[16] = { 0 };

extern "C" void CaptureSortCall(void* array, uint32_t count) {
    InterlockedIncrement64(&g_sortCallCount);
    Chams::OnSortCall(array, count);
}

static void* AllocNear(void* hint, size_t size) {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    uintptr_t gran = si.dwAllocationGranularity;
    uintptr_t base = (uintptr_t)hint & ~(gran - 1);
    for (int i = 1; i < 0x1000; i++) {
        uintptr_t addr = base - (uintptr_t)i * gran;
        void* mem = VirtualAlloc((void*)addr, size, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
        if (mem) return mem;
    }
    for (int i = 1; i < 0x1000; i++) {
        uintptr_t addr = base + (uintptr_t)i * gran;
        void* mem = VirtualAlloc((void*)addr, size, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
        if (mem) return mem;
    }
    return VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
}

static uint8_t* WriteSortStub(uintptr_t fn) {
    uint8_t* stub = (uint8_t*)AllocNear((void*)fn, 128);
    if (!stub) return nullptr;
    uint8_t* p = stub;

    *p++ = 0x51;
    *p++ = 0x52;
    *p++ = 0x41; *p++ = 0x50;
    *p++ = 0x41; *p++ = 0x51;
    *p++ = 0x48; *p++ = 0x83; *p++ = 0xEC; *p++ = 0x28;
    *p++ = 0x4C; *p++ = 0x89; *p++ = 0xC1;
    *p++ = 0x44; *p++ = 0x89; *p++ = 0xCA;
    *p++ = 0x48; *p++ = 0xB8;
    *(uint64_t*)p = (uint64_t)&CaptureSortCall; p += 8;
    *p++ = 0xFF; *p++ = 0xD0;
    *p++ = 0x48; *p++ = 0x83; *p++ = 0xC4; *p++ = 0x28;
    *p++ = 0x41; *p++ = 0x59;
    *p++ = 0x41; *p++ = 0x58;
    *p++ = 0x5A;
    *p++ = 0x59;
    *p++ = 0x55;
    *p++ = 0x56;
    *p++ = 0x57;
    *p++ = 0x48; *p++ = 0x83; *p++ = 0xEC; *p++ = 0x30;
    *p++ = 0xFF; *p++ = 0x25; *p++ = 0x00; *p++ = 0x00; *p++ = 0x00; *p++ = 0x00;
    *(uint64_t*)p = fn + 16; p += 8;
    return stub;
}

static bool InstallSortHook(uintptr_t fn) {
    memcpy(g_originalBytes, (void*)fn, 16);
    if (g_originalBytes[0] != 0x45 || g_originalBytes[1] != 0x85 || g_originalBytes[2] != 0xC9) return false;
    if (g_originalBytes[3] != 0x0F || g_originalBytes[4] != 0x84) return false;
    if (g_originalBytes[9] != 0x55 || g_originalBytes[10] != 0x56 || g_originalBytes[11] != 0x57) return false;

    g_stub = WriteSortStub(fn);
    if (!g_stub) return false;

    intptr_t rel = (intptr_t)g_stub - (intptr_t)(fn + 9) - 5;
    if (rel < INT32_MIN || rel > INT32_MAX) return false;

    DWORD oldProtect = 0;
    if (!VirtualProtect((void*)(fn + 9), 5, PAGE_EXECUTE_READWRITE, &oldProtect)) return false;
    uint8_t* patch = (uint8_t*)(fn + 9);
    patch[0] = 0xE9;
    *(int32_t*)(patch + 1) = (int32_t)rel;
    VirtualProtect((void*)(fn + 9), 5, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), (void*)(fn + 9), 5);
    return true;
}

static void RestoreSortHook() {
    if (!g_sortFn) return;
    DWORD oldProtect = 0;
    if (VirtualProtect((void*)(g_sortFn + 9), 5, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        memcpy((void*)(g_sortFn + 9), g_originalBytes + 9, 5);
        VirtualProtect((void*)(g_sortFn + 9), 5, oldProtect, &oldProtect);
        FlushInstructionCache(GetCurrentProcess(), (void*)(g_sortFn + 9), 5);
    }
    if (g_stub) { VirtualFree(g_stub, 0, MEM_RELEASE); g_stub = nullptr; }
}

void MainThread(HMODULE hModule) {
    LogInit((const char*)&MainThread);
    if (!AllocConsole()) { LogClose(); return; }
    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);
    SetForegroundWindow(GetConsoleWindow());

    Log("[Lavender] Initializing...\n");

    if (!OffsetConfig::Load()) {
        Log("[Lavender] Failed to load offsets\n");
        LogClose();
        return;
    }
    OffsetConfig::Resolve();
    Log("[Lavender] Offsets loaded.\n");

    Chams::SetEntityListGlobal(OffsetConfig::globals.dwEntityList);
    Chams::SetLocalPawnGlobal(OffsetConfig::globals.dwLocalPlayerPawn);

    ConfigManager::Initialize();

    if (!Materials::Initialize()) {
        Log("[Lavender] Materials failed\n");
    }

    if (!Skybox::Initialize()) {
        Log("[Lavender] Skybox failed (feature disabled)\n");
    }

    if (CVar::Initialize()) {
        Chams::ApplyLodOverride();
    }

    CombatShared::Initialize();
    Triggerbot::Initialize();

    PresentHook::Initialize();
    std::this_thread::sleep_for(std::chrono::milliseconds(2500));

    const char* pattern = "45 85 C9 0F 84 ? ? ? ? 55 56 57 48 83 EC 30";
    uintptr_t fn = Memory::FindPattern("scenesystem.dll", pattern);
    if (fn) {
        g_sortFn = fn;
        if (!InstallSortHook(fn)) {
            Log("[Lavender] Sort hook failed\n");
        }
    }
    else {
        Log("[Lavender] Sort pattern not found\n");
    }

    Log("[Lavender] Ready.\n");

    while (!Renderer::ShouldUnload()) {
        if (GetAsyncKeyState(VK_END) & 1) Renderer::RequestUnload();
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    Triggerbot::Shutdown();
    RestoreSortHook();
    CombatShared::Shutdown();
    CVar::Shutdown();
    Esp::Shutdown();
    Skybox::Shutdown();
    Materials::Shutdown();
    ConfigManager::Shutdown();
    PresentHook::Shutdown();
    fclose(f);
    LogClose();
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