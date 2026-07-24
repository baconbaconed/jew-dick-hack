#include "Renderer.h"
#include "../GUI/Menu.h"
#include "../Core/Graphics.h"
#include "../Core/Memory/Memory.h"
#include <dwmapi.h>
#include <windowsx.h>
#include <iostream>

#pragma comment(lib, "dwmapi.lib")

namespace Cheat {

    namespace {

        constexpr DWORD kOverlayExStyle =
            WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED |
            WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE;
    }

    HWND Renderer::FindGameWindow()
    {
        const DWORD pid = g_Memory.GetPID();
        if (!pid) return nullptr;

        struct Ctx {
            DWORD pid;
            HWND  best = nullptr;
            int   best_area = 0;
        } ctx{ pid };

        EnumWindows([](HWND hwnd, LPARAM lp) -> BOOL {
            auto* c = reinterpret_cast<Ctx*>(lp);
            DWORD wpid = 0;
            GetWindowThreadProcessId(hwnd, &wpid);
            if (wpid != c->pid) return TRUE;
            if (!IsWindowVisible(hwnd) || IsIconic(hwnd)) return TRUE;
            if (GetWindow(hwnd, GW_OWNER) != nullptr) return TRUE;

            RECT cr{};
            if (!GetClientRect(hwnd, &cr)) return TRUE;
            const int w = cr.right - cr.left;
            const int h = cr.bottom - cr.top;
            if (w < 200 || h < 200) return TRUE;

            wchar_t title[256]{};
            GetWindowTextW(hwnd, title, 256);
            if (title[0] == L'\0') return TRUE;

            const int area = w * h;
            if (area > c->best_area) {
                c->best_area = area;
                c->best = hwnd;
            }
            return TRUE;
        }, reinterpret_cast<LPARAM>(&ctx));

        return ctx.best;
    }

    void Renderer::ResizeSwapchain(int w, int h)
    {
        if (!m_SwapChain || w <= 0 || h <= 0) return;
        if (w == m_Width && h == m_Height) return;

        CleanupRenderTarget();
        if (FAILED(m_SwapChain->ResizeBuffers(0, (UINT)w, (UINT)h, DXGI_FORMAT_UNKNOWN, 0)))
            return;
        CreateRenderTarget();
        m_Width = w;
        m_Height = h;
    }

    void Renderer::SyncToGameWindow()
    {
        m_GameHwnd = FindGameWindow();
        m_GameActive = false;

        if (!m_GameHwnd || !IsWindow(m_GameHwnd) || IsIconic(m_GameHwnd)) {
            if (IsWindowVisible(m_Hwnd))
                ShowWindow(m_Hwnd, SW_HIDE);
            return;
        }

        const HWND fg = GetForegroundWindow();

        const bool focused = (fg == m_GameHwnd) || IsChild(m_GameHwnd, fg);
        if (!focused) {
            if (IsWindowVisible(m_Hwnd))
                ShowWindow(m_Hwnd, SW_HIDE);
            return;
        }

        RECT cr{};
        if (!GetClientRect(m_GameHwnd, &cr)) {
            ShowWindow(m_Hwnd, SW_HIDE);
            return;
        }

        POINT tl{ cr.left, cr.top };
        POINT br{ cr.right, cr.bottom };
        ClientToScreen(m_GameHwnd, &tl);
        ClientToScreen(m_GameHwnd, &br);

        const int w = br.x - tl.x;
        const int h = br.y - tl.y;
        if (w < 64 || h < 64) {
            ShowWindow(m_Hwnd, SW_HIDE);
            return;
        }

        ResizeSwapchain(w, h);

        SetWindowPos(
            m_Hwnd, HWND_TOPMOST,
            tl.x, tl.y, w, h,
            SWP_SHOWWINDOW | SWP_NOACTIVATE);

        m_GameActive = true;
    }

    bool Renderer::Initialize(HINSTANCE instance) {
        WNDCLASSEXW wc = {
            sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, instance,
            nullptr, nullptr, nullptr, nullptr, L"jewsploit.overlay", nullptr
        };
        if (!RegisterClassExW(&wc)) {
            std::cout << "overlay failed (window class)\n";
            return false;
        }

        m_Hwnd = CreateWindowExW(
            kOverlayExStyle,
            wc.lpszClassName,
            L"",
            WS_POPUP,
            0, 0, 2, 2,
            nullptr, nullptr, wc.hInstance, nullptr);
        if (!m_Hwnd) {
            std::cout << "overlay failed (window)\n";
            return false;
        }

        SetLayeredWindowAttributes(m_Hwnd, 0, 255, LWA_ALPHA);

        MARGINS margins = { -1 };
        DwmExtendFrameIntoClientArea(m_Hwnd, &margins);

        if (!CreateDevice()) {
            std::cout << "overlay failed (dx11)\n";
            CleanupDevice();
            UnregisterClassW(wc.lpszClassName, wc.hInstance);
            return false;
        }

        ShowWindow(m_Hwnd, SW_HIDE);

        Cheat::Core::g_Device        = m_Device;
        Cheat::Core::g_DeviceContext = m_DeviceContext;

        if (!GUI::Menu::Initialize(m_Hwnd, m_Device, m_DeviceContext)) {
            std::cout << "overlay failed (menu)\n";
            CleanupDevice();
            UnregisterClassW(wc.lpszClassName, wc.hInstance);
            return false;
        }

        std::cout << "overlay initialized\n";
        return true;
    }

