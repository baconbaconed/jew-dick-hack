#pragma once
#include "../../Roblox/Engine/Classes/Classes.h"
#include "../../PlayerHandler/PlayerHandler.h"
#include "../../../GUI/imgui/imgui.h"

namespace Cheat {
    namespace Features {
        class Aim {
        public:
            static void Render();

            static std::uint64_t CurrentTarget();

            static int     CurrentAimPart();
            static Vector3 CurrentAimPoint();

            static void mouse();
            static void viewport();
        };
    }
}
