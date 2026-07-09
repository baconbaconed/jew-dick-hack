#include "Classes.h"
#include "../../../Memory/Memory.h"
#include "../Offsets/Offsets.h"

std::shared_ptr<Cheat::Instance> Cheat::Workspace::GetCurrentCamera() const
{
    if (!g_Memory.IsValid(address))
        return nullptr;

    std::uint64_t cam_addr = g_Memory.Read<std::uint64_t>(address + Offsets::Workspace::CurrentCamera);
    if (!g_Memory.IsValid(cam_addr))
        return nullptr;

    return std::make_shared<Cheat::Instance>(cam_addr);
}

double Cheat::Workspace::GetDistributedGameTime() const
{
    if (!g_Memory.IsValid(address))
        return 0.0;

    return g_Memory.Read<double>(address + Offsets::Workspace::DistributedGameTime);
}

float Cheat::Workspace::GetGravity() const
{
    if (!g_Memory.IsValid(address))
        return 0.f;

    std::uint64_t world = g_Memory.Read<std::uint64_t>(address + Offsets::Workspace::World);
    if (!g_Memory.IsValid(world))
        return 0.f;

    return g_Memory.Read<float>(world + Offsets::World::Gravity);
}
