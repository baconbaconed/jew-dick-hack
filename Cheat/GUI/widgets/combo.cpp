#include "widgets.h"
#include "../colors/colors.h"
#include "../resources/fonts/fonts.h"
#include "imgui/imgui_internal.h"
#include <cstdio>

namespace {
    constexpr float k_combo_width_default = 161.0f;
    constexpr float k_combo_width_min = 80.0f;
    constexpr float k_widget_side_pad = 6.0f;

    float combo_width() {
        const float avail = ImGui::GetContentRegionAvail().x;
        if (avail <= 0.0f) {
            return k_combo_width_default;
        }
        return ImMax(k_combo_width_min, avail - k_widget_side_pad);
    }
    constexpr float k_combo_height = 16.0f;
    constexpr float k_label_combo_gap = 3.0f;
    constexpr float k_label_text_offset_x = 1.0f;
    constexpr float k_item_height = 16.0f;
    constexpr float k_text_pad_x = 5.0f;
    constexpr float k_selected_marker_gap = 2.0f;

    constexpr ImU32 k_outline = IM_COL32(0, 0, 0, 255);
    constexpr ImU32 k_inline_border = IM_COL32(24, 24, 25, 255);
    constexpr ImU32 k_fill = IM_COL32(18, 19, 19, 255);
    constexpr ImU32 k_text_inactive = IM_COL32(100, 100, 100, 255);
    constexpr ImU32 k_text_hover = IM_COL32(255, 255, 255, 255);

    void draw_framed_box(ImDrawList* draw_list, const ImVec2& min, const ImVec2& max) {
        const ImRect outer_rect(min, max);
        const ImRect inner_rect(
            ImVec2(outer_rect.Min.x + 1.0f, outer_rect.Min.y + 1.0f),
            ImVec2(outer_rect.Max.x - 1.0f, outer_rect.Max.y - 1.0f));
        const ImRect fill_rect(
            ImVec2(inner_rect.Min.x + 1.0f, inner_rect.Min.y + 1.0f),
            ImVec2(inner_rect.Max.x - 1.0f, inner_rect.Max.y - 1.0f));

        draw_list->AddRectFilled(fill_rect.Min, fill_rect.Max, k_fill);
        draw_list->AddRect(inner_rect.Min, inner_rect.Max, k_inline_border, 0.0f, 0, 1.0f);
        draw_list->AddRect(outer_rect.Min, outer_rect.Max, k_outline, 0.0f, 0, 1.0f);
    }
}

namespace widgets {
    bool combo(const char* label, int* current_item, const char* const items[], int items_count) {
        if (current_item == nullptr || items == nullptr || items_count <= 0) {
            return false;
        }

        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window == nullptr || window->SkipItems) {
            return false;
        }

        ImGuiContext& g = *GImGui;
        const char* text = label != nullptr ? label : "";
        const ImGuiID id = window->GetID(text);

        if (*current_item < 0) {
            *current_item = 0;
        }
        if (*current_item >= items_count) {
            *current_item = items_count - 1;
        }

        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        ImFont* label_font = fonts::tahoma != nullptr ? fonts::tahoma : ImGui::GetFont();
        const float label_font_size = fonts::tahoma && fonts::tahoma->LegacySize > 0.0f
            ? fonts::tahoma->LegacySize
            : 13.0f;

        const ImVec2 label_size = label_font->CalcTextSizeA(label_font_size, FLT_MAX, 0.0f, text);
        const float row_h = label_size.y;
        const float total_h = row_h + k_label_combo_gap + k_combo_height;
        const float box_width = combo_width();
        const ImVec2 total_size(box_width, total_h);

        const ImVec2 box_min(pos.x, pos.y + row_h + k_label_combo_gap);
        const ImVec2 box_max(box_min.x + box_width, box_min.y + k_combo_height);
        const ImRect box_bb(box_min, box_max);
        const ImRect total_bb(pos, pos + total_size);

        ImGui::ItemSize(total_size);
        if (!ImGui::ItemAdd(total_bb, id, &box_bb)) {
            return false;
        }

        bool hovered = false;
        bool held = false;
        const bool pressed = ImGui::ButtonBehavior(box_bb, id, &hovered, &held);

        char popup_id[128];
        ImFormatString(popup_id, IM_ARRAYSIZE(popup_id), "%s##popup", text);
        if (pressed) {
            ImGui::OpenPopup(popup_id);
        }

