#include "pch.h"
#include "config/OffsetConfig.h"
#include "core/hooks/PresentHook.h"
#include <thread>

void MainThread(HMODULE hModule) {
    if (!AllocConsole()) return;
    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);
    SetForegroundWindow(GetConsoleWindow());

    printf("[Lavender] Initializing...\n"); fflush(stdout);

    if (!OffsetConfig::Load()) {
        printf("[Lavender] Failed to load a2x offsets.\n"); fflush(stdout);
        return;
    }
    printf("[Lavender] Offsets loaded.\n"); fflush(stdout);

    PresentHook::Initialize();

    while (true) {
        if (GetAsyncKeyState(VK_END) & 1) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
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