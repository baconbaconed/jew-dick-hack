#include "tabs.h"
#include "widgets.h"
#include "imgui/imgui_internal.h"

namespace {
    constexpr ImU32 k_tab_active_text = IM_COL32(51, 122, 231, 255);
    constexpr ImU32 k_tab_inactive_text = IM_COL32(0x80, 0x80, 0x80, 255);
    constexpr ImU32 k_tab_inactive_hover_text = IM_COL32(0xd0, 0xd0, 0xd0, 255);

    constexpr ImU32 k_tab_gradient[] = {
        IM_COL32(28, 29, 28, 255),
        IM_COL32(28, 28, 28, 255),
        IM_COL32(28, 28, 28, 255),
        IM_COL32(26, 27, 26, 255),
        IM_COL32(26, 27, 26, 255),
        IM_COL32(27, 27, 26, 255),
        IM_COL32(27, 27, 26, 255),
        IM_COL32(24, 24, 25, 255),
        IM_COL32(24, 24, 24, 255),
        IM_COL32(24, 24, 24, 255),
        IM_COL32(22, 22, 23, 255),
        IM_COL32(22, 22, 23, 255),
        IM_COL32(22, 23, 23, 255),
        IM_COL32(22, 23, 23, 255),
        IM_COL32(20, 20, 21, 255),
        IM_COL32(20, 20, 21, 255),
        IM_COL32(21, 21, 20, 255),
    };

    constexpr int k_tab_gradient_count = IM_ARRAYSIZE(k_tab_gradient);
    constexpr int k_max_tabs = 8;

    struct tab_frame {
        int outer_l = 0;
        int outer_t = 0;
        int outer_r = 0;
        int outer_b = 0;
        int inner_l = 0;
        int inner_t = 0;
        int inner_r = 0;
        int inner_b = 0;
        ImRect fill{};
    };

    tab_frame make_tab_frame(float tab_left, float tab_top, float tab_width, int panel_top_outer, int panel_top_inner) {
        tab_frame frame{};
        frame.outer_l = static_cast<int>(ImFloor(tab_left));
        frame.outer_t = static_cast<int>(ImFloor(tab_top));
        frame.outer_r = static_cast<int>(ImFloor(tab_left + tab_width)) - 1;
        frame.outer_b = panel_top_outer;
        frame.inner_l = frame.outer_l + 1;
        frame.inner_t = frame.outer_t + 1;
        frame.inner_r = frame.outer_r - 1;
        frame.inner_b = panel_top_inner;
        frame.fill = ImRect(
            ImVec2(static_cast<float>(frame.inner_l + 1), static_cast<float>(frame.inner_t + 1)),
            ImVec2(static_cast<float>(frame.inner_r), static_cast<float>(panel_top_outer)));
        return frame;
    }

    void paint_hline(ImDrawList* draw_list, int x0, int x1, int y, ImU32 color) {
        if (x1 < x0) {
            return;
        }
        draw_list->AddRectFilled(
            ImVec2(static_cast<float>(x0), static_cast<float>(y)),
            ImVec2(static_cast<float>(x1 + 1), static_cast<float>(y + 1)),
            color);
    }

    void paint_vline(ImDrawList* draw_list, int x, int y0, int y1, ImU32 color) {
        if (y1 < y0) {
            return;
        }
        draw_list->AddRectFilled(
            ImVec2(static_cast<float>(x), static_cast<float>(y0)),
            ImVec2(static_cast<float>(x + 1), static_cast<float>(y1 + 1)),
            color);
    }