    void Renderer::MainLoop() {
        bool running = true;
        while (running) {
            MSG msg;
            while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
                if (msg.message == WM_QUIT)
                    running = false;
            }
            if (!running) break;

            SyncToGameWindow();

            if (!m_GameActive) {
                Sleep(15);
                continue;
            }

            const float clear_color_with_alpha[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
            m_DeviceContext->OMSetRenderTargets(1, &m_RenderTargetView, nullptr);
            m_DeviceContext->ClearRenderTargetView(m_RenderTargetView, clear_color_with_alpha);

            GUI::Menu::Render();

            const LONG base = WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE;
            if (GUI::Menu::IsVisible()) {
                SetWindowLong(m_Hwnd, GWL_EXSTYLE, base);
                ClipCursor(nullptr);
            } else {
                SetWindowLong(m_Hwnd, GWL_EXSTYLE, base | WS_EX_TRANSPARENT);
            }

            m_SwapChain->Present(1, 0);
        }
    }

    void Renderer::Shutdown() {
        GUI::Menu::Shutdown();

        CleanupDevice();
        if (m_Hwnd) {
            DestroyWindow(m_Hwnd);
            m_Hwnd = nullptr;
        }
    }

    bool Renderer::CreateDevice() {
        DXGI_SWAP_CHAIN_DESC sd;
        ZeroMemory(&sd, sizeof(sd));
        sd.BufferCount = 2;
        sd.BufferDesc.Width = 0;
        sd.BufferDesc.Height = 0;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferDesc.RefreshRate.Numerator = 0;
        sd.BufferDesc.RefreshRate.Denominator = 1;
        sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = m_Hwnd;
        sd.SampleDesc.Count = 1;
        sd.SampleDesc.Quality = 0;
        sd.Windowed = TRUE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        UINT createDeviceFlags = 0;
        D3D_FEATURE_LEVEL featureLevel;
        const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
        HRESULT res = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags,
            featureLevelArray, 2, D3D11_SDK_VERSION, &sd,
            &m_SwapChain, &m_Device, &featureLevel, &m_DeviceContext);
        if (res != S_OK) return false;

        CreateRenderTarget();

        DXGI_SWAP_CHAIN_DESC cur{};
        if (SUCCEEDED(m_SwapChain->GetDesc(&cur))) {
            m_Width  = (int)cur.BufferDesc.Width;
            m_Height = (int)cur.BufferDesc.Height;
        }
        return true;
    }

    void Renderer::CreateRenderTarget() {
        ID3D11Texture2D* pBackBuffer = nullptr;
        m_SwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
        if (!pBackBuffer) return;
        m_Device->CreateRenderTargetView(pBackBuffer, nullptr, &m_RenderTargetView);
        pBackBuffer->Release();
    }

    void Renderer::CleanupDevice() {
        CleanupRenderTarget();
        if (m_SwapChain) { m_SwapChain->Release(); m_SwapChain = nullptr; }
        if (m_DeviceContext) { m_DeviceContext->Release(); m_DeviceContext = nullptr; }
        if (m_Device) { m_Device->Release(); m_Device = nullptr; }
    }

    void Renderer::CleanupRenderTarget() {
        if (m_RenderTargetView) { m_RenderTargetView->Release(); m_RenderTargetView = nullptr; }
    }

    LRESULT WINAPI Renderer::WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {

        if (msg == WM_NCHITTEST) {
            if (GUI::Menu::IsVisible())
                return HTCLIENT;
            return HTTRANSPARENT;
        }

        if (GUI::Menu::HandleMessage(hwnd, msg, wparam, lparam))
            return true;

        switch (msg) {
        case WM_SIZE:
            if (m_Device != nullptr && wparam != SIZE_MINIMIZED) {
                const int w = (int)LOWORD(lparam);
                const int h = (int)HIWORD(lparam);
                if (w > 0 && h > 0)
                    ResizeSwapchain(w, h);
            }
            return 0;
        case WM_MOUSEACTIVATE:

            return MA_NOACTIVATE;
        case WM_SYSCOMMAND:
            if ((wparam & 0xfff0) == SC_KEYMENU) return 0;
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProcW(hwnd, msg, wparam, lparam);
    }

}
