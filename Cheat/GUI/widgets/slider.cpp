#define IMGUI_DEFINE_MATH_OPERATORS
#include "widgets.h"
#include "../resources/fonts/fonts.h"
#include "../colors/colors.h"
#include "imgui/imgui_internal.h"
#include <algorithm>
#include <cstdio>

namespace {
    constexpr float k_slider_width_default = 161.0f;
    constexpr float k_slider_width_min = 80.0f;
    constexpr float k_widget_side_pad = 6.0f;
    constexpr float k_slider_height = 12.0f;
    constexpr float k_label_track_gap = 3.0f;
    constexpr float k_label_text_offset_x = 1.0f;
    constexpr float k_value_text_offset_x = 0.0f;
    constexpr int k_fill_gradient_count = 8;

    float slider_width() {
        const float avail = ImGui::GetContentRegionAvail().x;
        if (avail <= 0.0f)
            return k_slider_width_default;
        return ImMax(k_slider_width_min, avail - k_widget_side_pad);
    }

    void draw_vertical_gradient_fill(ImDrawList* draw_list, const ImRect& fill_rect) {
        const int fill_l = static_cast<int>(ImFloor(fill_rect.Min.x));
        const int fill_t = static_cast<int>(ImFloor(fill_rect.Min.y));
        const int fill_r = static_cast<int>(ImCeil(fill_rect.Max.x)) - 1;
        const int fill_b = static_cast<int>(ImCeil(fill_rect.Max.y)) - 1;
        const int fill_h = fill_b - fill_t + 1;
        if (fill_h <= 0)
            return;

        const int rows = std::min(k_fill_gradient_count, fill_h);
        for (int i = 0; i < rows; ++i) {
            draw_list->AddRectFilled(
                ImVec2(static_cast<float>(fill_l), static_cast<float>(fill_t + i)),
                ImVec2(static_cast<float>(fill_r + 1), static_cast<float>(fill_t + i + 1)),
                colors::accent_gradient_row(i, k_fill_gradient_count));
        }

        if (rows < fill_h) {
            draw_list->AddRectFilled(
                ImVec2(static_cast<float>(fill_l), static_cast<float>(fill_t + rows)),
                ImVec2(static_cast<float>(fill_r + 1), static_cast<float>(fill_b + 1)),
                colors::accent_gradient_row(k_fill_gradient_count - 1, k_fill_gradient_count));
        }
    }

    void draw_slider_track(ImDrawList* draw_list, const ImVec2& min, const ImVec2& max, float fill_t) {
        const ImRect outer_rect(min, max);
        const ImRect inner_rect(
            ImVec2(outer_rect.Min.x + 1.0f, outer_rect.Min.y + 1.0f),
            ImVec2(outer_rect.Max.x - 1.0f, outer_rect.Max.y - 1.0f));
        const ImRect fill_rect(
            ImVec2(inner_rect.Min.x + 1.0f, inner_rect.Min.y + 1.0f),
            ImVec2(inner_rect.Max.x - 1.0f, inner_rect.Max.y - 1.0f));

        draw_list->AddRectFilled(fill_rect.Min, fill_rect.Max, colors::widget_track_u32());

        const float clamped_t = ImClamp(fill_t, 0.0f, 1.0f);
        if (clamped_t > 0.0f) {
            const ImRect filled_rect(
                fill_rect.Min,
                ImVec2(fill_rect.Min.x + clamped_t * (fill_rect.Max.x - fill_rect.Min.x), fill_rect.Max.y));
            draw_vertical_gradient_fill(draw_list, filled_rect);
        }

        draw_list->AddRect(inner_rect.Min, inner_rect.Max, colors::widget_inline_u32(), 0.0f, 0, 1.0f);
        draw_list->AddRect(outer_rect.Min, outer_rect.Max, colors::widget_outline_u32(), 0.0f, 0, 1.0f);
    }

    float normalized_value(ImGuiDataType type, void* data, const void* v_min, const void* v_max) {
        if (type == ImGuiDataType_Float) {
            const float range = *static_cast<const float*>(v_max) - *static_cast<const float*>(v_min);
            if (range <= 0.0f)
                return 0.0f;
            return ImClamp((*static_cast<float*>(data) - *static_cast<const float*>(v_min)) / range, 0.0f, 1.0f);
        }

        const int range = *static_cast<const int*>(v_max) - *static_cast<const int*>(v_min);
        if (range <= 0)
            return 0.0f;
        return ImClamp(static_cast<float>(*static_cast<int*>(data) - *static_cast<const int*>(v_min)) / static_cast<float>(range), 0.0f, 1.0f);
    }

    bool slider_scalar(
        const char* label,
        ImGuiDataType data_type,
        void* data,
        const void* v_min,
        const void* v_max,
        const char* format) {
        if (data == nullptr)
            return false;

        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window == nullptr || window->SkipItems)
            return false;

