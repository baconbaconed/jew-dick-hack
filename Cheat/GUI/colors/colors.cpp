#define IMGUI_DEFINE_MATH_OPERATORS
#include "colors.h"
#include <algorithm>
#include "imgui/imgui_internal.h"

namespace {
    ImU32 with_alpha(const ImVec4& color, float alpha) {
        ImVec4 adjusted = color;
        adjusted.w = std::clamp(adjusted.w * alpha, 0.0f, 1.0f);
        return ImGui::GetColorU32(adjusted);
    }

    ImRect shrink_rect(const ImRect& rect, float amount) {
        return ImRect(
            ImVec2(rect.Min.x + amount, rect.Min.y + amount),
            ImVec2(rect.Max.x - amount, rect.Max.y - amount));
    }

    ImU32 with_alpha_color(const ImVec4& color, float alpha) {
        ImVec4 adjusted = color;
        adjusted.w *= std::clamp(alpha, 0.f, 1.f);
        return ImGui::ColorConvertFloat4ToU32(adjusted);
    }

    void Set4(float out[4], float r, float g, float b, float a = 1.0f) {
        out[0] = r; out[1] = g; out[2] = b; out[3] = a;
    }

    void Set4u8(float out[4], int r, int g, int b, int a = 255) {
        out[0] = r / 255.0f;
        out[1] = g / 255.0f;
        out[2] = b / 255.0f;
        out[3] = a / 255.0f;
    }

    ImVec4 From4(const float c[4]) {
        return ImVec4(c[0], c[1], c[2], c[3]);
    }

    static const char* k_theme_names[] = {
        "default",
        "gamesense",
        "fatality",
        "neverlose",
        "onetap",
        "primordial",
        "nixware",
        "aimware",
        "custom",
    };
    static_assert(sizeof(k_theme_names) / sizeof(k_theme_names[0]) == colors::Theme_Count, "themes");
}

namespace colors {

    ImVec4 accent = ImVec4(51.0f / 255.0f, 122.0f / 255.0f, 231.0f / 255.0f, 1.0f);
    ImVec4 text_active = ImVec4(1.f, 1.f, 1.f, 1.f);
    ImVec4 text_inactive = ImVec4(136.0f / 255.0f, 136.0f / 255.0f, 136.0f / 255.0f, 1.0f);

    ImVec4 outer_border = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
    ImVec4 inner_border = ImVec4(31.0f / 255.0f, 30.0f / 255.0f, 31.0f / 255.0f, 1.0f);
    ImVec4 panel_fill = ImVec4(17.0f / 255.0f, 17.0f / 255.0f, 16.0f / 255.0f, 1.0f);

    ImVec4 content_outer_border = ImVec4(31.0f / 255.0f, 30.0f / 255.0f, 31.0f / 255.0f, 1.0f);
    ImVec4 content_inner_border = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
    ImVec4 content_fill = ImVec4(21.0f / 255.0f, 21.0f / 255.0f, 20.0f / 255.0f, 1.0f);
    ImVec4 child_fill = ImVec4(15.0f / 255.0f, 14.0f / 255.0f, 14.0f / 255.0f, 1.0f);

    const char* const* ThemeNames() { return k_theme_names; }

