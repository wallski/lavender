#include "pch.h"
#include "PresentHook.h"
#include "render/Renderer.h"
#include <vector>

namespace PresentHook {

    static std::vector<VmtHook*>        g_hooks;
    static std::vector<IDXGISwapChain*> g_swapchains;

    using PresentFn = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT);
    using Present1Fn = HRESULT(__stdcall*)(IDXGISwapChain1*, UINT, UINT, const DXGI_PRESENT_PARAMETERS*);

    static PresentFn  o_Present = nullptr;
    static Present1Fn o_Present1 = nullptr;

    static volatile LONG g_presentCount = 0;
    static volatile LONG g_present1Count = 0;
    static bool          g_rendererInitAttempted = false;

    LONG   GetPresentCount() { return g_presentCount; }
    LONG   GetPresent1Count() { return g_present1Count; }
    size_t GetHookCount() { return g_hooks.size(); }

    HRESULT __stdcall hkPresent(IDXGISwapChain* sc, UINT si, UINT fl) {
        InterlockedIncrement(&g_presentCount);

        if (!g_rendererInitAttempted) {
            g_rendererInitAttempted = true;
            Renderer::Initialize(sc);
        }
        Renderer::RenderFrame();

        return o_Present(sc, si, fl);
    }

    HRESULT __stdcall hkPresent1(IDXGISwapChain1* sc, UINT si, UINT fl, const DXGI_PRESENT_PARAMETERS* pp) {
        InterlockedIncrement(&g_present1Count);
        return o_Present1(sc, si, fl, pp);
    }

    static bool LooksLikeRealSwapchain(void* candidate, void** expectedVtable) {
        __try {
            void** vt = *reinterpret_cast<void***>(candidate);
            if (vt != expectedVtable) return false;

            uintptr_t q = reinterpret_cast<uintptr_t>(vt[0]);
            uintptr_t ar = reinterpret_cast<uintptr_t>(vt[1]);
            uintptr_t rl = reinterpret_cast<uintptr_t>(vt[2]);
            uintptr_t pr = reinterpret_cast<uintptr_t>(vt[8]);

            if (q < 0x10000 || q  > 0x00007FFFFFFFFFFF) return false;
            if (ar < 0x10000 || ar > 0x00007FFFFFFFFFFF) return false;
            if (rl < 0x10000 || rl > 0x00007FFFFFFFFFFF) return false;
            if (pr < 0x10000 || pr > 0x00007FFFFFFFFFFF) return false;

            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    static std::vector<IDXGISwapChain*> FindAllByVtable(void** vtable) {
        std::vector<IDXGISwapChain*> results;

        SYSTEM_INFO si;
        GetSystemInfo(&si);
        uintptr_t minAddr = (uintptr_t)si.lpMinimumApplicationAddress;
        uintptr_t maxAddr = (uintptr_t)si.lpMaximumApplicationAddress;

        HANDLE hSelf = GetCurrentProcess();

        const size_t CHUNK_BYTES = 4096;
        uintptr_t buffer[CHUNK_BYTES / sizeof(uintptr_t)];
        SIZE_T bytesRead = 0;

        MEMORY_BASIC_INFORMATION mbi;
        uintptr_t addr = minAddr;

        while (addr < maxAddr && results.size() < 64) {
            if (!VirtualQuery((LPCVOID)addr, &mbi, sizeof(mbi))) break;

            bool readable = (mbi.State == MEM_COMMIT)
                && (mbi.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_WRITECOPY))
                && !(mbi.Protect & PAGE_GUARD);

            if (readable) {
                uintptr_t regionStart = (uintptr_t)mbi.BaseAddress;
                uintptr_t regionEnd = regionStart + mbi.RegionSize;

                for (uintptr_t cur = regionStart; cur < regionEnd; cur += CHUNK_BYTES) {
                    size_t toRead = CHUNK_BYTES;
                    if (cur + toRead > regionEnd) toRead = regionEnd - cur;
                    if (toRead < sizeof(uintptr_t)) break;

                    bytesRead = 0;
                    if (!ReadProcessMemory(hSelf, (LPCVOID)cur, buffer, toRead, &bytesRead)) continue;
                    if (bytesRead < sizeof(uintptr_t)) continue;

                    size_t entries = bytesRead / sizeof(uintptr_t);
                    for (size_t i = 0; i < entries; i++) {
                        if (buffer[i] == (uintptr_t)vtable) {
                            uintptr_t candidateAddr = cur + i * sizeof(uintptr_t);
                            void* candidate = (void*)candidateAddr;

                            bool dup = false;
                            for (auto* existing : results) {
                                if ((uintptr_t)existing == candidateAddr) { dup = true; break; }
                            }
                            if (dup) continue;

                            if (!LooksLikeRealSwapchain(candidate, vtable)) continue;

                            results.push_back((IDXGISwapChain*)candidate);
                            if (results.size() >= 64) break;
                        }
                    }
                }
            }
            addr = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
        }
        return results;
    }

    bool Initialize() {
        printf("[PresentHook] Creating dummy swapchain...\n"); fflush(stdout);

        WNDCLASSEXA wc = { 0 };
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = DefWindowProcA;
        wc.hInstance = GetModuleHandleA(nullptr);
        wc.lpszClassName = "LavenderDummyWnd";
        RegisterClassExA(&wc);

        HWND hWnd = CreateWindowExA(0, "LavenderDummyWnd", "", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, nullptr, nullptr, wc.hInstance, nullptr);
        if (!hWnd) {
            printf("[PresentHook] FAILED: CreateWindowExA.\n"); fflush(stdout);
            UnregisterClassA("LavenderDummyWnd", wc.hInstance);
            return false;
        }

        DXGI_SWAP_CHAIN_DESC scd = { 0 };
        scd.BufferCount = 1;
        scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        scd.OutputWindow = hWnd;
        scd.SampleDesc.Count = 1;
        scd.Windowed = TRUE;
        scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        IDXGISwapChain* dummy = nullptr;
        ID3D11Device* dev = nullptr;
        ID3D11DeviceContext* ctx = nullptr;
        D3D_FEATURE_LEVEL fl;

        HRESULT hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
            nullptr, 0, D3D11_SDK_VERSION,
            &scd, &dummy, &dev, &fl, &ctx);

        if (FAILED(hr) || !dummy) {
            printf("[PresentHook] FAILED: D3D11CreateDeviceAndSwapChain hr=0x%X\n", (unsigned)hr); fflush(stdout);
            if (hWnd) DestroyWindow(hWnd);
            UnregisterClassA("LavenderDummyWnd", wc.hInstance);
            return false;
        }

        void** dummyVtable = *reinterpret_cast<void***>(dummy);
        o_Present = (PresentFn)dummyVtable[8];
        o_Present1 = (Present1Fn)dummyVtable[22];

        if (ctx) ctx->Release();
        if (dev) dev->Release();
        if (dummy) dummy->Release();
        DestroyWindow(hWnd);
        UnregisterClassA("LavenderDummyWnd", wc.hInstance);

        printf("[PresentHook] Scanning for game swapchains...\n"); fflush(stdout);
        auto candidates = FindAllByVtable(dummyVtable);
        printf("[PresentHook] Found %zu candidate(s).\n", candidates.size()); fflush(stdout);

        if (candidates.empty()) {
            printf("[PresentHook] FAILED: no valid swapchain candidates.\n"); fflush(stdout);
            return false;
        }

        for (size_t i = 0; i < candidates.size(); i++) {
            printf("[PresentHook]   [%zu] 0x%p", i, candidates[i]); fflush(stdout);

            VmtHook* hook = new VmtHook();
            if (!hook->Initialize(candidates[i])) {
                printf("  VMT init failed\n"); fflush(stdout);
                delete hook;
                continue;
            }
            hook->Hook(8, (void*)hkPresent);
            hook->Hook(22, (void*)hkPresent1);
            g_hooks.push_back(hook);
            g_swapchains.push_back(candidates[i]);
            printf("  hooked\n"); fflush(stdout);
        }

        printf("[PresentHook] Installed %zu hook(s).\n", g_hooks.size()); fflush(stdout);
        printf("[PresentHook]   original Present  = 0x%p\n", (void*)o_Present); fflush(stdout);
        printf("[PresentHook]   original Present1 = 0x%p\n", (void*)o_Present1); fflush(stdout);
        return !g_hooks.empty();
    }

    void Shutdown() {
        for (auto* h : g_hooks) h->Restore();
        Renderer::Shutdown();
        for (auto* h : g_hooks) delete h;
        g_hooks.clear();
    }
}