        ImGuiContext& g = *GImGui;
        const char* text = label != nullptr ? label : "";
        if (format == nullptr)
            format = ImGui::DataTypeGetInfo(data_type)->PrintFmt;

        const ImGuiID id = window->GetID(text);
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        ImFont* label_font = fonts::tahoma != nullptr ? fonts::tahoma : ImGui::GetFont();
        const float label_font_size = fonts::tahoma && fonts::tahoma->LegacySize > 0.0f
            ? fonts::tahoma->LegacySize
            : 13.0f;

        const ImVec2 label_size = label_font->CalcTextSizeA(label_font_size, FLT_MAX, 0.0f, text);

        char value_buf[64];
        ImGui::DataTypeFormatString(value_buf, IM_ARRAYSIZE(value_buf), data_type, data, format);
        const ImVec2 value_size = label_font->CalcTextSizeA(label_font_size, FLT_MAX, 0.0f, value_buf);

        const float row_h = ImMax(label_size.y, value_size.y);
        const float total_h = row_h + k_label_track_gap + k_slider_height;
        const float track_width = slider_width();
        const ImVec2 total_size(track_width, total_h);

        const ImVec2 track_min(pos.x, pos.y + row_h + k_label_track_gap);
        const ImVec2 track_max(track_min.x + track_width, track_min.y + k_slider_height);
        const ImRect track_bb(track_min, track_max);
        const ImRect total_bb(pos, pos + total_size);

        ImGui::ItemSize(total_size);
        if (!ImGui::ItemAdd(total_bb, id, &track_bb))
            return false;

        const bool hovered = ImGui::ItemHoverable(track_bb, id, g.LastItemData.ItemFlags);
        const bool clicked = hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left, ImGuiInputFlags_None, id);
        const bool make_active = clicked || g.NavActivateId == id;

        if (make_active) {
            if (clicked)
                ImGui::SetKeyOwner(ImGuiKey_MouseLeft, id);
            ImGui::SetActiveID(id, window);
            ImGui::SetFocusID(id, window);
            ImGui::FocusWindow(window);
            g.ActiveIdUsingNavDirMask |= (1 << ImGuiDir_Left) | (1 << ImGuiDir_Right);
        }

        ImRect grab_bb;
        const bool value_changed = ImGui::SliderBehavior(
            track_bb,
            id,
            data_type,
            data,
            v_min,
            v_max,
            format,
            ImGuiSliderFlags_None,
            &grab_bb);

        if (value_changed)
            ImGui::MarkItemEdited(id);

        if (value_changed)
            ImGui::DataTypeFormatString(value_buf, IM_ARRAYSIZE(value_buf), data_type, data, format);

        const float relative_value = normalized_value(data_type, data, v_min, v_max);
        const bool held = g.ActiveId == id;

        float* anim_hover = window->StateStorage.GetFloatRef(id, 0.0f);
        *anim_hover = ImLerp(*anim_hover, (hovered || held) ? 1.0f : 0.0f, 15.0f * g.IO.DeltaTime);

        float* anim_value = window->StateStorage.GetFloatRef(id + 1, relative_value);
        *anim_value = ImLerp(*anim_value, relative_value, 15.0f * g.IO.DeltaTime);

        const ImU32 label_color = colors::label_u32(*anim_hover);

        if (text[0] != '\0') {
            widgets::draw_outlined_text(
                draw_list,
                label_font,
                label_font_size,
                ImVec2(ImFloor(pos.x + k_label_text_offset_x), ImFloor(pos.y)),
                label_color,
                text);
        }

        const ImVec2 display_value_size = label_font->CalcTextSizeA(label_font_size, FLT_MAX, 0.0f, value_buf);
        widgets::draw_outlined_text(
            draw_list,
            label_font,
            label_font_size,
            ImVec2(ImFloor(pos.x + track_width - display_value_size.x + k_value_text_offset_x), ImFloor(pos.y)),
            label_color,
            value_buf);

        draw_slider_track(draw_list, track_min, track_max, *anim_value);

        return value_changed;
    }
}

namespace widgets {
    bool slider_float(const char* label, float* value, float v_min, float v_max, const char* format) {
        if (value == nullptr)
            return false;
        if (format == nullptr)
            format = "%.3f";
        return slider_scalar(label, ImGuiDataType_Float, value, &v_min, &v_max, format);
    }

    bool slider_int(const char* label, int* value, int v_min, int v_max, const char* format) {
        if (value == nullptr)
            return false;
        if (format == nullptr)
            format = "%d";
        return slider_scalar(label, ImGuiDataType_S32, value, &v_min, &v_max, format);
    }
}
