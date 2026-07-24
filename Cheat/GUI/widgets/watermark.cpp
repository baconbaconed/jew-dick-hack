#include "widgets.h"
#include "../colors/colors.h"
#include "../resources/fonts/fonts.h"
#include "imgui/imgui_internal.h"
#include <cstdio>

namespace {
    constexpr ImVec2 k_position(10.0f, 10.0f);
    constexpr ImVec2 k_padding(8.0f, 5.0f);
    constexpr float k_segment_gap = 8.0f;

    constexpr ImU32 k_outline = IM_COL32(0, 0, 0, 255);
    constexpr ImU32 k_text_muted = IM_COL32(100, 100, 100, 255);
    constexpr ImU32 k_text_bright = IM_COL32(255, 255, 255, 255);

    ImU32 with_alpha(ImU32 color, float alpha) {
        ImVec4 value = ImGui::ColorConvertU32ToFloat4(color);
        value.w *= ImClamp(alpha, 0.0f, 1.0f);
        return ImGui::ColorConvertFloat4ToU32(value);
    }

    float measure_text(ImFont* font, float font_size, const char* text) {
        if (text == nullptr || text[0] == '\0') {
            return 0.0f;
        }
        if (font != nullptr) {
            return font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, text).x;
        }
        return ImGui::CalcTextSize(text).x;
    }
}

namespace widgets {
    void watermark(const char* username, float alpha) {
        (void)username;

        alpha = ImClamp(alpha, 0.0f, 1.0f);
        if (alpha <= 0.0f) {
            return;
        }

        ImDrawList* draw_list = ImGui::GetForegroundDrawList();

        ImFont* brand_font = fonts::tahoma_bold != nullptr ? fonts::tahoma_bold
                                                          : (fonts::tahoma != nullptr ? fonts::tahoma : ImGui::GetFont());
        ImFont* meta_font = fonts::tahoma != nullptr ? fonts::tahoma : brand_font;
        const float brand_font_size = fonts::tahoma_bold && fonts::tahoma_bold->LegacySize > 0.0f
            ? fonts::tahoma_bold->LegacySize
            : 13.0f;
        const float meta_font_size = fonts::tahoma && fonts::tahoma->LegacySize > 0.0f
            ? fonts::tahoma->LegacySize
            : brand_font_size;

        const widgets::text_span brand_spans[] = {
            {"jew",    with_alpha(colors::accent_u32(), alpha)},
            {"sploit", with_alpha(k_text_bright, alpha)},
        };

        char build_buf[64];
        ImFormatString(build_buf, IM_ARRAYSIZE(build_buf), "build: %s", __DATE__);

        const float brand_jew_w = measure_text(brand_font, brand_font_size, brand_spans[0].text);
        const float brand_sploit_w = measure_text(brand_font, brand_font_size, brand_spans[1].text);
        const float brand_w = brand_jew_w + brand_sploit_w;
        const float build_w = measure_text(meta_font, meta_font_size, build_buf);
        const float text_h = brand_font->CalcTextSizeA(brand_font_size, FLT_MAX, 0.0f, "A").y;
        const float meta_text_h = meta_font->CalcTextSizeA(meta_font_size, FLT_MAX, 0.0f, "A").y;

        const float content_w = brand_w + k_segment_gap + build_w;
        const float total_w = k_padding.x * 2.0f + content_w;
        const float total_h = k_padding.y * 2.0f + text_h;

        const float intro_slide = (1.0f - alpha) * 12.0f;
        const ImVec2 origin(k_position.x - intro_slide, k_position.y);
        const ImVec2 rect_max(origin.x + total_w, origin.y + total_h);

        const ImU32 fill_top = with_alpha(ImGui::ColorConvertFloat4ToU32(colors::panel_fill), alpha);
        const ImU32 fill_bot = with_alpha(ImGui::ColorConvertFloat4ToU32(colors::child_fill), alpha);
        draw_list->AddRectFilledMultiColor(origin, rect_max, fill_top, fill_top, fill_bot, fill_bot);
        draw_list->AddRect(origin, rect_max, with_alpha(k_outline, alpha), 0.0f, 0, 1.0f);
        draw_list->AddRect(
            ImVec2(origin.x + 1.0f, origin.y + 1.0f),
            ImVec2(rect_max.x - 1.0f, rect_max.y - 1.0f),
            with_alpha(ImGui::ColorConvertFloat4ToU32(colors::inner_border), alpha),
            0.0f,
            0,
            1.0f);

        const float brand_text_y = ImFloor(origin.y + k_padding.y);
        const float meta_text_y = ImFloor(origin.y + k_padding.y + (text_h - meta_text_h) * 0.5f);
        float cursor_x = origin.x + k_padding.x;

        const float phase = static_cast<float>(ImGui::GetTime()) * 2.4f;
        const float wavelength = 70.0f;
        const ImU32 highlight = with_alpha(IM_COL32(210, 225, 255, 255), alpha);

        widgets::draw_outlined_text_spans_shimmer(
            draw_list,
            brand_font,
            brand_font_size,
            ImVec2(ImFloor(cursor_x), brand_text_y),
            brand_spans,
            IM_ARRAYSIZE(brand_spans),
            highlight,
            phase,
            wavelength);
        cursor_x += brand_w + k_segment_gap;

        draw_outlined_text(
            draw_list,
            meta_font,
            meta_font_size,
            ImVec2(ImFloor(cursor_x), meta_text_y),
            with_alpha(k_text_muted, alpha),
            build_buf);
    }
}
