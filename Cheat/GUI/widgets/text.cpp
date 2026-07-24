#include "widgets.h"
#include "../resources/fonts/fonts.h"
#include "imgui/imgui_internal.h"
#include <cmath>

namespace {
    ImU32 lerp_u32(ImU32 a, ImU32 b, float t) {
        const ImVec4 va = ImGui::ColorConvertU32ToFloat4(a);
        const ImVec4 vb = ImGui::ColorConvertU32ToFloat4(b);
        return ImGui::ColorConvertFloat4ToU32(ImLerp(va, vb, ImSaturate(t)));
    }

    float text_width(ImFont* font, float font_size, const char* text) {
        if (text == nullptr)
            return 0.0f;
        if (font != nullptr)
            return font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, text).x;
        return ImGui::CalcTextSize(text).x;
    }

    void draw_text_outlined(ImDrawList* draw_list, ImFont* font, float font_size, ImVec2 pos, ImU32 color, const char* text) {
        if (!draw_list || !text)
            return;

        font_size = fonts::snap_px(font_size);
        const float x = std::floor(pos.x);
        const float y = std::floor(pos.y);
        const ImU32 shadow = IM_COL32(0, 0, 0, 255);

        for (int i = -1; i <= 1; ++i) {
            for (int j = -1; j <= 1; ++j) {
                if (i == 0 && j == 0)
                    continue;
                if (font)
                    draw_list->AddText(font, font_size, ImVec2(x + i, y + j), shadow, text);
                else
                    draw_list->AddText(ImVec2(x + i, y + j), shadow, text);
            }
        }

        if (font)
            draw_list->AddText(font, font_size, ImVec2(x, y), color, text);
        else
            draw_list->AddText(ImVec2(x, y), color, text);
    }
}

namespace widgets {
    void draw_outlined_text(ImDrawList* draw_list, ImFont* font, float font_size, ImVec2 pos, ImU32 color, const char* text, float extra_width) {
        if (draw_list == nullptr || text == nullptr)
            return;

        if (font_size <= 0.0f)
            font_size = fonts::ui_size(font);
        if (!font)
            font = fonts::ui();

        if (extra_width > 0.0f) {
            const float natural_width = text_width(font, font_size, text);
            if (natural_width > 0.0f) {
                const float advance_scale = (natural_width + extra_width) / natural_width;
                char glyph[2] = {};
                float x = pos.x;
                for (const char* p = text; *p != '\0'; ++p) {
                    glyph[0] = *p;
                    draw_text_outlined(draw_list, font, font_size, ImVec2(x, pos.y), color, glyph);
                    x += text_width(font, font_size, glyph) * advance_scale;
                }
                return;
            }
        }

        draw_text_outlined(draw_list, font, font_size, pos, color, text);
    }

    void draw_outlined_text_spans(ImDrawList* draw_list, ImFont* font, float font_size, ImVec2 pos, const text_span* spans, int span_count) {
        if (draw_list == nullptr || spans == nullptr || span_count <= 0)
            return;

        if (font_size <= 0.0f)
            font_size = fonts::ui_size(font);
        if (!font)
            font = fonts::ui();

        float x = pos.x;
        for (int i = 0; i < span_count; ++i) {
            draw_outlined_text(draw_list, font, font_size, ImVec2(x, pos.y), spans[i].color, spans[i].text);
            x += text_width(font, font_size, spans[i].text);
        }
    }

    static void draw_shimmer_glyphs(
        ImDrawList* draw_list,
        ImFont* font,
        float font_size,
        ImVec2 pos,
        float origin_x,
        ImU32 base_color,
        ImU32 highlight_color,
        const char* text,
        float phase,
        float wavelength_px)
    {
        if (draw_list == nullptr || text == nullptr || text[0] == '\0')
            return;

        if (wavelength_px < 1.0f)
            wavelength_px = 1.0f;
        const float two_pi = 6.28318530718f;
        const float k = two_pi / wavelength_px;

        char glyph[2] = {};
        float x = pos.x;
        for (const char* p = text; *p != '\0'; ++p) {
            glyph[0] = *p;
            const float glyph_w = text_width(font, font_size, glyph);
            const float sample_x = x + glyph_w * 0.5f - origin_x;
            const float wave = 0.5f + 0.5f * std::sin(sample_x * k - phase);
            const ImU32 color = lerp_u32(base_color, highlight_color, wave);
            draw_text_outlined(draw_list, font, font_size, ImVec2(x, pos.y), color, glyph);
            x += glyph_w;
        }
    }

    void draw_outlined_text_shimmer(
        ImDrawList* draw_list,
        ImFont* font,
        float font_size,
        ImVec2 pos,
        ImU32 base_color,
        ImU32 highlight_color,
        const char* text,
        float phase,
        float wavelength_px)
    {
        if (font_size <= 0.0f)
            font_size = fonts::ui_size(font);
        if (!font)
            font = fonts::ui();
        draw_shimmer_glyphs(
            draw_list, font, font_size, pos, pos.x,
            base_color, highlight_color, text, phase, wavelength_px);
    }

    void draw_outlined_text_spans_shimmer(
        ImDrawList* draw_list,
        ImFont* font,
        float font_size,
        ImVec2 pos,
        const text_span* spans,
        int span_count,
        ImU32 highlight_color,
        float phase,
        float wavelength_px)
    {
        if (draw_list == nullptr || spans == nullptr || span_count <= 0)
            return;

        if (font_size <= 0.0f)
            font_size = fonts::ui_size(font);
        if (!font)
            font = fonts::ui();

        const float origin_x = pos.x;
        float x = pos.x;
        for (int i = 0; i < span_count; ++i) {
            draw_shimmer_glyphs(
                draw_list, font, font_size, ImVec2(x, pos.y), origin_x,
                spans[i].color, highlight_color, spans[i].text, phase, wavelength_px);
            x += text_width(font, font_size, spans[i].text);
        }
    }
}
