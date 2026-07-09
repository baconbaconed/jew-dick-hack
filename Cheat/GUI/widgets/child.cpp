#include "widgets.h"
#include "../colors/colors.h"
#include "imgui/imgui_internal.h"
#include <algorithm>
#include <vector>

namespace {
    struct child_context {
        ImVec2 cursor_start{};
        ImVec2 panel_size{};
    };

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

    struct child_layout {
        ImRect outer{};
        ImRect inner{};
        ImRect fill{};
    };

    std::vector<child_context>& child_stack() {
        static std::vector<child_context> stack;
        return stack;
    }

    child_layout make_child_layout(const ImVec2& origin, const ImVec2& size) {
        const int outer_l = static_cast<int>(ImFloor(origin.x));
        const int outer_t = static_cast<int>(ImFloor(origin.y));
        const int outer_r = outer_l + static_cast<int>(ImFloor(size.x));
        const int outer_b = outer_t + static_cast<int>(ImFloor(size.y));

        child_layout layout{};
        layout.outer = ImRect(
            ImVec2(static_cast<float>(outer_l), static_cast<float>(outer_t)),
            ImVec2(static_cast<float>(outer_r), static_cast<float>(outer_b)));
        layout.inner = ImRect(
            ImVec2(static_cast<float>(outer_l + 1), static_cast<float>(outer_t + 1)),
            ImVec2(static_cast<float>(outer_r - 1), static_cast<float>(outer_b - 1)));
        layout.fill = ImRect(
            ImVec2(static_cast<float>(outer_l + 2), static_cast<float>(outer_t + 1)),
            ImVec2(static_cast<float>(outer_r - 2), static_cast<float>(outer_b - 2)));
        return layout;
    }

    void draw_inner_border_no_top(const child_layout& layout, ImDrawList* draw_list, ImU32 color) {
        const int inner_l = static_cast<int>(layout.inner.Min.x);
        const int inner_t = static_cast<int>(layout.inner.Min.y);
        const int inner_r = static_cast<int>(layout.inner.Max.x) - 1;
        const int inner_b = static_cast<int>(layout.inner.Max.y) - 1;

        paint_vline(draw_list, inner_l, inner_t, inner_b, color);
        paint_vline(draw_list, inner_r, inner_t, inner_b, color);
        paint_hline(draw_list, inner_l, inner_r, inner_b, color);
    }

    constexpr ImU32 k_child_gradient[] = {
        IM_COL32(28, 29, 28, 255),
        IM_COL32(28, 28, 28, 255),
        IM_COL32(26, 27, 26, 255),
        IM_COL32(26, 27, 26, 255),
        IM_COL32(27, 27, 26, 255),
        IM_COL32(24, 24, 25, 255),
        IM_COL32(24, 24, 24, 255),
        IM_COL32(24, 24, 24, 255),
        IM_COL32(22, 22, 23, 255),
        IM_COL32(22, 23, 23, 255),
        IM_COL32(20, 20, 21, 255),
        IM_COL32(20, 20, 21, 255),
        IM_COL32(21, 21, 20, 255),
        IM_COL32(18, 18, 19, 255),
        IM_COL32(18, 19, 19, 255),
        IM_COL32(18, 19, 19, 255),
        IM_COL32(16, 17, 16, 255),
        IM_COL32(17, 17, 16, 255),
        IM_COL32(15, 14, 14, 255),
    };

    constexpr int k_child_gradient_rows = IM_ARRAYSIZE(k_child_gradient);

    bool has_child_title(const char* title) {
        return title != nullptr && title[0] != '\0';
    }

    void draw_child_gradient_rows(
        ImDrawList* draw_list,
        int fill_l,
        int fill_t,
        int fill_r,
        int fill_b) {
        const int fill_h = fill_b - fill_t + 1;
        const int gradient_rows = std::min(k_child_gradient_rows, fill_h);

        for (int i = 0; i < gradient_rows; ++i) {
            draw_list->AddRectFilled(
                ImVec2(static_cast<float>(fill_l), static_cast<float>(fill_t + i)),
                ImVec2(static_cast<float>(fill_r + 1), static_cast<float>(fill_t + i + 1)),
                k_child_gradient[i]);
        }
    }

    void draw_child_background(ImDrawList* draw_list, const child_layout& layout, ImU32 fill_color, bool draw_header) {
        const int fill_l = static_cast<int>(ImFloor(layout.fill.Min.x));
        const int fill_t = static_cast<int>(ImFloor(layout.fill.Min.y));
        const int fill_r = static_cast<int>(ImCeil(layout.fill.Max.x)) - 1;
        const int fill_b = static_cast<int>(ImCeil(layout.fill.Max.y)) - 1;

        draw_list->AddRectFilled(
            ImVec2(static_cast<float>(fill_l), static_cast<float>(fill_t)),
            ImVec2(static_cast<float>(fill_r + 1), static_cast<float>(fill_b + 1)),
            fill_color);

        if (!draw_header) {
            return;
        }

        const int header_b = fill_t + k_child_gradient_rows - 1;
        draw_child_gradient_rows(draw_list, fill_l, fill_t, fill_r, header_b);

        if (fill_b > header_b) {
            draw_list->AddRectFilled(
                ImVec2(static_cast<float>(fill_l), static_cast<float>(header_b + 1)),
                ImVec2(static_cast<float>(fill_r + 1), static_cast<float>(fill_b + 1)),
                fill_color);
        }
    }