    void ApplyPreset(int theme, float accent_c[4], float text_active_c[4], float text_inactive_c[4],
                     float outer_border_c[4], float inner_border_c[4], float panel_fill_c[4],
                     float content_outer_c[4], float content_inner_c[4],
                     float content_fill_c[4], float child_fill_c[4]) {
        theme = (std::clamp)(theme, 0, Theme_Count - 1);
        if (theme == Theme_Custom) return;

        Set4u8(text_active_c,   255, 255, 255);
        Set4u8(text_inactive_c, 136, 136, 136);
        Set4u8(outer_border_c,    0,   0,   0);
        Set4u8(inner_border_c,   31,  30,  31);
        Set4u8(panel_fill_c,     17,  17,  16);
        Set4u8(content_outer_c,  31,  30,  31);
        Set4u8(content_inner_c,   0,   0,   0);
        Set4u8(content_fill_c,   21,  21,  20);
        Set4u8(child_fill_c,     15,  14,  14);

        switch (theme) {
        case Theme_Gamesense:
            Set4u8(accent_c,        143, 188,  90);
            Set4u8(panel_fill_c,     12,  12,  12);
            Set4u8(content_fill_c,   17,  17,  17);
            Set4u8(child_fill_c,     10,  10,  10);
            Set4u8(inner_border_c,   40,  40,  40);
            Set4u8(content_outer_c,  40,  40,  40);
            Set4u8(text_inactive_c, 110, 110, 110);
            break;
        case Theme_Fatality:
            Set4u8(accent_c,        232,  64,  94);
            Set4u8(panel_fill_c,     18,  12,  18);
            Set4u8(content_fill_c,   24,  16,  24);
            Set4u8(child_fill_c,     14,   9,  14);
            Set4u8(inner_border_c,   48,  28,  42);
            Set4u8(content_outer_c,  48,  28,  42);
            Set4u8(text_inactive_c, 150, 120, 140);
            break;
        case Theme_Neverlose:
            Set4u8(accent_c,         70, 145, 255);
            Set4u8(panel_fill_c,     14,  16,  22);
            Set4u8(content_fill_c,   18,  22,  30);
            Set4u8(child_fill_c,     11,  13,  18);
            Set4u8(inner_border_c,   36,  44,  58);
            Set4u8(content_outer_c,  36,  44,  58);
            Set4u8(text_inactive_c, 120, 135, 160);
            break;
        case Theme_Onetap:
            Set4u8(accent_c,        255, 140,  40);
            Set4u8(panel_fill_c,     16,  14,  12);
            Set4u8(content_fill_c,   22,  19,  16);
            Set4u8(child_fill_c,     12,  11,   9);
            Set4u8(inner_border_c,   48,  38,  28);
            Set4u8(content_outer_c,  48,  38,  28);
            Set4u8(text_inactive_c, 150, 130, 110);
            break;
        case Theme_Primordial:
            Set4u8(accent_c,        168,  85, 247);
            Set4u8(panel_fill_c,     16,  12,  22);
            Set4u8(content_fill_c,   22,  16,  30);
            Set4u8(child_fill_c,     12,   9,  16);
            Set4u8(inner_border_c,   48,  34,  64);
            Set4u8(content_outer_c,  48,  34,  64);
            Set4u8(text_inactive_c, 145, 130, 170);
            break;
        case Theme_Nixware:
            Set4u8(accent_c,         40, 210, 190);
            Set4u8(panel_fill_c,     12,  16,  16);
            Set4u8(content_fill_c,   16,  22,  22);
            Set4u8(child_fill_c,      9,  13,  13);
            Set4u8(inner_border_c,   30,  48,  46);
            Set4u8(content_outer_c,  30,  48,  46);
            Set4u8(text_inactive_c, 110, 145, 140);
            break;
        case Theme_Aimware:
            Set4u8(accent_c,         55, 120, 220);
            Set4u8(panel_fill_c,     20,  20,  22);
            Set4u8(content_fill_c,   26,  26,  30);
            Set4u8(child_fill_c,     15,  15,  18);
            Set4u8(inner_border_c,   42,  42,  50);
            Set4u8(content_outer_c,  42,  42,  50);
            Set4u8(text_inactive_c, 130, 130, 145);
            break;
        case Theme_Default:
        default:
            Set4u8(accent_c,         51, 122, 231);
            break;
        }
    }

    void SyncFromSettings(const float accent_c[4], const float text_active_c[4], const float text_inactive_c[4],
                          const float outer_border_c[4], const float inner_border_c[4], const float panel_fill_c[4],
                          const float content_outer_c[4], const float content_inner_c[4],
                          const float content_fill_c[4], const float child_fill_c[4]) {
        accent              = From4(accent_c);
        text_active         = From4(text_active_c);
        text_inactive       = From4(text_inactive_c);
        outer_border        = From4(outer_border_c);
        inner_border        = From4(inner_border_c);
        panel_fill          = From4(panel_fill_c);
        content_outer_border = From4(content_outer_c);
        content_inner_border = From4(content_inner_c);
        content_fill        = From4(content_fill_c);
        child_fill          = From4(child_fill_c);
        apply_style();
    }

    ImU32 accent_u32(float alpha) {
        ImVec4 color = accent;
        color.w *= std::clamp(alpha, 0.f, 1.f);
        return ImGui::ColorConvertFloat4ToU32(color);
    }

