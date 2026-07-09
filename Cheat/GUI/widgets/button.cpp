#define IMGUI_DEFINE_MATH_OPERATORS
#include "widgets.h"
#include "../resources/fonts/fonts.h"
#include "../colors/colors.h"
#include "imgui/imgui_internal.h"

namespace {
    constexpr float k_button_height = 16.0f;
    constexpr float k_text_pad_x = 10.0f;
    constexpr float k_input_pad_x = 5.0f;
    constexpr float k_input_pad_y = 2.0f;

    ImFont* label_font() {
        return fonts::tahoma != nullptr ? fonts::tahoma : ImGui::GetFont();
    }

    float label_font_size() {
        return fonts::tahoma && fonts::tahoma->LegacySize > 0.0f ? fonts::tahoma->LegacySize : 13.0f;
    }

    void draw_framed_box(ImDrawList* draw_list, const ImVec2& min, const ImVec2& max) {
        const ImRect outer_rect(min, max);
        const ImRect inner_rect(
            ImVec2(outer_rect.Min.x + 1.0f, outer_rect.Min.y + 1.0f),
            ImVec2(outer_rect.Max.x - 1.0f, outer_rect.Max.y - 1.0f));
        const ImRect fill_rect(
            ImVec2(inner_rect.Min.x + 1.0f, inner_rect.Min.y + 1.0f),
            ImVec2(inner_rect.Max.x - 1.0f, inner_rect.Max.y - 1.0f));

        draw_list->AddRectFilled(fill_rect.Min, fill_rect.Max, colors::widget_track_u32());
        draw_list->AddRect(inner_rect.Min, inner_rect.Max, colors::widget_inline_u32(), 0.0f, 0, 1.0f);
        draw_list->AddRect(outer_rect.Min, outer_rect.Max, colors::widget_outline_u32(), 0.0f, 0, 1.0f);
    }
}

namespace widgets {
    bool button(const char* label, const ImVec2& size_arg) {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window == nullptr || window->SkipItems) {
            return false;
        }

        ImGuiContext& g = *GImGui;
        const char* text = label != nullptr ? label : "";

        ImFont* font = label_font();
        const float font_size = label_font_size();
        const ImVec2 text_size = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, text);

        const float width = size_arg.x > 0.0f ? size_arg.x : text_size.x + k_text_pad_x * 2.0f;
        const float height = size_arg.y > 0.0f ? size_arg.y : k_button_height;

        const ImVec2 pos = ImGui::GetCursorScreenPos();
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        const ImVec2 size(width, height);
        const ImRect bb(pos, pos + size);

        ImGui::ItemSize(size);
        const ImGuiID id = window->GetID(text);
        if (!ImGui::ItemAdd(bb, id)) {
            return false;
        }

        bool hovered = false;
        bool held = false;
        const bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);

        float* anim_hover = window->StateStorage.GetFloatRef(id, 0.0f);
        *anim_hover = ImLerp(*anim_hover, hovered ? 1.0f : 0.0f, 15.0f * g.IO.DeltaTime);

        draw_framed_box(draw_list, pos, pos + size);

        if (*anim_hover > 0.01f) {
            const ImRect inner_rect(
                ImVec2(pos.x + 2.0f, pos.y + 2.0f),
                ImVec2(pos.x + width - 2.0f, pos.y + height - 2.0f));
            draw_list->AddRectFilled(
                inner_rect.Min, inner_rect.Max,
                colors::widget_track_hover_u32(*anim_hover));
        }

        const ImVec2 text_pos(
            ImFloor(pos.x + (width - text_size.x) * 0.5f),
            ImFloor(pos.y + (height - text_size.y) * 0.5f));
        draw_outlined_text(draw_list, font, font_size, text_pos, colors::label_u32(*anim_hover), text);

        return pressed;
    }

    bool input_text(const char* id, const char* hint, char* buffer, int buffer_size,
                    float width, ImGuiInputTextFlags flags) {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window == nullptr || window->SkipItems || buffer == nullptr) {
            return false;
        }

        const ImVec2 pos = ImGui::GetCursorScreenPos();
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        if (width <= 0.0f) {
            width = ImMax(60.0f, ImGui::GetContentRegionAvail().x);
        }

        const ImVec2 box_min = pos;
        const ImVec2 box_max(pos.x + width, pos.y + k_button_height);
        draw_framed_box(draw_list, box_min, box_max);

        ImGui::SetCursorScreenPos(ImVec2(box_min.x + k_input_pad_x, box_min.y + k_input_pad_y));
        ImGui::PushItemWidth(width - k_input_pad_x * 2.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, colors::text_active);
        ImGui::PushStyleColor(ImGuiCol_TextDisabled, colors::text_inactive);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 1.0f));

        const bool result = hint != nullptr
            ? ImGui::InputTextWithHint(id, hint, buffer, static_cast<size_t>(buffer_size), flags)
            : ImGui::InputText(id, buffer, static_cast<size_t>(buffer_size), flags);

        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(6);
        ImGui::PopItemWidth();
        ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + k_button_height));

        return result;
    }
}
