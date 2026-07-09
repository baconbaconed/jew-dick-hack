#define IMGUI_DEFINE_MATH_OPERATORS
#include "widgets.h"
#include "../resources/fonts/fonts.h"
#include "../colors/colors.h"
#include "imgui/imgui_internal.h"
#include <algorithm>

namespace {
    constexpr float k_box_size = 12.0f;
    constexpr float k_label_spacing = 7.0f;
    constexpr float k_box_offset_y = 6.0f;
    constexpr int k_checked_gradient_rows = 8;

    ImU32 lerp_color(ImU32 from, ImU32 to, float t) {
        return ImGui::ColorConvertFloat4ToU32(ImLerp(
            ImGui::ColorConvertU32ToFloat4(from),
            ImGui::ColorConvertU32ToFloat4(to),
            t));
    }

    void draw_checkbox_fill(ImDrawList* draw_list, const ImRect& fill_rect, float check_t, bool hovered) {
        const int fill_l = static_cast<int>(ImFloor(fill_rect.Min.x));
        const int fill_t = static_cast<int>(ImFloor(fill_rect.Min.y));
        const int fill_r = static_cast<int>(ImCeil(fill_rect.Max.x)) - 1;
        const int fill_b = static_cast<int>(ImCeil(fill_rect.Max.y)) - 1;
        const int fill_h = fill_b - fill_t + 1;
        if (fill_h <= 0) {
            return;
        }

        const float clamped_check = ImClamp(check_t, 0.0f, 1.0f);
        ImU32 base_color = colors::widget_track_u32();
        if (hovered && clamped_check < 1.0f) {
            base_color = colors::widget_track_hover_u32();
        }

        const int rows = fill_h;
        for (int i = 0; i < rows; ++i) {
            const int gradient_index = std::min(i, k_checked_gradient_rows - 1);
            const ImU32 checked_color = colors::accent_gradient_row(gradient_index, k_checked_gradient_rows);
            const ImU32 row_color = lerp_color(base_color, checked_color, clamped_check);
            draw_list->AddRectFilled(
                ImVec2(static_cast<float>(fill_l), static_cast<float>(fill_t + i)),
                ImVec2(static_cast<float>(fill_r + 1), static_cast<float>(fill_t + i + 1)),
                row_color);
        }
    }

    void draw_checkbox_box(ImDrawList* draw_list, const ImVec2& min, const ImVec2& max, float check_t, bool hovered) {
        const ImRect outer_rect(min, max);
        const ImRect inner_rect(
            ImVec2(outer_rect.Min.x + 1.0f, outer_rect.Min.y + 1.0f),
            ImVec2(outer_rect.Max.x - 1.0f, outer_rect.Max.y - 1.0f));
        const ImRect fill_rect(
            ImVec2(inner_rect.Min.x + 1.0f, inner_rect.Min.y + 1.0f),
            ImVec2(inner_rect.Max.x - 1.0f, inner_rect.Max.y - 1.0f));

        draw_checkbox_fill(draw_list, fill_rect, check_t, hovered);
        draw_list->AddRect(inner_rect.Min, inner_rect.Max, colors::widget_inline_u32(), 0.0f, 0, 1.0f);
        draw_list->AddRect(outer_rect.Min, outer_rect.Max, colors::widget_outline_u32(), 0.0f, 0, 1.0f);
    }
}

namespace widgets {
    bool checkbox(const char* label, bool* value) {
        if (value == nullptr) {
            return false;
        }

        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window == nullptr || window->SkipItems) {
            return false;
        }

        ImGuiContext& g = *GImGui;
        const char* text = label != nullptr ? label : "";
        const ImGuiID id = window->GetID(text);
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        ImFont* label_font = fonts::tahoma != nullptr ? fonts::tahoma : ImGui::GetFont();
        const float label_font_size = fonts::tahoma && fonts::tahoma->LegacySize > 0.0f
            ? fonts::tahoma->LegacySize
            : 13.0f;

        const ImVec2 label_size = label_font->CalcTextSizeA(label_font_size, FLT_MAX, 0.0f, text);
        const float total_w = text[0] != '\0'
            ? k_box_size + k_label_spacing + label_size.x
            : k_box_size;
        const ImVec2 total_size(total_w, k_box_size + k_box_offset_y);
        const ImRect bounds(pos, pos + total_size);

        ImGui::ItemSize(total_size);
        if (!ImGui::ItemAdd(bounds, id)) {
            return false;
        }

        bool hovered = false;
        bool held = false;
        const bool pressed = ImGui::ButtonBehavior(bounds, id, &hovered, &held);
        if (pressed) {
            *value = !*value;
        }

        float* anim_hover = window->StateStorage.GetFloatRef(id, 0.0f);
        *anim_hover = ImLerp(*anim_hover, (hovered || *value) ? 1.0f : 0.0f, 15.0f * g.IO.DeltaTime);

        float* anim_check = window->StateStorage.GetFloatRef(id + 1, 0.0f);
        *anim_check = ImLerp(*anim_check, *value ? 1.0f : 0.0f, 15.0f * g.IO.DeltaTime);

        const ImVec2 box_min(pos.x, pos.y + k_box_offset_y);
        const ImVec2 box_max(box_min.x + k_box_size, box_min.y + k_box_size);
        draw_checkbox_box(draw_list, box_min, box_max, *anim_check, hovered);

        if (text[0] != '\0') {
            const ImU32 label_color = colors::label_u32(*anim_hover);
            const ImVec2 text_pos(
                ImFloor(box_max.x + k_label_spacing),
                ImFloor(box_min.y + (k_box_size - label_size.y) * 0.5f));
            draw_outlined_text(draw_list, label_font, label_font_size, text_pos, label_color, text);
        }

        return pressed;
    }
}
