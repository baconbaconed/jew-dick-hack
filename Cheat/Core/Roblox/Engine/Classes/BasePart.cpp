#include "Classes.h"
#include "../../../Memory/Memory.h"
#include "../Offsets/Offsets.h"

static std::uint64_t GetPrimitive(std::uint64_t base_part_addr)
{
    return g_Memory.Read<std::uint64_t>(base_part_addr + Offsets::BasePart::Primitive);
}

Vector3 Cheat::BasePart::GetPosition() const
{
    if (!g_Memory.IsValid(address))
        return {};

    std::uint64_t prim = GetPrimitive(address);
    if (!g_Memory.IsValid(prim))
        return {};

    return g_Memory.Read<Vector3>(prim + Offsets::Primitive::Position);
}

Vector3 Cheat::BasePart::GetSize() const
{
    if (!g_Memory.IsValid(address))
        return {};

    std::uint64_t prim = GetPrimitive(address);
    if (!g_Memory.IsValid(prim))
        return {};

    return g_Memory.Read<Vector3>(prim + Offsets::Primitive::Size);
}

Matrix4x4 Cheat::BasePart::GetRotation() const
{
    if (!g_Memory.IsValid(address))
        return {};

    std::uint64_t prim = GetPrimitive(address);
    if (!g_Memory.IsValid(prim))
        return {};

    float rot[9];
    g_Memory.ReadRaw(prim + Offsets::Primitive::Rotation, &rot, sizeof(rot));

    return Matrix4x4(
        rot[0], rot[1], rot[2], 0.0f,
        rot[3], rot[4], rot[5], 0.0f,
        rot[6], rot[7], rot[8], 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    );
}

Vector3 Cheat::BasePart::GetAssemblyLinearVelocity() const
{
    if (!g_Memory.IsValid(address))
        return {};

    std::uint64_t prim = GetPrimitive(address);
    if (!g_Memory.IsValid(prim))
        return {};

    return g_Memory.Read<Vector3>(prim + Offsets::Primitive::AssemblyLinearVelocity);
}

Vector3 Cheat::BasePart::GetAssemblyAngularVelocity() const
{
    if (!g_Memory.IsValid(address))
        return {};

    std::uint64_t prim = GetPrimitive(address);
    if (!g_Memory.IsValid(prim))
        return {};

    return g_Memory.Read<Vector3>(prim + Offsets::Primitive::AssemblyAngularVelocity);
}

void Cheat::BasePart::SetPosition(const Vector3& pos) const
{
    if (!g_Memory.IsValid(address))
        return;

    std::uint64_t prim = GetPrimitive(address);
    if (!g_Memory.IsValid(prim))
        return;

    g_Memory.Write<Vector3>(prim + Offsets::Primitive::Position, pos);
}

void Cheat::BasePart::SetAssemblyLinearVelocity(const Vector3& vel) const
{
    if (!g_Memory.IsValid(address))
        return;

    std::uint64_t prim = GetPrimitive(address);
    if (!g_Memory.IsValid(prim))
        return;

    g_Memory.Write<Vector3>(prim + Offsets::Primitive::AssemblyLinearVelocity, vel);
}

void Cheat::BasePart::SetCanCollide(bool value) const
{
    if (!g_Memory.IsValid(address))
        return;

    std::uint64_t prim = GetPrimitive(address);
    if (!g_Memory.IsValid(prim))
        return;

    std::uint8_t flags = g_Memory.Read<std::uint8_t>(prim + Offsets::Primitive::Flags);
    const std::uint8_t bit = Offsets::PrimitiveFlags::CanCollide;
    const std::uint8_t next = value ? (flags | bit) : (flags & ~bit);
    if (next != flags)
        g_Memory.Write<std::uint8_t>(prim + Offsets::Primitive::Flags, next);
}

Color3 Cheat::BasePart::GetColor() const
{
    if (!g_Memory.IsValid(address))
        return {};

    return g_Memory.Read<Color3>(address + Offsets::BasePart::Color3);
}

float Cheat::BasePart::GetTransparency() const
{
    if (!g_Memory.IsValid(address))
        return 0.f;

    return g_Memory.Read<float>(address + Offsets::BasePart::Transparency);
}

float Cheat::BasePart::GetReflectance() const
{
    if (!g_Memory.IsValid(address))
        return 0.f;

    return g_Memory.Read<float>(address + Offsets::BasePart::Reflectance);
}

bool Cheat::BasePart::IsAnchored() const
{
    if (!g_Memory.IsValid(address))
        return false;

    std::uint64_t prim = GetPrimitive(address);
    if (!g_Memory.IsValid(prim))
        return false;

    std::uint8_t flags = g_Memory.Read<std::uint8_t>(prim + Offsets::Primitive::Flags);
    return (flags & Offsets::PrimitiveFlags::Anchored) != 0;
}

bool Cheat::BasePart::CanCollide() const
{
    if (!g_Memory.IsValid(address))
        return false;

    std::uint64_t prim = GetPrimitive(address);
    if (!g_Memory.IsValid(prim))
        return false;

    std::uint8_t flags = g_Memory.Read<std::uint8_t>(prim + Offsets::Primitive::Flags);
    return (flags & Offsets::PrimitiveFlags::CanCollide) != 0;
}

bool Cheat::BasePart::CastShadow() const
{
    if (!g_Memory.IsValid(address))
        return false;

    return g_Memory.Read<bool>(address + Offsets::BasePart::CastShadow);
}
