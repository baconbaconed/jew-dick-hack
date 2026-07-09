#include "Renderer.h"
#include "../GUI/Menu.h"
#include "../Core/Graphics.h"
#include <dwmapi.h>
#include <iostream>

namespace Cheat {

    bool Renderer::Initialize(HINSTANCE instance) {
        WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, instance, nullptr, nullptr, nullptr, nullptr, L"jewsploit Overlay", nullptr };
        if (!RegisterClassExW(&wc)) {
            std::cout << "overlay failed (window class)\n";
            return false;
        }

        m_Hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED, wc.lpszClassName, L"jewsploit Overlay", WS_POPUP, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), nullptr, nullptr, wc.hInstance, nullptr);
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

        ShowWindow(m_Hwnd, SW_SHOWDEFAULT);
        UpdateWindow(m_Hwnd);

        Cheat::Core::g_Device       = m_Device;
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

            long style = GetWindowLong(m_Hwnd, GWL_EXSTYLE);
            if (GUI::Menu::IsVisible()) {
                if (style & WS_EX_TRANSPARENT)
                    SetWindowLong(m_Hwnd, GWL_EXSTYLE, style & ~WS_EX_TRANSPARENT);
            } else {
                if (!(style & WS_EX_TRANSPARENT))
                    SetWindowLong(m_Hwnd, GWL_EXSTYLE, style | WS_EX_TRANSPARENT);
            }

            const float clear_color_with_alpha[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
            m_DeviceContext->OMSetRenderTargets(1, &m_RenderTargetView, nullptr);
            m_DeviceContext->ClearRenderTargetView(m_RenderTargetView, clear_color_with_alpha);

            GUI::Menu::Render();

            m_SwapChain->Present(1, 0);
        }
    }

    void Renderer::Shutdown() {
        GUI::Menu::Shutdown();

        CleanupDevice();
        DestroyWindow(m_Hwnd);
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
        HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &m_SwapChain, &m_Device, &featureLevel, &m_DeviceContext);
        if (res != S_OK) return false;

        CreateRenderTarget();
        return true;
    }

    void Renderer::CreateRenderTarget() {
        ID3D11Texture2D* pBackBuffer;
        m_SwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
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
        if (GUI::Menu::HandleMessage(hwnd, msg, wparam, lparam))
            return true;

        switch (msg) {
        case WM_SIZE:
            if (m_Device != nullptr && wparam != SIZE_MINIMIZED) {
                CleanupRenderTarget();
                m_SwapChain->ResizeBuffers(0, (UINT)LOWORD(lparam), (UINT)HIWORD(lparam), DXGI_FORMAT_UNKNOWN, 0);
                CreateRenderTarget();
            }
            return 0;
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
