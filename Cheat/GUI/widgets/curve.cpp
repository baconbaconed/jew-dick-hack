#define IMGUI_DEFINE_MATH_OPERATORS
#include "widgets.h"
#include "../colors/colors.h"
#include "imgui/imgui_internal.h"
#include <algorithm>

namespace {
    constexpr ImU32 k_outline    = IM_COL32(0, 0, 0, 255);
    constexpr ImU32 k_inline     = IM_COL32(24, 24, 25, 255);
    constexpr ImU32 k_fill       = IM_COL32(18, 19, 19, 255);
    constexpr ImU32 k_grid       = IM_COL32(38, 39, 40, 255);
    constexpr ImU32 k_accent     = IM_COL32(51, 122, 231, 255);
    constexpr ImU32 k_accent_dim = IM_COL32(51, 122, 231, 90);
    constexpr ImU32 k_handle     = IM_COL32(210, 225, 255, 255);

    float value_to_y(float v, float top, float h) {
        return top + (1.0f - ImClamp(v, 0.0f, 1.0f)) * h;
    }
}

namespace widgets {
    bool curve_editor(const char* id, float* points, int count, const ImVec2& size) {
        if (points == nullptr || count < 2) {
            return false;
        }

        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window == nullptr || window->SkipItems) {
            return false;
        }

        ImGuiContext& g = *GImGui;
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const ImVec2 box_size(
            size.x > 0.0f ? size.x : ImGui::GetContentRegionAvail().x,
            size.y > 0.0f ? size.y : 80.0f);

        const ImGuiID item_id = window->GetID(id);
        const ImRect bb(pos, pos + box_size);
        ImGui::ItemSize(box_size);
        if (!ImGui::ItemAdd(bb, item_id)) {
            return false;
        }

        bool hovered = false;
        bool held = false;
        ImGui::ButtonBehavior(bb, item_id, &hovered, &held);

        ImDrawList* dl = ImGui::GetWindowDrawList();

        const ImRect inner(pos + ImVec2(1, 1), bb.Max - ImVec2(1, 1));
        const ImRect fill(pos + ImVec2(2, 2), bb.Max - ImVec2(2, 2));
        dl->AddRectFilled(fill.Min, fill.Max, k_fill);
        dl->AddRect(inner.Min, inner.Max, k_inline, 0.0f, 0, 1.0f);
        dl->AddRect(pos, bb.Max, k_outline, 0.0f, 0, 1.0f);

        const float left = fill.Min.x;
        const float top  = fill.Min.y;
        const float w    = fill.GetWidth();
        const float h    = fill.GetHeight();

        for (int i = 1; i < 4; ++i) {
            const float gx = left + w * (i / 4.0f);
            const float gy = top  + h * (i / 4.0f);
            dl->AddLine(ImVec2(gx, top), ImVec2(gx, top + h), k_grid, 1.0f);
            dl->AddLine(ImVec2(left, gy), ImVec2(left + w, gy), k_grid, 1.0f);
        }

        const float step = w / static_cast<float>(count - 1);

        bool changed = false;
        if (held) {
            const ImVec2 m = g.IO.MousePos;
            int nearest = 0;
            float best = FLT_MAX;
            for (int i = 0; i < count; ++i) {
                const float px = left + step * i;
                const float d = std::abs(m.x - px);
                if (d < best) { best = d; nearest = i; }
            }
            const float v = ImClamp((top + h - m.y) / h, 0.0f, 1.0f);
            if (points[nearest] != v) { points[nearest] = v; changed = true; }
        }

        for (int i = 0; i < count - 1; ++i) {
            const ImVec2 a(left + step * i,       value_to_y(points[i],   top, h));
            const ImVec2 b(left + step * (i + 1), value_to_y(points[i+1], top, h));
            dl->AddQuadFilled(
                ImVec2(a.x, top + h), a, b, ImVec2(b.x, top + h), k_accent_dim);
            dl->AddLine(a, b, k_accent, 1.6f);
        }

        for (int i = 0; i < count; ++i) {
            const ImVec2 c(left + step * i, value_to_y(points[i], top, h));
            dl->AddCircleFilled(c, 2.6f, k_handle);
            dl->AddCircle(c, 2.6f, k_outline, 0, 1.0f);
        }

        return changed;
    }
}
