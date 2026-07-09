#include "Classes.h"
#include "../../../Memory/Memory.h"
#include "../../../Globals/Globals.h"
#include "../Offsets/Offsets.h"

std::string Cheat::Player::GetDisplayName() const
{
    if (!g_Memory.IsValid(address))
        return "Unknown";

    std::uint64_t name_ptr = g_Memory.Read<std::uint64_t>(address + Offsets::Player::DisplayName);
    if (!g_Memory.IsValid(name_ptr))
        return "Unknown";

    return g_Memory.ReadString(name_ptr);
}

std::int64_t Cheat::Player::GetUserId() const
{
    if (!g_Memory.IsValid(address))
        return 0;

    return g_Memory.Read<std::int64_t>(address + Offsets::Player::UserId);
}

std::int32_t Cheat::Player::GetAccountAge() const
{
    if (!g_Memory.IsValid(address))
        return 0;

    return g_Memory.Read<std::int32_t>(address + Offsets::Player::AccountAge);
}

float Cheat::Player::GetMaxZoomDistance() const
{
    if (!g_Memory.IsValid(address))
        return 0.f;

    return g_Memory.Read<float>(address + Offsets::Player::MaxZoomDistance);
}

float Cheat::Player::GetMinZoomDistance() const
{
    if (!g_Memory.IsValid(address))
        return 0.f;

    return g_Memory.Read<float>(address + Offsets::Player::MinZoomDistance);
}

bool Cheat::Player::IsLocalPlayer() const
{
    if (!g_Memory.IsValid(address))
        return false;

    if (!Cheat::Globals::Players || !g_Memory.IsValid(Cheat::Globals::Players->address))
        return false;

    std::uint64_t local_ptr = g_Memory.Read<std::uint64_t>(
        Cheat::Globals::Players->address + Offsets::Player::LocalPlayer);
    return local_ptr == address;
}

std::uint64_t Cheat::Player::GetCharacterAddress() const
{
    if (!g_Memory.IsValid(address))
        return 0;

    return g_Memory.Read<std::uint64_t>(address + Offsets::Player::ModelInstance);
}

std::shared_ptr<Cheat::Instance> Cheat::Player::GetCharacter() const
{
    std::uint64_t char_addr = GetCharacterAddress();
    if (!g_Memory.IsValid(char_addr))
        return nullptr;

    return std::make_shared<Cheat::Instance>(char_addr);
}