    void draw_tab_gradient(ImDrawList* draw_list, const ImRect& rect) {
        const float height = rect.GetHeight();
        if (draw_list == nullptr || height <= 0.0f) {
            return;
        }

        for (int g = 0; g < k_tab_gradient_count - 1; ++g) {
            const float y0 = rect.Min.y + (height * static_cast<float>(g)) / static_cast<float>(k_tab_gradient_count - 1);
            const float y1 = rect.Min.y + (height * static_cast<float>(g + 1)) / static_cast<float>(k_tab_gradient_count - 1);
            draw_list->AddRectFilledMultiColor(
                ImVec2(rect.Min.x, y0),
                ImVec2(rect.Max.x, y1),
                k_tab_gradient[g],
                k_tab_gradient[g],
                k_tab_gradient[g + 1],
                k_tab_gradient[g + 1]);
        }
    }

    void draw_tab_sides(ImDrawList* draw_list, const tab_frame& f, ImU32 outer_col, ImU32 inner_col) {
        paint_hline(draw_list, f.outer_l, f.outer_r, f.outer_t, outer_col);
        paint_vline(draw_list, f.outer_l, f.outer_t, f.outer_b, outer_col);
        paint_vline(draw_list, f.outer_r, f.outer_t, f.outer_b, outer_col);

        paint_hline(draw_list, f.inner_l, f.inner_r, f.inner_t, inner_col);
        paint_vline(draw_list, f.inner_l, f.inner_t, f.inner_b, inner_col);
        paint_vline(draw_list, f.inner_r, f.inner_t, f.inner_b, inner_col);
    }

    void paint_panel_top_with_tab_gap(
        ImDrawList* draw_list,
        int seg_l,
        int seg_r,
        int y,
        int gap_l,
        int gap_r,
        ImU32 color) {
        if (gap_l > seg_l) {
            paint_hline(draw_list, seg_l, gap_l - 1, y, color);
        }
        if (gap_r < seg_r) {
            paint_hline(draw_list, gap_r + 1, seg_r, y, color);
        }
    }

