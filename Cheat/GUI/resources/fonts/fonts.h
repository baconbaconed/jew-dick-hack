#pragma once

#include "imgui/imgui.h"

namespace fonts {
    extern ImFont* tahoma_bold;
    extern ImFont* tahoma;

    extern ImFont* esp;
    extern ImFont* esp_bold;

    void load(ImGuiIO& io);
}
