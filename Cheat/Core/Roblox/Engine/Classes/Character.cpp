#include "Classes.h"
#include "../../../Memory/Memory.h"
#include "../Offsets/Offsets.h"

std::shared_ptr<Cheat::Instance> Cheat::Character::GetHumanoid() const
{
    return FindFirstChild("Humanoid");
}

std::shared_ptr<Cheat::Instance> Cheat::Character::GetRootPart() const
{
    return FindFirstChild("HumanoidRootPart");
}

std::shared_ptr<Cheat::Instance> Cheat::Character::GetHead() const
{
    return FindFirstChild("Head");
}