    void draw_content_panel(
        ImDrawList* draw_list,
        const ImVec2& panel_min,
        const ImVec2& panel_max,
        ImU32 fill,
        ImU32 outer_border,
        ImU32 inner_border,
        float tab_start_x,
        float tab_top,
        float tab_width,
        float tab_spacing,
        int label_count,
        int active_index) {
        const int panel_top_outer = static_cast<int>(ImFloor(panel_min.y));
        const int panel_top_inner = panel_top_outer + 1;
        const int panel_outer_l = static_cast<int>(ImFloor(panel_min.x));
        const int panel_outer_r = static_cast<int>(ImCeil(panel_max.x)) - 1;
        const int panel_outer_b = static_cast<int>(ImCeil(panel_max.y)) - 1;
        const int panel_inner_l = panel_outer_l + 1;
        const int panel_inner_r = panel_outer_r - 1;
        const int panel_inner_b = panel_outer_b - 1;

        const tab_frame active = make_tab_frame(
            tab_start_x + static_cast<float>(active_index) * (tab_width + tab_spacing),
            tab_top,
            tab_width,
            panel_top_outer,
            panel_top_inner);

        draw_list->AddRectFilled(
            ImVec2(static_cast<float>(panel_inner_l + 1), static_cast<float>(panel_top_outer)),
            ImVec2(static_cast<float>(panel_inner_r), static_cast<float>(panel_inner_b + 1)),
            fill);

        paint_vline(draw_list, panel_outer_l, panel_top_outer, panel_outer_b, outer_border);
        paint_vline(draw_list, panel_outer_r, panel_top_outer, panel_outer_b, outer_border);
        paint_hline(draw_list, panel_outer_l, panel_outer_r, panel_outer_b, outer_border);

        paint_vline(draw_list, panel_inner_l, panel_top_inner, panel_inner_b, inner_border);
        paint_vline(draw_list, panel_inner_r, panel_top_inner, panel_inner_b, inner_border);
        paint_hline(draw_list, panel_inner_l, panel_inner_r, panel_inner_b, inner_border);

        paint_panel_top_with_tab_gap(
            draw_list,
            panel_outer_l,
            panel_outer_r,
            panel_top_outer,
            active.outer_l,
            active.outer_r,
            outer_border);
        paint_panel_top_with_tab_gap(
            draw_list,
            panel_inner_l,
            panel_inner_r,
            panel_top_inner,
            active.inner_l,
            active.inner_r,
            inner_border);
    }
}

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
        ImU32 panel_inner_border) {
        if (draw_list == nullptr || labels == nullptr || label_count <= 0) {
            return active_index;
        }

        if (active_index < 0 || active_index >= label_count) {
            active_index = 0;
        }

        if (font == nullptr) {
            font = ImGui::GetFont();
        }
        if (font_active == nullptr) {
            font_active = font;
        }
        if (font_size <= 0.0f) {
            font_size = ImGui::GetFontSize();
        }

        const int panel_top_outer = static_cast<int>(ImFloor(panel_min.y));
        const int panel_top_inner = panel_top_outer + 1;
        int clicked_index = active_index;
        static float tab_hover[k_max_tabs] = {};

        for (int i = 0; i < label_count; ++i) {
            const ImVec2 tab_min(tab_start_x + static_cast<float>(i) * (tab_width + tab_spacing), tab_top);
            ImGui::SetCursorScreenPos(tab_min);
            ImGui::PushID(i);
            if (ImGui::InvisibleButton("tab", ImVec2(tab_width, tab_height))) {
                clicked_index = i;
            }
            ImGui::PopID();

            tab_hover[i] = ImLerp(
                tab_hover[i],
                ImGui::IsItemHovered() ? 1.0f : 0.0f,
                15.0f * ImGui::GetIO().DeltaTime);
        }

        auto draw_tab_visual = [&](int i, bool is_active, bool draw_label) {
            const float tab_left = tab_start_x + static_cast<float>(i) * (tab_width + tab_spacing);
            const tab_frame frame = make_tab_frame(tab_left, tab_top, tab_width, panel_top_outer, panel_top_inner);

            draw_tab_gradient(draw_list, frame.fill);

            if (is_active) {
                draw_list->AddRectFilled(
                    ImVec2(static_cast<float>(frame.inner_l), static_cast<float>(panel_top_outer)),
                    ImVec2(static_cast<float>(frame.inner_r + 1), static_cast<float>(panel_top_inner + 1)),
                    panel_fill);
            } else {
                draw_list->AddRectFilled(
                    ImVec2(static_cast<float>(frame.outer_l), static_cast<float>(panel_top_outer - 1)),
                    ImVec2(static_cast<float>(frame.outer_r + 1), static_cast<float>(panel_top_inner + 1)),
                    panel_fill);
            }

            draw_tab_sides(draw_list, frame, panel_outer_border, panel_inner_border);

            if (!draw_label) {
                return;
            }

            const ImU32 text_color = is_active
                ? k_tab_active_text
                : ImGui::ColorConvertFloat4ToU32(ImLerp(
                      ImGui::ColorConvertU32ToFloat4(k_tab_inactive_text),
                      ImGui::ColorConvertU32ToFloat4(k_tab_inactive_hover_text),
                      tab_hover[i]));

            ImFont* label_font =  is_active ? font_active : font;
            const ImVec2 text_size = label_font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, labels[i]);
            const ImVec2 text_pos(
                ImFloor(static_cast<float>(frame.outer_l) + (tab_width - text_size.x) * 0.5f),
                ImFloor(static_cast<float>(frame.outer_t) + (tab_height - text_size.y) * 0.5f));
            widgets::draw_outlined_text(draw_list, label_font, font_size, text_pos, text_color, labels[i]);
        };

        for (int i = 0; i < label_count; ++i) {
            if (i == clicked_index) {
                continue;
            }
            draw_tab_visual(i, false, true);
        }

        draw_content_panel(
            draw_list,
            panel_min,
            panel_max,
            panel_fill,
            panel_outer_border,
            panel_inner_border,
            tab_start_x,
            tab_top,
            tab_width,
            tab_spacing,
            label_count,
            clicked_index);

        draw_tab_visual(clicked_index, true, true);

        return clicked_index;
    }
}
