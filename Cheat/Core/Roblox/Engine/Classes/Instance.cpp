#include "Classes.h"
#include "../../../Memory/Memory.h"
#include "../Offsets/Offsets.h"
#undef GetClassName

std::string Cheat::Instance::GetName() const
{
    if (!g_Memory.IsValid(this->address))
        return "Unknown";

    std::uint64_t name = g_Memory.Read<std::uint64_t>(this->address + Offsets::Instance::Name);
    if (g_Memory.IsValid(name))
        return g_Memory.ReadString(name);

    return "Unknown";
}

std::string Cheat::Instance::GetClassName() const
{
    if (!g_Memory.IsValid(this->address))
        return "Unknown";

    std::uint64_t class_descriptor = g_Memory.Read<std::uint64_t>(this->address + Offsets::Instance::ClassDescriptor);
    if (!g_Memory.IsValid(class_descriptor))
        return "Unknown";

    std::uint64_t class_name = g_Memory.Read<std::uint64_t>(class_descriptor + Offsets::Instance::ClassName);
    if (g_Memory.IsValid(class_name))
        return g_Memory.ReadString(class_name);

    return "Unknown";
}

std::vector<Cheat::Instance> Cheat::Instance::GetChildren() const
{
    if (!address || !g_Memory.IsValid(address)) return {};

    std::uint64_t children_ptr = g_Memory.Read<std::uint64_t>(address + Offsets::Instance::ChildrenStart);
    if (!g_Memory.IsValid(children_ptr)) return {};

    std::uint64_t start = g_Memory.Read<std::uint64_t>(children_ptr);
    std::uint64_t end = g_Memory.Read<std::uint64_t>(children_ptr + Offsets::Instance::ChildrenEnd);

    if (!g_Memory.IsValid(start) || !g_Memory.IsValid(end) || start >= end) return {};

    std::vector<Cheat::Instance> children;
    children.reserve(32);

    int count = 0;
    for (std::uint64_t ptr = start; ptr < end && count < 10000; ptr += 16, ++count)
    {
        std::uint64_t child_addr = g_Memory.Read<std::uint64_t>(ptr);
        if (g_Memory.IsValid(child_addr))
            children.emplace_back(child_addr);
    }

    return children;
}

std::shared_ptr<Cheat::Instance> Cheat::Instance::FindFirstChild(std::string child) const
{
    for (const auto& c : GetChildren())
    {
        if (c.GetName() == child)
            return std::make_shared<Cheat::Instance>(c);
    }

    return nullptr;
}

std::shared_ptr<Cheat::Instance> Cheat::Instance::GetParent() const
{
    if (!g_Memory.IsValid(address))
        return nullptr;

    std::uint64_t parent_addr = g_Memory.Read<std::uint64_t>(address + Offsets::Instance::Parent);
    if (!g_Memory.IsValid(parent_addr))
        return nullptr;

    return std::make_shared<Cheat::Instance>(parent_addr);
}
