#pragma once

#include "imgui/imgui.h"

struct ImFont;

namespace widgets {
    int draw_tab_bar(
        ImDrawList* draw_list,
        const ImVec2& panel_min,
        const ImVec2& panel_max,
        float tab_start_x,
        float tab_top,
        int active_index,
        const char* const* labels,
        int label_count,
        float tab_width,
        float tab_height,
        float tab_spacing,
        ImFont* font,
        ImFont* font_active,
        float font_size,
        ImU32 panel_fill,
        ImU32 panel_outer_border,
        ImU32 panel_inner_border);
}
