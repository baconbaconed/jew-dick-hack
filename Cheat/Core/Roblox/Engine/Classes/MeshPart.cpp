#include "Classes.h"
#include "../../../Memory/Memory.h"
#include "../Offsets/Offsets.h"

std::string Cheat::MeshPart::GetMeshId() const
{
    if (!g_Memory.IsValid(address))
        return "Unknown";

    std::uint64_t mesh_ptr = g_Memory.Read<std::uint64_t>(address + Offsets::MeshPart::MeshId);
    if (!g_Memory.IsValid(mesh_ptr))
        return "Unknown";

    return g_Memory.ReadString(mesh_ptr);
}

std::string Cheat::MeshPart::GetTexture() const
{
    if (!g_Memory.IsValid(address))
        return "Unknown";

    std::uint64_t tex_ptr = g_Memory.Read<std::uint64_t>(address + Offsets::MeshPart::Texture);
    if (!g_Memory.IsValid(tex_ptr))
        return "Unknown";

    return g_Memory.ReadString(tex_ptr);
}
