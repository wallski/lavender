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

    static std::vector<IDXGISwapChain*> FindAllByVtable(void** vtable) {
        std::vector<IDXGISwapChain*> results;

        SYSTEM_INFO si;
        GetSystemInfo(&si);
        uintptr_t minAddr = (uintptr_t)si.lpMinimumApplicationAddress;
        uintptr_t maxAddr = (uintptr_t)si.lpMaximumApplicationAddress;

        MEMORY_BASIC_INFORMATION mbi;
        uintptr_t addr = minAddr;

        while (addr < maxAddr && results.size() < 64) {
            if (!VirtualQuery((LPCVOID)addr, &mbi, sizeof(mbi))) break;

            if (mbi.State == MEM_COMMIT
                && (mbi.Protect & PAGE_READWRITE
                    || mbi.Protect & PAGE_EXECUTE_READWRITE
                    || mbi.Protect & PAGE_WRITECOPY
                    || mbi.Protect & PAGE_EXECUTE_WRITECOPY)
                && !(mbi.Protect & PAGE_GUARD)) {

                uintptr_t* p = (uintptr_t*)mbi.BaseAddress;
                size_t count = mbi.RegionSize / sizeof(uintptr_t);

                for (size_t i = 0; i < count; i++) {
                    if (p[i] == (uintptr_t)vtable) {
                        IDXGISwapChain* sc = (IDXGISwapChain*)&p[i];
                        bool dup = false;
                        for (auto* existing : results) {
                            if (existing == sc) { dup = true; break; }
                        }
                        if (!dup) results.push_back(sc);
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

        auto candidates = FindAllByVtable(dummyVtable);
        printf("[PresentHook] Found %zu candidate(s).\n", candidates.size()); fflush(stdout);

        if (candidates.empty()) return false;

        for (size_t i = 0; i < candidates.size(); i++) {
            VmtHook* hook = new VmtHook();
            if (!hook->Initialize(candidates[i])) {
                delete hook;
                continue;
            }
            hook->Hook(8, (void*)hkPresent);
            hook->Hook(22, (void*)hkPresent1);
            g_hooks.push_back(hook);
            g_swapchains.push_back(candidates[i]);
        }

        printf("[PresentHook] Installed %zu hook(s).\n", g_hooks.size()); fflush(stdout);
        return !g_hooks.empty();
    }

    void Shutdown() {
        Renderer::Shutdown();
        for (auto* h : g_hooks) {
            h->Restore();
            delete h;
        }
        g_hooks.clear();
    }
}