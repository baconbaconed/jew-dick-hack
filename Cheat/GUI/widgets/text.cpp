#include "widgets.h"
#include "imgui/imgui_internal.h"
#include <cmath>

namespace {
    constexpr ImVec2 k_outline_cardinal[] = {
        ImVec2(-1.0f, 0.0f),
        ImVec2(1.0f, 0.0f),
        ImVec2(0.0f, -1.0f),
        ImVec2(0.0f, 1.0f),
    };

    constexpr ImVec2 k_outline_diagonal[] = {
        ImVec2(-1.0f, -1.0f),
        ImVec2(1.0f, -1.0f),
        ImVec2(-1.0f, 1.0f),
        ImVec2(1.0f, 1.0f),
    };

    ImU32 lerp_u32(ImU32 a, ImU32 b, float t) {
        const ImVec4 va = ImGui::ColorConvertU32ToFloat4(a);
        const ImVec4 vb = ImGui::ColorConvertU32ToFloat4(b);
        return ImGui::ColorConvertFloat4ToU32(ImLerp(va, vb, ImSaturate(t)));
    }
}

namespace widgets {
    static float text_width(ImFont* font, float font_size, const char* text) {
        if (text == nullptr) {
            return 0.0f;
        }
        if (font != nullptr) {
            return font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, text).x;
        }
        return ImGui::CalcTextSize(text).x;
    }

    static void draw_glyph_outline(ImDrawList* draw_list, ImFont* font, float font_size, ImVec2 pos, const char* glyph) {
        const ImU32 outline_color = IM_COL32(0, 0, 0, 255);

        if (font != nullptr) {
            for (const ImVec2& offset : k_outline_cardinal) {
                draw_list->AddText(
                    font,
                    font_size,
                    ImVec2(ImFloor(pos.x + offset.x), ImFloor(pos.y + offset.y)),
                    outline_color,
                    glyph);
            }
            for (const ImVec2& offset : k_outline_diagonal) {
                draw_list->AddText(
                    font,
                    font_size,
                    ImVec2(ImFloor(pos.x + offset.x), ImFloor(pos.y + offset.y)),
                    outline_color,
                    glyph);
            }
            return;
        }

        for (const ImVec2& offset : k_outline_cardinal) {
            draw_list->AddText(
                ImVec2(ImFloor(pos.x + offset.x), ImFloor(pos.y + offset.y)),
                outline_color,
                glyph);
        }
        for (const ImVec2& offset : k_outline_diagonal) {
            draw_list->AddText(
                ImVec2(ImFloor(pos.x + offset.x), ImFloor(pos.y + offset.y)),
                outline_color,
                glyph);
        }
    }

    void draw_outlined_text(ImDrawList* draw_list, ImFont* font, float font_size, ImVec2 pos, ImU32 color, const char* text, float extra_width) {
        if (draw_list == nullptr || text == nullptr) {
            return;
        }

        pos.x = ImFloor(pos.x);
        pos.y = ImFloor(pos.y);

        const float natural_width = text_width(font, font_size, text);
        const float advance_scale = (extra_width > 0.0f && natural_width > 0.0f)
            ? (natural_width + extra_width) / natural_width
            : 1.0f;

        char glyph[2] = {};
        float x = pos.x;
        for (const char* p = text; *p != '\0'; ++p) {
            glyph[0] = *p;
            const ImVec2 glyph_pos(x, pos.y);

            draw_glyph_outline(draw_list, font, font_size, glyph_pos, glyph);

            if (font != nullptr) {
                draw_list->AddText(font, font_size, glyph_pos, color, glyph);
            } else {
                draw_list->AddText(glyph_pos, color, glyph);
            }

            x += text_width(font, font_size, glyph) * advance_scale;
        }
    }

    void draw_outlined_text_spans(ImDrawList* draw_list, ImFont* font, float font_size, ImVec2 pos, const text_span* spans, int span_count) {
        if (draw_list == nullptr || spans == nullptr || span_count <= 0) {
            return;
        }

        pos.x = ImFloor(pos.x);
        pos.y = ImFloor(pos.y);

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
        if (draw_list == nullptr || text == nullptr || text[0] == '\0') {
            return;
        }

        if (wavelength_px < 1.0f) {
            wavelength_px = 1.0f;
        }
        const float two_pi = 6.28318530718f;
        const float k = two_pi / wavelength_px;

        char glyph[2] = {};
        float x = ImFloor(pos.x);
        const float y = ImFloor(pos.y);

        for (const char* p = text; *p != '\0'; ++p) {
            glyph[0] = *p;
            const ImVec2 glyph_pos(x, y);

            draw_glyph_outline(draw_list, font, font_size, glyph_pos, glyph);

            const float glyph_w = text_width(font, font_size, glyph);
            const float sample_x = x + glyph_w * 0.5f - origin_x;
            const float wave = 0.5f + 0.5f * std::sin(sample_x * k - phase);
            const ImU32 color = lerp_u32(base_color, highlight_color, wave);

            if (font != nullptr) {
                draw_list->AddText(font, font_size, glyph_pos, color, glyph);
            } else {
                draw_list->AddText(glyph_pos, color, glyph);
            }

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
        draw_shimmer_glyphs(
            draw_list, font, font_size, pos, ImFloor(pos.x),
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
        if (draw_list == nullptr || spans == nullptr || span_count <= 0) {
            return;
        }

        const float origin_x = ImFloor(pos.x);
        pos.x = origin_x;
        pos.y = ImFloor(pos.y);

        float x = origin_x;
        for (int i = 0; i < span_count; ++i) {
            draw_shimmer_glyphs(
                draw_list, font, font_size, ImVec2(x, pos.y), origin_x,
                spans[i].color, highlight_color, spans[i].text, phase, wavelength_px);
            x += text_width(font, font_size, spans[i].text);
        }
    }
}