    void draw_child_separator(ImDrawList* draw_list, const child_layout& layout, ImU32 inner_color) {
        const int fill_l = static_cast<int>(ImFloor(layout.fill.Min.x));
        const int fill_t = static_cast<int>(ImFloor(layout.fill.Min.y));
        const int fill_r = static_cast<int>(ImCeil(layout.fill.Max.x)) - 1;
        const int separator_y = fill_t + k_child_gradient_rows;
        paint_hline(draw_list, fill_l, fill_r, separator_y, inner_color);
    }

    void draw_child_title_on_gradient(
        ImDrawList* draw_list,
        const child_layout& layout,
        const char* title,
        ImFont* title_font,
        float title_font_size) {
        if (title == nullptr || title[0] == '\0') {
            return;
        }

        if (title_font == nullptr) {
            title_font = ImGui::GetFont();
        }
        if (title_font_size <= 0.0f) {
            title_font_size = ImGui::GetFontSize();
        }

        const int fill_l = static_cast<int>(ImFloor(layout.fill.Min.x));
        const int fill_t = static_cast<int>(ImFloor(layout.fill.Min.y));

        constexpr float k_title_pad_x = 6.0f;
        const ImVec2 text_size = title_font->CalcTextSizeA(title_font_size, FLT_MAX, 0.0f, title);
        const ImVec2 text_pos(
            ImFloor(static_cast<float>(fill_l) + k_title_pad_x),
            ImFloor(static_cast<float>(fill_t) + (static_cast<float>(k_child_gradient_rows) - text_size.y) * 0.5f));

        widgets::draw_outlined_text(
            draw_list,
            title_font,
            title_font_size,
            text_pos,
            IM_COL32(255, 255, 255, 255),
            title);
    }

    void draw_child_frame(
        ImDrawList* draw_list,
        const child_layout& layout,
        ImU32 outer_color,
        ImU32 inner_color,
        ImU32 fill_color,
        const char* title,
        ImFont* title_font,
        float title_font_size) {
        const bool draw_header = has_child_title(title);
        draw_child_background(draw_list, layout, fill_color, draw_header);
        if (draw_header) {
            draw_child_separator(draw_list, layout, inner_color);
        }

        const int outer_l = static_cast<int>(layout.outer.Min.x);
        const int outer_t = static_cast<int>(layout.outer.Min.y);
        const int outer_r = static_cast<int>(layout.outer.Max.x) - 1;
        const int outer_b = static_cast<int>(layout.outer.Max.y) - 1;

        paint_hline(draw_list, outer_l, outer_r, outer_t, outer_color);
        paint_vline(draw_list, outer_l, outer_t, outer_b, outer_color);
        paint_vline(draw_list, outer_r, outer_t, outer_b, outer_color);
        paint_hline(draw_list, outer_l, outer_r, outer_b, outer_color);

        draw_inner_border_no_top(layout, draw_list, inner_color);
        draw_child_title_on_gradient(draw_list, layout, title, title_font, title_font_size);
    }
}

namespace widgets {
    bool begin_child_panel(
        const char* id,
        const ImVec2& size,
        const char* title,
        ImFont* title_font,
        float title_font_size,
        const ImVec4* outer_border,
        const ImVec4* inner_border,
        const ImVec4* fill) {
        const ImVec2 cursor = ImGui::GetCursorPos();
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        const child_layout layout = make_child_layout(origin, size);

        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        const ImU32 outer_col = ImGui::GetColorU32(outer_border ? *outer_border : colors::outer_border);
        const ImU32 inner_col = ImGui::GetColorU32(inner_border ? *inner_border : colors::inner_border);
        const ImU32 fill_col = ImGui::GetColorU32(fill ? *fill : colors::child_fill);

        draw_child_frame(
            draw_list,
            layout,
            outer_col,
            inner_col,
            fill_col,
            title,
            title_font,
            title_font_size);

        const ImVec2 content_size(layout.fill.GetWidth(), layout.fill.GetHeight());
        const bool draw_header = has_child_title(title);
        const float header_height = draw_header ? static_cast<float>(k_child_gradient_rows) + 1.0f : 0.0f;
        constexpr float k_content_inset_y = 1.0f;

        const ImVec4 child_bg = fill ? *fill : colors::child_fill;
        ImGui::PushStyleColor(ImGuiCol_ChildBg, child_bg);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        child_stack().push_back(child_context{cursor, size});

        ImGui::SetCursorPos(ImVec2(cursor.x + 2.0f, cursor.y + k_content_inset_y + header_height));

        return ImGui::BeginChild(
            id,
            ImVec2(content_size.x, ImMax(0.0f, content_size.y - k_content_inset_y - header_height)),
            false,
            ImGuiWindowFlags_NoScrollbar);
    }

    void end_child_panel() {
        std::vector<child_context>& stack = child_stack();
        IM_ASSERT(!stack.empty());
        const child_context context = stack.back();
        stack.pop_back();

        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        ImGui::SetCursorPos(context.cursor_start);
        ImGui::Dummy(context.panel_size);
    }
}
