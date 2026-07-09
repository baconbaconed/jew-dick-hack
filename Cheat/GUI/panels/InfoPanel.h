#pragma once
#define IMGUI_DEFINE_MATH_OPERATORS
#include "../imgui/imgui.h"
#include <cstdint>

namespace Cheat::GUI {

    class InfoPanel {
    public:

        static void Render(float child_w, float avail_h);

        static void RenderDetailWindow(float info_x, float info_y,
                                       float info_w, float info_h,
                                       float alpha);

    private:
        inline static std::uint64_t s_selected = 0;
    };

}