    ImU32 accent_gradient_row(int row, int row_count, float alpha) {
        row_count = std::max(1, row_count);
        row = std::clamp(row, 0, row_count - 1);
        const float t = static_cast<float>(row) / static_cast<float>(row_count - 1);
        ImVec4 bright = accent;
        ImVec4 dark = accent;
        dark.x *= 0.82f;
        dark.y *= 0.82f;
        dark.z *= 0.82f;
        ImVec4 mixed = ImLerp(bright, dark, t);
        mixed.w = accent.w * std::clamp(alpha, 0.f, 1.f);
        return ImGui::ColorConvertFloat4ToU32(mixed);
    }

    ImU32 header_gradient_row(int row, int row_count, float alpha) {
        row_count = std::max(1, row_count);
        row = std::clamp(row, 0, row_count - 1);
        const float t = static_cast<float>(row) / static_cast<float>(row_count - 1);
        ImVec4 top = ImLerp(accent, panel_fill, 0.35f);
        ImVec4 bottom = child_fill;
        ImVec4 mixed = ImLerp(top, bottom, t);
        return with_alpha_color(mixed, alpha);
    }

    ImU32 title_gradient_row(int row, int row_count, float alpha) {
        row_count = std::max(1, row_count);
        row = std::clamp(row, 0, row_count - 1);
        const float t = static_cast<float>(row) / static_cast<float>(row_count - 1);
        ImVec4 top = text_active;
        top.x = std::min(1.f, top.x + (1.f - top.x) * 0.12f);
        top.y = std::min(1.f, top.y + (1.f - top.y) * 0.12f);
        top.z = std::min(1.f, top.z + (1.f - top.z) * 0.12f);
        const ImVec4 bottom = ImLerp(text_active, text_inactive, 0.28f);
        return with_alpha_color(ImLerp(top, bottom, t), alpha);
    }

    ImU32 text_active_u32(float alpha) {
        return with_alpha_color(text_active, alpha);
    }

    ImU32 text_inactive_u32(float alpha) {
        return with_alpha_color(text_inactive, alpha);
    }

    ImU32 widget_outline_u32(float alpha) {
        return with_alpha_color(outer_border, alpha);
    }

    ImU32 widget_inline_u32(float alpha) {
        return with_alpha_color(inner_border, alpha);
    }

    ImU32 widget_track_u32(float alpha) {
        return with_alpha_color(child_fill, alpha);
    }

    ImU32 widget_track_hover_u32(float alpha) {
        ImVec4 hover = ImLerp(child_fill, accent, 0.18f);
        return with_alpha_color(hover, alpha);
    }

    ImU32 label_u32(float hover_t, float alpha) {
        const float t = std::clamp(hover_t, 0.f, 1.f);
        ImVec4 mixed = ImLerp(text_inactive, text_active, t);
        return with_alpha_color(mixed, alpha);
    }

    void apply_style() {
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowBorderSize = 0.0f;
        style.WindowBorderHoverPadding = 6.0f;
        style.WindowPadding = ImVec2(8.0f, 8.0f);
        style.FrameBorderSize = 1.0f;
        style.Colors[ImGuiCol_WindowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        style.Colors[ImGuiCol_ChildBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        style.Colors[ImGuiCol_Border] = outer_border;
        style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    }

    void draw_panel_background(float alpha) {
        if (ImGui::IsWindowCollapsed()) {
            return;
        }

        alpha = std::clamp(alpha, 0.0f, 1.0f);
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 window_pos = ImGui::GetWindowPos();
        ImVec2 window_size = ImGui::GetWindowSize();
        ImVec2 window_pos_br(window_pos.x + window_size.x, window_pos.y + window_size.y);
        ImRect outer_rect(window_pos, window_pos_br);

        const float border_thickness = 1.0f;
        ImRect inner_border_rect = shrink_rect(outer_rect, border_thickness);
        ImRect fill_rect = shrink_rect(inner_border_rect, border_thickness);

        ImU32 outer_col = with_alpha(outer_border, alpha);
        ImU32 inner_col = with_alpha(inner_border, alpha);
        ImU32 fill_col = with_alpha(panel_fill, alpha);

        draw_list->AddRect(outer_rect.Min, outer_rect.Max, outer_col, 0.0f, 0, border_thickness);
        draw_list->AddRect(inner_border_rect.Min, inner_border_rect.Max, inner_col, 0.0f, 0, border_thickness);
        draw_list->AddRectFilled(fill_rect.Min, fill_rect.Max, fill_col);
    }
}