        float* anim_hover = window->StateStorage.GetFloatRef(id, 0.0f);
        *anim_hover = ImLerp(*anim_hover, (hovered || ImGui::IsPopupOpen(popup_id)) ? 1.0f : 0.0f, 15.0f * g.IO.DeltaTime);

        const ImU32 label_color = ImGui::ColorConvertFloat4ToU32(ImLerp(
            ImGui::ColorConvertU32ToFloat4(k_text_inactive),
            ImGui::ColorConvertU32ToFloat4(k_text_hover),
            *anim_hover));

        if (text[0] != '\0') {
            draw_outlined_text(
                draw_list,
                label_font,
                label_font_size,
                ImVec2(ImFloor(pos.x + k_label_text_offset_x), ImFloor(pos.y)),
                label_color,
                text);
        }

        draw_framed_box(draw_list, box_min, box_max);

        const char* current_text = items[*current_item] != nullptr ? items[*current_item] : "";
        const ImVec2 preview_size = label_font->CalcTextSizeA(label_font_size, FLT_MAX, 0.0f, current_text);
        draw_outlined_text(
            draw_list,
            label_font,
            label_font_size,
            ImVec2(
                ImFloor(box_min.x + k_text_pad_x),
                ImFloor(box_min.y + (k_combo_height - preview_size.y) * 0.5f)),
            label_color,
            current_text);

