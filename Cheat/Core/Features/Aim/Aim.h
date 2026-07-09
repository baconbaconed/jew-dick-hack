#pragma once
#include "../../Roblox/Engine/Classes/Classes.h"
#include "../../PlayerHandler/PlayerHandler.h"
#include "../../../GUI/imgui/imgui.h"

namespace Cheat {
    namespace Features {
        class Aim {
        public:
            static void Render();

            static void mouse();
            static void viewport();
        };
    }
}
