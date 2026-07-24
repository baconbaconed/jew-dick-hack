#define IMGUI_DEFINE_MATH_OPERATORS
#include "widgets.h"
#include "../colors/colors.h"
#include "../resources/fonts/fonts.h"
#include "imgui/imgui_internal.h"
#include <Windows.h>
#include <cstdio>

namespace {
    constexpr float k_key_box_w = 58.0f;
    constexpr float k_mode_box_w = 44.0f;
    constexpr float k_key_box_h = 16.0f;
    constexpr float k_box_gap = 4.0f;
    constexpr float k_label_gap = 6.0f;

    constexpr float k_right_margin = 12.0f;

    constexpr ImU32 k_outline = IM_COL32(0, 0, 0, 255);
    constexpr ImU32 k_inline_border = IM_COL32(24, 24, 25, 255);
    constexpr ImU32 k_fill = IM_COL32(18, 19, 19, 255);
    constexpr ImU32 k_text_inactive = IM_COL32(100, 100, 100, 255);
    constexpr ImU32 k_text_hover = IM_COL32(255, 255, 255, 255);

    const char* mode_name(int mode) {
        return mode == 1 ? "toggle" : "hold";
    }

    void key_name(int vk, char* out, int out_size) {
        if (vk == 0) {
            ImFormatString(out, out_size, "none");
            return;
        }

        switch (vk) {
            case VK_LBUTTON: ImFormatString(out, out_size, "mouse1"); return;
            case VK_RBUTTON: ImFormatString(out, out_size, "mouse2"); return;
            case VK_MBUTTON: ImFormatString(out, out_size, "mouse3"); return;
            case VK_XBUTTON1: ImFormatString(out, out_size, "mouse4"); return;
            case VK_XBUTTON2: ImFormatString(out, out_size, "mouse5"); return;
            default: break;
        }

        UINT scan_code = MapVirtualKeyA(static_cast<UINT>(vk), MAPVK_VK_TO_VSC);
        LONG lparam = static_cast<LONG>(scan_code) << 16;

        switch (vk) {
            case VK_LEFT: case VK_UP: case VK_RIGHT: case VK_DOWN:
            case VK_PRIOR: case VK_NEXT: case VK_END: case VK_HOME:
            case VK_INSERT: case VK_DELETE: case VK_DIVIDE: case VK_NUMLOCK:
                lparam |= (1 << 24);
                break;
            default:
                break;
        }

        char buf[64] = {};
        if (GetKeyNameTextA(lparam, buf, sizeof(buf)) > 0) {
            ImFormatString(out, out_size, "%s", buf);
        } else {
            ImFormatString(out, out_size, "key%d", vk);
        }
    }

    static bool s_capture_snapshot[256] = {};

    void take_key_snapshot() {
        for (int vk = 1; vk < 256; ++vk) {
            s_capture_snapshot[vk] = (GetAsyncKeyState(vk) & 0x8000) != 0;
        }
    }

    int find_new_key_since_snapshot() {
        for (int vk = 1; vk < 256; ++vk) {
            if (vk == VK_LBUTTON) continue;
            if (!s_capture_snapshot[vk] && (GetAsyncKeyState(vk) & 0x8000)) {
                return vk;
            }
        }
        return 0;
    }

    bool draw_key_box(const char* id_seed, int* key) {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window == nullptr || window->SkipItems || key == nullptr) {
            return false;
        }

        const ImGuiID imgui_id = window->GetID(id_seed);
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const ImVec2 size(k_key_box_w, k_key_box_h);
        const ImRect bb(pos, pos + size);
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        ImGui::ItemSize(size);
        if (!ImGui::ItemAdd(bb, imgui_id)) {
            return false;
        }

        bool hovered = false;
        bool held = false;
        const bool pressed = ImGui::ButtonBehavior(bb, imgui_id, &hovered, &held);

        bool* capturing = window->StateStorage.GetBoolRef(imgui_id, false);