        const bool is_open = ImGui::IsPopupOpen(popup_id);
        const char* icon = is_open ? "-" : "+";
        const ImVec2 icon_size = label_font->CalcTextSizeA(label_font_size, FLT_MAX, 0.0f, icon);
        draw_outlined_text(
            draw_list,
            label_font,
            label_font_size,
            ImVec2(
                ImFloor(box_max.x - icon_size.x - k_text_pad_x),
                ImFloor(box_min.y + (k_combo_height - icon_size.y) * 0.5f)),
            colors::accent_u32(),
            icon);

        ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 0.0f);

        bool value_changed = false;

        constexpr float k_popup_max_rows = 12.0f;
        const float content_h = items_count * k_item_height;
        const float popup_h = ImMin(content_h, k_item_height * k_popup_max_rows);
        const bool need_scroll = content_h > popup_h + 0.5f;

        ImGui::SetNextWindowPos(ImVec2(box_min.x, box_max.y + 1.0f));
        ImGui::SetNextWindowSize(ImVec2(box_width, popup_h));
        if (ImGui::BeginPopup(popup_id, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove)) {
            ImDrawList* popup_draw = ImGui::GetWindowDrawList();
            const ImVec2 popup_min = ImGui::GetWindowPos();
            const ImVec2 popup_max(
                popup_min.x + ImGui::GetWindowWidth(),
                popup_min.y + ImGui::GetWindowHeight());

            draw_framed_box(popup_draw, popup_min, popup_max);

            ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0.07f, 0.07f, 0.07f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, ImVec4(0.25f, 0.25f, 0.25f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImVec4(0.35f, 0.35f, 0.35f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, ImVec4(0.45f, 0.45f, 0.45f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 8.0f);

            ImGuiWindowFlags child_flags = ImGuiWindowFlags_NoBackground;
            if (!need_scroll)
                child_flags |= ImGuiWindowFlags_NoScrollbar;

            ImGui::SetCursorPos(ImVec2(0.0f, 0.0f));
            ImGui::BeginChild("##combo_list", ImVec2(box_width, popup_h), false, child_flags);
            ImDrawList* list_draw = ImGui::GetWindowDrawList();

            if (ImGui::IsWindowAppearing() && need_scroll && *current_item > 0) {
                ImGui::SetScrollY((float)*current_item * k_item_height - popup_h * 0.35f);
            }

            for (int i = 0; i < items_count; ++i) {
                const char* item_text = items[i] != nullptr ? items[i] : "";
                ImGui::SetCursorPosY(i * k_item_height);
                char item_id[64];
                ImFormatString(item_id, IM_ARRAYSIZE(item_id), "##ci%d", i);
                if (ImGui::InvisibleButton(item_id, ImVec2(box_width - (need_scroll ? 8.0f : 0.0f), k_item_height))) {
                    *current_item = i;
                    value_changed = true;
                    ImGui::CloseCurrentPopup();
                }

                const ImVec2 item_min = ImGui::GetItemRectMin();
                const bool item_hovered = ImGui::IsItemHovered();
                const bool item_selected = (*current_item == i);
                ImU32 item_color = k_text_inactive;
                if (item_selected) {
                    item_color = colors::accent_u32();
                } else if (item_hovered) {
                    item_color = k_text_hover;
                }

                const ImVec2 item_text_size = label_font->CalcTextSizeA(label_font_size, FLT_MAX, 0.0f, item_text);
                const float item_text_y = ImFloor(item_min.y + (k_item_height - item_text_size.y) * 0.5f);
                float item_text_x = item_min.x + k_text_pad_x;

                if (item_selected) {
                    constexpr const char* k_selected_marker = ">";
                    const ImVec2 marker_size = label_font->CalcTextSizeA(
                        label_font_size,
                        FLT_MAX,
                        0.0f,
                        k_selected_marker);
                    draw_outlined_text(
                        list_draw,
                        label_font,
                        label_font_size,
                        ImVec2(ImFloor(item_min.x + k_text_pad_x), item_text_y),
                        item_color,
                        k_selected_marker);
                    item_text_x += marker_size.x + k_selected_marker_gap;
                }

                draw_outlined_text(
                    list_draw,
                    label_font,
                    label_font_size,
                    ImVec2(ImFloor(item_text_x), item_text_y),
                    item_color,
                    item_text);
            }

            ImGui::EndChild();

            ImGui::PopStyleVar();
            ImGui::PopStyleColor(4);

            ImGui::EndPopup();
        }

        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(2);

        return value_changed;
    }

    static void build_multi_preview(
        char* buffer,
        int buffer_size,
        const bool* selected,
        const char* const items[],
        int items_count) {
        if (buffer == nullptr || buffer_size <= 0) {
            return;
        }

        buffer[0] = '\0';
        int written = 0;
        int selected_count = 0;

        for (int i = 0; i < items_count; ++i) {
            if (selected == nullptr || !selected[i]) {
                continue;
            }

            const char* item_text = items[i] != nullptr ? items[i] : "";
            if (item_text[0] == '\0') {
                continue;
            }

            if (written > 0) {
                written += ImFormatString(buffer + written, buffer_size - written, ", ");
            }
            written += ImFormatString(buffer + written, buffer_size - written, "%s", item_text);
            ++selected_count;
        }

        if (selected_count == 0) {
            ImFormatString(buffer, buffer_size, "none showing");
        }
    }

    bool multi_combo(const char* label, bool* selected, const char* const items[], int items_count) {
        if (selected == nullptr || items == nullptr || items_count <= 0) {
            return false;
        }

        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window == nullptr || window->SkipItems) {
            return false;
        }

        ImGuiContext& g = *GImGui;
        const char* text = label != nullptr ? label : "";
        const ImGuiID id = window->GetID(text);

        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        ImFont* label_font = fonts::tahoma != nullptr ? fonts::tahoma : ImGui::GetFont();
        const float label_font_size = fonts::tahoma && fonts::tahoma->LegacySize > 0.0f
            ? fonts::tahoma->LegacySize
            : 13.0f;

        const ImVec2 label_size = label_font->CalcTextSizeA(label_font_size, FLT_MAX, 0.0f, text);
        const float row_h = label_size.y;
        const float total_h = row_h + k_label_combo_gap + k_combo_height;
        const float box_width = combo_width();
        const ImVec2 total_size(box_width, total_h);

        const ImVec2 box_min(pos.x, pos.y + row_h + k_label_combo_gap);
        const ImVec2 box_max(box_min.x + box_width, box_min.y + k_combo_height);
        const ImRect box_bb(box_min, box_max);
        const ImRect total_bb(pos, pos + total_size);

        ImGui::ItemSize(total_size);
        if (!ImGui::ItemAdd(total_bb, id, &box_bb)) {
            return false;
        }

        bool hovered = false;
        bool held = false;
        const bool pressed = ImGui::ButtonBehavior(box_bb, id, &hovered, &held);

        char popup_id[128];
        ImFormatString(popup_id, IM_ARRAYSIZE(popup_id), "%s##multipopup", text);
        if (pressed) {
            ImGui::OpenPopup(popup_id);
        }

        float* anim_hover = window->StateStorage.GetFloatRef(id, 0.0f);
        *anim_hover = ImLerp(*anim_hover, (hovered || ImGui::IsPopupOpen(popup_id)) ? 1.0f : 0.0f, 15.0f * g.IO.DeltaTime);

        const ImU32 label_color = ImGui::ColorConvertFloat4ToU32(ImLerp(
            ImGui::ColorConvertU32ToFloat4(k_text_inactive),
            ImGui::ColorConvertU32ToFloat4(k_text_hover),
            *anim_hover));

        if (text[0] != '\0') {
            draw_outlined_text(
                draw_list,
                label_font,
                label_font_size,
                ImVec2(ImFloor(pos.x + k_label_text_offset_x), ImFloor(pos.y)),
                label_color,
                text);
        }

        draw_framed_box(draw_list, box_min, box_max);

        char preview_buf[128];
        build_multi_preview(preview_buf, IM_ARRAYSIZE(preview_buf), selected, items, items_count);

        const ImVec2 preview_size = label_font->CalcTextSizeA(label_font_size, FLT_MAX, 0.0f, preview_buf);
        const float preview_max_w = box_width - k_text_pad_x * 2.0f - 12.0f;
        const char* preview_draw = preview_buf;
        char preview_truncated[128];
        if (preview_size.x > preview_max_w) {
            ImFormatString(preview_truncated, IM_ARRAYSIZE(preview_truncated), "%s...", preview_buf);
            preview_draw = preview_truncated;
        }

        const ImVec2 preview_draw_size = label_font->CalcTextSizeA(label_font_size, FLT_MAX, 0.0f, preview_draw);
        draw_outlined_text(
            draw_list,
            label_font,
            label_font_size,
            ImVec2(
                ImFloor(box_min.x + k_text_pad_x),
                ImFloor(box_min.y + (k_combo_height - preview_draw_size.y) * 0.5f)),
            label_color,
            preview_draw);

        const bool is_open = ImGui::IsPopupOpen(popup_id);
        const char* icon = is_open ? "-" : "+";
        const ImVec2 icon_size = label_font->CalcTextSizeA(label_font_size, FLT_MAX, 0.0f, icon);
        draw_outlined_text(
            draw_list,
            label_font,
            label_font_size,
            ImVec2(
                ImFloor(box_max.x - icon_size.x - k_text_pad_x),
                ImFloor(box_min.y + (k_combo_height - icon_size.y) * 0.5f)),
            colors::accent_u32(),
            icon);

        bool value_changed = false;

        ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 0.0f);

        ImGui::SetNextWindowPos(ImVec2(box_min.x, box_max.y + 1.0f));
        ImGui::SetNextWindowSize(ImVec2(box_width, items_count * k_item_height));
        if (ImGui::BeginPopup(popup_id, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove)) {
            ImDrawList* popup_draw = ImGui::GetWindowDrawList();
            const ImVec2 popup_min = ImGui::GetWindowPos();
            const ImVec2 popup_max(
                popup_min.x + ImGui::GetWindowWidth(),
                popup_min.y + ImGui::GetWindowHeight());

            draw_framed_box(popup_draw, popup_min, popup_max);

            for (int i = 0; i < items_count; ++i) {
                const char* item_text = items[i] != nullptr ? items[i] : "";
                const bool item_shown = selected[i];

                char item_button_id[128];
                ImFormatString(item_button_id, IM_ARRAYSIZE(item_button_id), "%s##multi_%d", text, i);

                ImGui::SetCursorPosY(i * k_item_height);
                if (ImGui::InvisibleButton(item_button_id, ImVec2(box_width, k_item_height))) {
                    selected[i] = !selected[i];
                    value_changed = true;
                }

                const ImVec2 item_min = ImGui::GetItemRectMin();
                const bool item_hovered = ImGui::IsItemHovered();
                ImU32 item_color = k_text_inactive;
                if (item_shown) {
                    item_color = colors::accent_u32();
                } else if (item_hovered) {
                    item_color = k_text_hover;
                }

                const ImVec2 item_text_size = label_font->CalcTextSizeA(label_font_size, FLT_MAX, 0.0f, item_text);
                const float item_text_y = ImFloor(item_min.y + (k_item_height - item_text_size.y) * 0.5f);
                float item_text_x = item_min.x + k_text_pad_x;

                if (item_shown) {
                    constexpr const char* k_selected_marker = ">";
                    const ImVec2 marker_size = label_font->CalcTextSizeA(
                        label_font_size,
                        FLT_MAX,
                        0.0f,
                        k_selected_marker);
                    draw_outlined_text(
                        popup_draw,
                        label_font,
                        label_font_size,
                        ImVec2(ImFloor(item_min.x + k_text_pad_x), item_text_y),
                        item_color,
                        k_selected_marker);
                    item_text_x += marker_size.x + k_selected_marker_gap;
                }

                draw_outlined_text(
                    popup_draw,
                    label_font,
                    label_font_size,
                    ImVec2(ImFloor(item_text_x), item_text_y),
                    item_color,
                    item_text);
            }

            ImGui::EndPopup();
        }

        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(2);

        return pressed || value_changed;
    }
}
