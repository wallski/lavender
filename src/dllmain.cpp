#include "pch.h"
#include "core/memory/OffsetManager.h"
#include <thread>

void MainThread(HMODULE hModule) {
    if (!AllocConsole()) return;
    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);
    SetForegroundWindow(GetConsoleWindow());

    printf("[Lavender] Initializing...\n"); fflush(stdout);

    Offsets::Initialize();

    while (true) {
        if (GetAsyncKeyState(VK_END) & 1) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

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