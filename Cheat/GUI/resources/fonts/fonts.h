#pragma once

#include "imgui/imgui.h"
#include <cmath>

namespace fonts {
    extern ImFont* tahoma_bold;
    extern ImFont* tahoma;

    extern ImFont* esp;
    extern ImFont* esp_bold;

    void load(ImGuiIO& io);

    inline float snap_px(float size) {
        if (size < 8.0f) size = 8.0f;
        return std::floor(size + 0.5f);
    }
}
