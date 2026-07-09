#pragma once
#include <windows.h>
#include <d3d11.h>

namespace Cheat {

    class Renderer {
    public:
        static bool Initialize(HINSTANCE instance);
        static void Shutdown();
        static void MainLoop();

    private:
        static bool CreateDevice();
        static void CleanupDevice();
        static void CreateRenderTarget();
        static void CleanupRenderTarget();
        static LRESULT WINAPI WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

        inline static HWND m_Hwnd = nullptr;
        inline static ID3D11Device* m_Device = nullptr;
        inline static ID3D11DeviceContext* m_DeviceContext = nullptr;
        inline static IDXGISwapChain* m_SwapChain = nullptr;
        inline static ID3D11RenderTargetView* m_RenderTargetView = nullptr;
    };

}
