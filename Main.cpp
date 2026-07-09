#include "Cheat/Core/Memory/Memory.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <conio.h>
#include "Cheat/Core/Roblox/Engine/Offsets/Offsets.h"
#include "Cheat/Core/Globals/Globals.h"
#include "Cheat/Core/PlayerHandler/PlayerHandler.h"
#include "Cheat/Renderer/Renderer.h"

void OverlayThread()
{
    if (Cheat::Renderer::Initialize(GetModuleHandle(nullptr)))
    {
        Cheat::Renderer::MainLoop();
        Cheat::Renderer::Shutdown();
    }
}

int main()
{
    SetProcessDPIAware();
    std::thread(OverlayThread).detach();

    if (!g_Memory.Attach(L"RobloxPlayerBeta.exe")) {
        std::cout << "waiting for roblox...\n";
        while (!g_Memory.Attach(L"RobloxPlayerBeta.exe")) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    std::cout << "attached\n";

    uintptr_t clientBase = g_Memory.GetModuleBase();
    if (!clientBase)
        throw std::runtime_error("Failed to get base address.");
    std::cout << "pointer correct\n";

    Cheat::PlayerHandler::StartCacheThread();

    while (true)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    Cheat::PlayerHandler::StopCacheThread();
    return 0;
}