        if (pressed) {
            const bool now_capturing = !*capturing;
            *capturing = now_capturing;
            if (now_capturing) {

                take_key_snapshot();
            }
        }

        bool changed = false;
        if (*capturing) {
            if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
                *key = 0;
                *capturing = false;
                changed = true;
            } else {
                const int captured = find_new_key_since_snapshot();
                if (captured != 0) {
                    *key = captured;
                    *capturing = false;
                    changed = true;
                }
            }
        }

        draw_list->AddRectFilled(ImVec2(bb.Min.x + 1.0f, bb.Min.y + 1.0f), ImVec2(bb.Max.x - 1.0f, bb.Max.y - 1.0f), k_fill);
        draw_list->AddRect(ImVec2(bb.Min.x + 1.0f, bb.Min.y + 1.0f), ImVec2(bb.Max.x - 1.0f, bb.Max.y - 1.0f), k_inline_border, 0.0f, 0, 1.0f);
        draw_list->AddRect(bb.Min, bb.Max, k_outline, 0.0f, 0, 1.0f);

        ImFont* font = fonts::tahoma != nullptr ? fonts::tahoma : ImGui::GetFont();
        const float font_size = fonts::tahoma && fonts::tahoma->LegacySize > 0.0f ? fonts::tahoma->LegacySize : 13.0f;

        char text_buf[32];
        if (*capturing) {
            ImFormatString(text_buf, IM_ARRAYSIZE(text_buf), "...");
        } else {
            key_name(*key, text_buf, IM_ARRAYSIZE(text_buf));
        }

        const ImU32 text_color = *capturing ? colors::accent_u32() : (hovered ? k_text_hover : k_text_inactive);
        const ImVec2 text_size = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, text_buf);
        widgets::draw_outlined_text(
            draw_list,
            font,
            font_size,
            ImVec2(ImFloor(bb.Min.x + (size.x - text_size.x) * 0.5f), ImFloor(bb.Min.y + (size.y - text_size.y) * 0.5f)),
            text_color,
            text_buf);

        return changed;
    }

    bool draw_mode_box(const char* id_seed, int* mode) {
        if (mode == nullptr) {
            return false;
        }

        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window == nullptr || window->SkipItems) {
            return false;
        }

        const ImGuiID imgui_id = window->GetID(id_seed);
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const ImVec2 size(k_mode_box_w, k_key_box_h);
        const ImRect bb(pos, pos + size);
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        ImGui::ItemSize(size);
        if (!ImGui::ItemAdd(bb, imgui_id)) {
            return false;
        }

        bool hovered = false;
        bool held = false;
        const bool pressed = ImGui::ButtonBehavior(bb, imgui_id, &hovered, &held);
        if (pressed) {
            *mode = (*mode + 1) % 2;
        }

        draw_list->AddRectFilled(ImVec2(bb.Min.x + 1.0f, bb.Min.y + 1.0f), ImVec2(bb.Max.x - 1.0f, bb.Max.y - 1.0f), k_fill);
        draw_list->AddRect(ImVec2(bb.Min.x + 1.0f, bb.Min.y + 1.0f), ImVec2(bb.Max.x - 1.0f, bb.Max.y - 1.0f), k_inline_border, 0.0f, 0, 1.0f);
        draw_list->AddRect(bb.Min, bb.Max, k_outline, 0.0f, 0, 1.0f);

        ImFont* font = fonts::tahoma != nullptr ? fonts::tahoma : ImGui::GetFont();
        const float font_size = fonts::tahoma && fonts::tahoma->LegacySize > 0.0f ? fonts::tahoma->LegacySize : 13.0f;
        const char* name = mode_name(*mode);
        const ImVec2 text_size = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, name);
        widgets::draw_outlined_text(
            draw_list,
            font,
            font_size,
            ImVec2(ImFloor(bb.Min.x + (size.x - text_size.x) * 0.5f), ImFloor(bb.Min.y + (size.y - text_size.y) * 0.5f)),
            hovered ? k_text_hover : k_text_inactive,
            name);

        return pressed;
    }
}

