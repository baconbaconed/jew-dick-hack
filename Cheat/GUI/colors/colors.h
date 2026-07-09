#pragma once

#include "imgui/imgui.h"

namespace colors {
    extern ImVec4 accent;
    extern ImVec4 text_active;
    extern ImVec4 text_inactive;

    extern ImVec4 outer_border;
    extern ImVec4 inner_border;
    extern ImVec4 panel_fill;

    extern ImVec4 content_outer_border;
    extern ImVec4 content_inner_border;
    extern ImVec4 content_fill;
    extern ImVec4 child_fill;

    ImU32 accent_u32(float alpha = 1.0f);
    ImU32 accent_gradient_row(int row, int row_count, float alpha = 1.0f);
    ImU32 header_gradient_row(int row, int row_count, float alpha = 1.0f);
    ImU32 title_gradient_row(int row, int row_count, float alpha = 1.0f);
    ImU32 text_active_u32(float alpha = 1.0f);
    ImU32 text_inactive_u32(float alpha = 1.0f);
    ImU32 widget_outline_u32(float alpha = 1.0f);
    ImU32 widget_inline_u32(float alpha = 1.0f);
    ImU32 widget_track_u32(float alpha = 1.0f);
    ImU32 widget_track_hover_u32(float alpha = 1.0f);
    ImU32 label_u32(float hover_t, float alpha = 1.0f);

    void apply_style();
    void draw_panel_background(float alpha = 1.0f);
}
