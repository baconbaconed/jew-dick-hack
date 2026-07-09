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
