#pragma once
#define IMGUI_DEFINE_MATH_OPERATORS
#include "../../../GUI/imgui/imgui.h"

namespace Cheat::Visuals {

    class ESPPreview {
    public:

        static void Initialize();
        static void Shutdown();

        static void Render();
    };

}
