#include "../../Roblox/Engine/Classes/Classes.h"
#include "LocalMods.h"
#include "../../Memory/Memory.h"
#include "../../Roblox/Engine/Offsets/Offsets.h"
#include <Windows.h>

#undef GetClassName

namespace Cheat {
    namespace Features {

        void LocalMods::Noclip(std::uint64_t character, bool enabled)
        {
            if (!enabled || !g_Memory.IsValid(character)) return;

            Cheat::Instance ch(character);
            for (const auto& part : ch.GetChildren()) {
                const std::string cls = part.GetClassName();
                if (cls == "Part" || cls == "MeshPart" || cls == "UnionOperation" ||
                    cls == "BasePart" || cls == "TrussPart" || cls == "WedgePart" ||
                    cls == "CornerWedgePart") {
                    Cheat::BasePart(part.address).SetCanCollide(false);
                }
            }
        }

        void LocalMods::InfiniteJump(std::uint64_t humanoid, bool enabled)
        {
            if (!enabled || !g_Memory.IsValid(humanoid)) return;

            static bool s_prev = false;
            const bool down = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
            if (down && !s_prev)
                g_Memory.Write<bool>(humanoid + Offsets::Humanoid::Jump, true);
            s_prev = down;
        }

    }
}