namespace widgets {
    bool keybind(const char* label, int* key, int* activation_mode) {
        if (key == nullptr) {
            return false;
        }

        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window == nullptr || window->SkipItems) {
            return false;
        }

        const char* text = label != nullptr ? label : "";
        ImFont* label_font = fonts::tahoma != nullptr ? fonts::tahoma : ImGui::GetFont();
        const float label_font_size = fonts::tahoma && fonts::tahoma->LegacySize > 0.0f ? fonts::tahoma->LegacySize : 13.0f;

        if (text[0] != '\0') {
            const ImVec2 pos = ImGui::GetCursorScreenPos();
            const ImVec2 label_size = label_font->CalcTextSizeA(label_font_size, FLT_MAX, 0.0f, text);
            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            draw_outlined_text(
                draw_list,
                label_font,
                label_font_size,
                ImVec2(ImFloor(pos.x), ImFloor(pos.y + (k_key_box_h - label_size.y) * 0.5f)),
                k_text_hover,
                text);
            ImGui::Dummy(ImVec2(label_size.x, k_key_box_h));
            ImGui::SameLine();
        }

        {
            ImGuiWindow* win = ImGui::GetCurrentWindow();
            const float key_area_w = k_key_box_w + (activation_mode != nullptr ? (k_box_gap + k_mode_box_w) : 0.0f);
            const float right_edge  = win->WorkRect.Max.x - k_right_margin;
            const float box_left    = right_edge - key_area_w;
            const ImVec2 cur        = ImGui::GetCursorScreenPos();
            ImGui::SetCursorScreenPos(ImVec2(ImMax(cur.x, box_left), cur.y));
        }

        char id_buf[128];
        ImFormatString(id_buf, IM_ARRAYSIZE(id_buf), "%s##keybind", text);
        bool changed = draw_key_box(id_buf, key);

        if (activation_mode != nullptr) {
            ImGui::SameLine(0.0f, k_box_gap);
            char mode_id[128];
            ImFormatString(mode_id, IM_ARRAYSIZE(mode_id), "%s##mode", text);
            changed |= draw_mode_box(mode_id, activation_mode);
        }

        return changed;
    }

    bool checkbox_keybind(const char* label, bool* value, int* key, int* activation_mode) {
        const bool label_changed = checkbox(label, value);

        {
            ImGuiWindow* win = ImGui::GetCurrentWindow();
            const float key_area_w = k_key_box_w + (activation_mode != nullptr ? (k_box_gap + k_mode_box_w) : 0.0f);
            const float right_edge  = win->WorkRect.Max.x - k_right_margin;
            const float box_left    = right_edge - key_area_w;
            ImGui::SameLine();
            const ImVec2 cur = ImGui::GetCursorScreenPos();
            ImGui::SetCursorScreenPos(ImVec2(ImMax(cur.x, box_left), cur.y));
        }

        char id_buf[128];
        ImFormatString(id_buf, IM_ARRAYSIZE(id_buf), "%s##keybind", label != nullptr ? label : "");
        bool key_changed = draw_key_box(id_buf, key);

        if (activation_mode != nullptr) {
            ImGui::SameLine(0.0f, k_box_gap);
            char mode_id[128];
            ImFormatString(mode_id, IM_ARRAYSIZE(mode_id), "%s##mode", label != nullptr ? label : "");
            key_changed |= draw_mode_box(mode_id, activation_mode);
        }

        if (value != nullptr && key != nullptr && *key != 0) {
            const int mode = activation_mode != nullptr ? *activation_mode : 0;
            if (mode == 1) {
                if (GetAsyncKeyState(*key) & 1) {
                    *value = !*value;
                }
            } else {
                *value = (GetAsyncKeyState(*key) & 0x8000) != 0;
            }
        }

        return label_changed || key_changed;
    }
}
