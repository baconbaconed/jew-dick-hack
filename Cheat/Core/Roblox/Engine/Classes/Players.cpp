#include "Classes.h"
#include "../../../Memory/Memory.h"
#include "../Offsets/Offsets.h"

std::vector<std::shared_ptr<Cheat::Instance>> Cheat::Players::GetPlayers() const
{
    std::vector<std::shared_ptr<Cheat::Instance>> result;

    for (const auto& child : GetChildren())
    {

        result.push_back(std::make_shared<Cheat::Instance>(child.address));
    }

    return result;
}
