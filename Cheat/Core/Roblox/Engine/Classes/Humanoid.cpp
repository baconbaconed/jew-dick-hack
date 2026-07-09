#include "Classes.h"
#include "../../../Memory/Memory.h"
#include "../Offsets/Offsets.h"

float Cheat::Humanoid::GetHealth() const
{
    if (!g_Memory.IsValid(address))
        return 0.f;

    return g_Memory.Read<float>(address + Offsets::Humanoid::Health);
}

float Cheat::Humanoid::GetMaxHealth() const
{
    if (!g_Memory.IsValid(address))
        return 0.f;

    return g_Memory.Read<float>(address + Offsets::Humanoid::MaxHealth);
}

float Cheat::Humanoid::GetWalkSpeed() const
{
    if (!g_Memory.IsValid(address))
        return 0.f;

    return g_Memory.Read<float>(address + Offsets::Humanoid::Walkspeed);
}

float Cheat::Humanoid::GetJumpPower() const
{
    if (!g_Memory.IsValid(address))
        return 0.f;

    return g_Memory.Read<float>(address + Offsets::Humanoid::JumpPower);
}

float Cheat::Humanoid::GetJumpHeight() const
{
    if (!g_Memory.IsValid(address))
        return 0.f;

    return g_Memory.Read<float>(address + Offsets::Humanoid::JumpHeight);
}

float Cheat::Humanoid::GetHipHeight() const
{
    if (!g_Memory.IsValid(address))
        return 0.f;

    return g_Memory.Read<float>(address + Offsets::Humanoid::HipHeight);
}

Vector3 Cheat::Humanoid::GetMoveDirection() const
{
    if (!g_Memory.IsValid(address))
        return {};

    return g_Memory.Read<Vector3>(address + Offsets::Humanoid::MoveDirection);
}

std::string Cheat::Humanoid::GetDisplayName() const
{
    if (!g_Memory.IsValid(address))
        return "Unknown";

    std::uint64_t name_ptr = g_Memory.Read<std::uint64_t>(address + Offsets::Humanoid::DisplayName);
    if (!g_Memory.IsValid(name_ptr))
        return "Unknown";

    return g_Memory.ReadString(name_ptr);
}

bool Cheat::Humanoid::IsWalking() const
{
    if (!g_Memory.IsValid(address))
        return false;

    return g_Memory.Read<bool>(address + Offsets::Humanoid::IsWalking);
}

bool Cheat::Humanoid::GetAutoRotate() const
{
    if (!g_Memory.IsValid(address))
        return false;

    return g_Memory.Read<bool>(address + Offsets::Humanoid::AutoRotate);
}

std::int32_t Cheat::Humanoid::GetStateId() const
{
    if (!g_Memory.IsValid(address))
        return -1;

    std::uint64_t state_ptr = g_Memory.Read<std::uint64_t>(address + Offsets::Humanoid::HumanoidState);
    if (!g_Memory.IsValid(state_ptr))
        return -1;

    return g_Memory.Read<std::int32_t>(state_ptr + Offsets::Humanoid::HumanoidStateID);
}

bool Cheat::Humanoid::GetAutoJump() const
{
    if (!g_Memory.IsValid(address))
        return false;

    return g_Memory.Read<bool>(address + Offsets::Humanoid::AutoJumpEnabled);
}

void Cheat::Humanoid::SetHealth(float value) const
{
    if (!g_Memory.IsValid(address))
        return;

    g_Memory.Write<float>(address + Offsets::Humanoid::Health, value);
}

void Cheat::Humanoid::SetMaxHealth(float value) const
{
    if (!g_Memory.IsValid(address))
        return;

    g_Memory.Write<float>(address + Offsets::Humanoid::MaxHealth, value);
}

void Cheat::Humanoid::SetWalkSpeed(float value) const
{
    if (!g_Memory.IsValid(address))
        return;

    g_Memory.Write<float>(address + Offsets::Humanoid::Walkspeed, value);
}

void Cheat::Humanoid::SetJumpPower(float value) const
{
    if (!g_Memory.IsValid(address))
        return;

    g_Memory.Write<float>(address + Offsets::Humanoid::JumpPower, value);
}

void Cheat::Humanoid::SetJumpHeight(float value) const
{
    if (!g_Memory.IsValid(address))
        return;

    g_Memory.Write<float>(address + Offsets::Humanoid::JumpHeight, value);
}

std::uint64_t Cheat::Humanoid::GetRootPartAddress() const
{
    if (!g_Memory.IsValid(address))
        return 0;

    return g_Memory.Read<std::uint64_t>(address + Offsets::Humanoid::HumanoidRootPart);
}

std::shared_ptr<Cheat::Instance> Cheat::Humanoid::GetRootPart() const
{
    std::uint64_t root_addr = GetRootPartAddress();
    if (!g_Memory.IsValid(root_addr))
        return nullptr;

    return std::make_shared<Cheat::Instance>(root_addr);
}
