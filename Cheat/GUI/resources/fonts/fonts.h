#pragma once

#include "imgui/imgui.h"
#include "../../../Settings.h"
#include <cmath>

namespace fonts {
    extern ImFont* imgui;
    extern ImFont* tahoma_bold;

    extern ImFont* tahoma;
    extern ImFont* esp;
    extern ImFont* esp_bold;

    void load(ImGuiIO& io);

    inline ImFont* selected() {
        if (Cheat::g_Settings.esp.font == 1 && tahoma_bold)
            return tahoma_bold;
        if (imgui)
            return imgui;
        if (tahoma_bold)
            return tahoma_bold;
        return ImGui::GetFont();
    }

    inline ImFont* ui() {
        return selected();
    }

    inline ImFont* ui_bold() {
        return selected();
    }

    inline float ui_size(ImFont* font = nullptr) {
        ImFont* f = font ? font : ui();
        if (f && f->LegacySize > 0.0f)
            return f->LegacySize;
        return 13.0f;
    }

    inline float snap_px(float size) {
        if (size < 8.0f) size = 8.0f;
        return std::floor(size + 0.5f);
    }
}
