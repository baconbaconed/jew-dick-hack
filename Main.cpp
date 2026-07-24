#include "Cheat/Core/Memory/Memory.h"
#include "Cheat/Core/Console/Console.h"
#include "Cheat/Core/Roblox/Engine/Offsets/Offsets.h"
#include "Cheat/Core/Globals/Globals.h"
#include "Cheat/Core/PlayerHandler/PlayerHandler.h"
#include "Cheat/Renderer/Renderer.h"
#include <thread>
#include <chrono>

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
        Cheat::Console::Log(Cheat::Console::Color::Yellow, "waiting for roblox");
        while (!g_Memory.Attach(L"RobloxPlayerBeta.exe")) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    Cheat::Console::Clear();
    Cheat::Console::Log(Cheat::Console::Color::Green, "attached");
    Cheat::Console::Ptr(Cheat::Console::Color::Cyan, "Module", g_Memory.GetModuleBase());
    Cheat::Console::Log(Cheat::Console::Color::Gray, "PID            %lu",
        (unsigned long)g_Memory.GetPID());

    Cheat::PlayerHandler::StartCacheThread();

    while (true)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    Cheat::PlayerHandler::StopCacheThread();
    return 0;
}
