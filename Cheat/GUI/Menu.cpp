#include "Menu.h"
#include "animation/animation.h"
#include "colors/colors.h"
#include "resources/fonts/fonts.h"
#include "widgets/widgets.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "imgui/backends/imgui_impl_dx11.h"
#include "imgui/backends/imgui_impl_win32.h"
#include "imgui/misc/imgui_freetype.h"
#include "../Settings.h"
#include "../Renderer/Renderer.h"
#include "../Core/Features/Visuals/ESP.h"
#include "../Core/Features/Visuals/ESPPreview.h"
#include "../Core/Features/Visuals/ShaderChams.h"
#include "../Core/Features/Visuals/KillEffects.h"
#include "../Core/Features/Aim/Aim.h"
#include "../Core/Features/Aim/RaycastSilent.h"
#include "../Core/Features/Misc/Misc.h"
#include "../Core/Features/Misc/HitSounds.h"
#include "../Core/Features/Explorer/Explorer.h"
#include "../Core/PlayerHandler/PlayerHandler.h"
#include <Windows.h>
#include <cstdio>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace {
    void row_checkbox_color(const char* label, bool* value, float color[4], const char* color_id) {
        widgets::checkbox(label, value);
        const float row_y = widgets::color_picker_row_y();
        widgets::same_line_color_picker(row_y, 0, 1);
        widgets::color_edit4(color_id, color);
    }

    void sync_gui_theme() {
        auto& g = Cheat::g_Settings.gui;
        colors::SyncFromSettings(
            g.accent, g.text_active, g.text_inactive,
            g.outer_border, g.inner_border, g.panel_fill,
            g.content_outer, g.content_inner, g.content_fill, g.child_fill);
    }

    void theme_color_row(const char* label, float color[4], const char* id) {
        ImGui::SetCursorPosX(6.0f);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
        ImGui::TextUnformatted(label);
        const float row_y = widgets::color_picker_row_y();
        widgets::same_line_color_picker(row_y, 0, 1);
        if (widgets::color_edit4(id, color))
            Cheat::g_Settings.gui.theme = colors::Theme_Custom;
    }
}

namespace Cheat {
namespace GUI {

bool Menu::Initialize(HWND hWnd, ID3D11Device* pDevice, ID3D11DeviceContext* pDeviceContext)
{
    bool result = true;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    io.IniFilename = nullptr;
    io.ConfigWindowsResizeFromEdges = true;

    colors::apply_style();
    fonts::load(io);

    result = ImGui_ImplWin32_Init(hWnd);
    if (!result) return false;

    result = ImGui_ImplDX11_Init(pDevice, pDeviceContext);
    if (!result) return false;

    m_bInitialized = true;

    Cheat::Features::Explorer::Initialize();

    Cheat::Features::Misc::Start();

    Cheat::Features::RaycastSilent::Ensure(true);

    return true;
}

void Menu::Render()
{
    if (!m_bInitialized) return;

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    {
        sync_gui_theme();

        if (Renderer::IsGameActive()) {
            Visuals::ESP::Render();
            Features::Aim::Render();
            Visuals::KillEffects::Tick();
        }
        DrawMenu();
        Features::Explorer::Render();
    }
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

bool Menu::IsVisible()
{
    return m_bMenuVisible;
}

bool Menu::IsPointOverUI(float x, float y)
{
    if (!m_bInitialized) return false;
    ImGuiContext* ctx = ImGui::GetCurrentContext();
    if (!ctx || ctx->Windows.Size <= 0) return false;

    const ImVec2 p(x, y);
    for (int i = ctx->Windows.Size - 1; i >= 0; --i) {
        ImGuiWindow* w = ctx->Windows[i];
        if (!w || !w->Active || w->Hidden) continue;
        if (w->IsFallbackWindow) continue;

        if (w->Flags & ImGuiWindowFlags_Tooltip) continue;
        if (!(w->Flags & ImGuiWindowFlags_NoInputs)) {
            if (w->Rect().Contains(p))
                return true;
        }
    }
    return false;
}

bool Menu::ShouldCaptureMouse(float x, float y)
{
    if (!m_bInitialized || !m_bMenuVisible) return false;

    if (ImGui::GetIO().WantCaptureMouse) return true;
    return IsPointOverUI(x, y);
}

void Menu::DrawMenu()
{
    ImGuiIO& io = ImGui::GetIO();

    constexpr float k_animation_duration = 0.30f;
    static bool watermark_enabled = false;
    static bool insert_previous = false;
    static float animation_time = k_animation_duration;
    static bool animation_open_direction = true;
    static int menu_key = VK_DELETE;
    static int active_tab = 0;
    static ImVec2 s_menu_pos = {};
    static ImVec2 s_menu_size = {};

    const bool insert_pressed = (GetAsyncKeyState(menu_key) & 0x8000) != 0;
    if (insert_pressed && !insert_previous) {
        m_bMenuVisible = !m_bMenuVisible;
        animation_time = 0.0f;
        animation_open_direction = m_bMenuVisible;
        if (m_bMenuVisible) {
            ClipCursor(nullptr);
            while (ShowCursor(TRUE) < 0) {}
            if (HWND overlay = Renderer::GetHwnd()) {
                const LONG base =
                    WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE;
                SetWindowLong(overlay, GWL_EXSTYLE, base);
            }
        }
    }
    insert_previous = insert_pressed;

    if (animation_time < k_animation_duration) {
        animation_time += io.DeltaTime;
        if (animation_time > k_animation_duration) {
            animation_time = k_animation_duration;
        }
    }

    const bool animation_running = animation_time < k_animation_duration;
    float progress = animation_time / k_animation_duration;
    progress = ImClamp(progress, 0.0f, 1.0f);

    float window_alpha = 0.0f;
    if (animation_open_direction) {
        window_alpha = animation::ease_cubic_out(progress);
    } else {
        window_alpha = 1.0f - animation::ease_cubic_in(progress);
    }

    if (watermark_enabled) {
        const float watermark_alpha = m_bMenuVisible ? window_alpha : 1.0f;
        widgets::watermark("user", watermark_alpha);
    }

    if (!m_bMenuVisible && !animation_running && window_alpha <= 0.0f) {
        return;
    }

    constexpr ImVec2 k_menu_size(560.0f, 460.0f);

    ImGui::SetNextWindowPos(io.DisplaySize * 0.5f, ImGuiCond_Once, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(k_menu_size, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(520.0f, 340.0f), ImVec2(820.0f, 800.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, window_alpha);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    if (ImGui::Begin("menu", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize))
    {
        s_menu_pos  = ImGui::GetWindowPos();
        s_menu_size = ImGui::GetWindowSize();

        {
            constexpr float kGrip = 5.0f;
            constexpr float kMinW = 520.0f, kMaxW = 820.0f;
            constexpr float kMinH = 340.0f, kMaxH = 800.0f;
            const ImVec2 sz = s_menu_size;

            ImGui::SetCursorPos(ImVec2(sz.x - kGrip, 0.0f));
            ImGui::InvisibleButton("##rzr", ImVec2(kGrip, sz.y - kGrip));
            if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
            if (ImGui::IsItemActive()) {
                const float nw = ImClamp(sz.x + io.MouseDelta.x, kMinW, kMaxW);
                ImGui::SetWindowSize(ImVec2(nw, sz.y));
            }

            ImGui::SetCursorPos(ImVec2(0.0f, sz.y - kGrip));
            ImGui::InvisibleButton("##rzb", ImVec2(sz.x - kGrip, kGrip));
            if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
            if (ImGui::IsItemActive()) {
                const float nh = ImClamp(sz.y + io.MouseDelta.y, kMinH, kMaxH);
                ImGui::SetWindowSize(ImVec2(sz.x, nh));
            }

            ImGui::SetCursorPos(ImVec2(sz.x - kGrip, sz.y - kGrip));
            ImGui::InvisibleButton("##rzc", ImVec2(kGrip, kGrip));
            if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNWSE);
            if (ImGui::IsItemActive()) {
                const float nw = ImClamp(sz.x + io.MouseDelta.x, kMinW, kMaxW);
                const float nh = ImClamp(sz.y + io.MouseDelta.y, kMinH, kMaxH);
                ImGui::SetWindowSize(ImVec2(nw, nh));
            }
        }
        const ImVec2 window_pos  = s_menu_pos;
        const ImVec2 window_size = s_menu_size;
        colors::draw_panel_background(window_alpha);

        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        constexpr float k_border = 2.0f;
        constexpr ImVec2 k_title_pad(10.0f, 10.0f);
        const ImVec2 text_pos(
            window_pos.x + k_border + k_title_pad.x,
            window_pos.y + k_border + k_title_pad.y);

        const ImU32 text_color = IM_COL32(255, 255, 255, 255);
        const ImU32 accent_color = colors::accent_u32();

        const ImU32 highlight_color = IM_COL32(210, 225, 255, 255);
        const float font_size = fonts::tahoma_bold && fonts::tahoma_bold->LegacySize > 0.0f
            ? fonts::tahoma_bold->LegacySize
            : 12.0f;

        const widgets::text_span title_spans[] = {
            {"jew", accent_color},
            {"sploit", text_color},
        };

        const float title_phase = static_cast<float>(ImGui::GetTime()) * 2.4f;
        const float title_wavelength = 90.0f;

        const float title_intro_offset = (1.0f - window_alpha) * 6.0f;
        ImVec2 title_pos = text_pos;
        title_pos.y -= title_intro_offset;

        widgets::draw_outlined_text_spans_shimmer(
            draw_list,
            fonts::tahoma_bold,
            font_size,
            title_pos,
            title_spans,
            IM_ARRAYSIZE(title_spans),
            highlight_color,
            title_phase,
            title_wavelength);

        constexpr float k_content_margin_x = 10.0f;
        constexpr float k_content_top = 29.0f;
        constexpr float k_content_margin_bottom = 10.0f;
        constexpr float k_tab_width = 81.0f;
        constexpr float k_tab_height = 18.0f;
        constexpr float k_tab_spacing = 2.0f;
        constexpr float k_panel_inset = 2.0f;
        constexpr ImVec2 k_content_padding(6.0f, 6.0f);
        constexpr float k_min_content_w = 280.0f;
        constexpr float k_min_content_h = 240.0f;

        const float content_w = ImMax(k_min_content_w, window_size.x - k_content_margin_x * 2.0f);
        const float content_h = ImMax(
            k_min_content_h,
            window_size.y - k_content_top - k_content_margin_bottom);

        const ImVec2 panel_min(
            window_pos.x + k_content_margin_x,
            window_pos.y + k_content_top);
        const ImVec2 panel_max(panel_min.x + content_w, panel_min.y + content_h);
        const float tabs_total_width = k_tab_width * 5.0f + k_tab_spacing * 4.0f;
        const float tab_start_x = panel_max.x - tabs_total_width;
        const float tab_top = panel_min.y - k_tab_height;
        const char* tab_labels[] = { "esp", "players", "settings", "aim", "misc" };

        const ImU32 panel_fill = ImGui::GetColorU32(colors::content_fill);
        const ImU32 panel_outer = ImGui::GetColorU32(colors::content_outer_border);
        const ImU32 panel_inner = ImGui::GetColorU32(colors::content_inner_border);

        const float tab_font_size = fonts::tahoma && fonts::tahoma->LegacySize > 0.0f
            ? fonts::tahoma->LegacySize
            : 13.0f;

        active_tab = widgets::draw_tab_bar(
            draw_list,
            panel_min,
            panel_max,
            tab_start_x,
            tab_top,
            active_tab,
            tab_labels,
            IM_ARRAYSIZE(tab_labels),
            k_tab_width,
            k_tab_height,
            k_tab_spacing,
            fonts::tahoma,
            fonts::tahoma,
            tab_font_size,
            panel_fill,
            panel_outer,
            panel_inner);

        const ImVec2 child_size(
            content_w - k_panel_inset * 2.0f - k_content_padding.x * 2.0f,
            content_h - k_panel_inset * 2.0f - k_content_padding.y * 2.0f);

        ImGui::SetCursorPos(ImVec2(
            k_content_margin_x + k_panel_inset + k_content_padding.x,
            k_content_top + k_panel_inset + k_content_padding.y));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        if (ImGui::BeginChild(
            "menu_content",
            child_size,
            false,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
        {
            constexpr float k_side_child_w_min = 120.0f;
            constexpr float k_side_child_h_min = 160.0f;
            constexpr float k_child_margin = 2.0f;
            constexpr float k_side_child_gap = 6.0f;

            const ImVec2 content_avail = ImGui::GetContentRegionAvail();
            const float inner_w = content_avail.x - k_child_margin * 2.0f;
            const float side_child_h = ImMax(
                k_side_child_h_min,
                content_avail.y - k_child_margin * 2.0f);

            const float side_title_size = fonts::tahoma && fonts::tahoma->LegacySize > 0.0f
                ? fonts::tahoma->LegacySize
                : 13.0f;

            auto make_side_child_size = [&](int columns) -> ImVec2 {
                const int cols = columns < 1 ? 1 : columns;
                const float gaps = k_side_child_gap * static_cast<float>(cols - 1);
                const float column_w = (inner_w - gaps) / static_cast<float>(cols);
                return ImVec2(ImMax(k_side_child_w_min, column_w), side_child_h);
            };

            auto draw_side_child = [&](
                const char* id,
                const char* title,
                const ImVec2& cursor_pos,
                const ImVec2& size) {
                ImGui::SetCursorPos(cursor_pos);
                return widgets::begin_child_panel(
                    id,
                    size,
                    title,
                    fonts::tahoma,
                    side_title_size,
                    nullptr,
                    nullptr,
                    nullptr);
            };

            const ImVec2 full_child_size = make_side_child_size(1);
            const ImVec2 full_child_pos(k_child_margin, k_child_margin);

            if (active_tab == 0) {

                const ImVec2 side_child_size = make_side_child_size(2);
                const ImVec2 left_child_pos(k_child_margin, k_child_margin);
                const ImVec2 right_child_pos(
                    k_child_margin + side_child_size.x + k_side_child_gap,
                    k_child_margin);

                if (draw_side_child("esp_main", "esp", left_child_pos, side_child_size)) {
                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 1.0f);
                    widgets::checkbox("enabled", &g_Settings.esp.enabled);

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                    widgets::checkbox("draw local", &g_Settings.esp.draw_local);

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                    row_checkbox_color("bounding box", &g_Settings.esp.box, g_Settings.esp.box_color, "esp_box_color");

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                    row_checkbox_color("name", &g_Settings.esp.name, g_Settings.esp.name_color, "esp_name_color");

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                    row_checkbox_color("skeleton", &g_Settings.esp.skeleton, g_Settings.esp.skeleton_color, "esp_skeleton_color");

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                    widgets::checkbox("chams", &g_Settings.esp.chams);

                    if (g_Settings.esp.chams_mode != 3) {
                        const float row_y = widgets::color_picker_row_y();
                        widgets::same_line_color_picker(row_y, 1, 2);
                        widgets::color_edit4("esp_chams_outline_color", g_Settings.esp.chams_outline_color);
                        widgets::same_line_color_picker(row_y, 0, 2);
                        widgets::color_edit4("esp_chams_fill_color", g_Settings.esp.chams_fill_color);
                    }

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    {
                        static const char* k_chams_modes[] = { "box", "box filled", "clipper", "shader" };
                        widgets::combo("chams mode", &g_Settings.esp.chams_mode, k_chams_modes, 4);
                    }

                    if (g_Settings.esp.chams_mode == 3) {
                        ImGui::SetCursorPosX(6.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                        widgets::combo("shader", &g_Settings.esp.chams_shader,
                                       Visuals::ShaderChams::StyleNames(),
                                       Visuals::ShaderChams::StyleNameCount());
                    }

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                    widgets::checkbox("health bar", &g_Settings.esp.healthbar);

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                    widgets::checkbox("health text", &g_Settings.esp.health_text);

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                    row_checkbox_color("distance", &g_Settings.esp.distance, g_Settings.esp.distance_color, "esp_distance_color");

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                    row_checkbox_color("tool", &g_Settings.esp.tool, g_Settings.esp.tool_color, "esp_tool_color");

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                    widgets::checkbox("flags", &g_Settings.esp.flags);
                }
                widgets::end_child_panel();

                if (draw_side_child("esp_settings", "settings", right_child_pos, side_child_size)) {
                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);
                    {
                        static const char* k_fonts[] = { "gui (thin)", "gui bold" };
                        widgets::combo("font", &g_Settings.esp.font, k_fonts, 2);
                    }

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    widgets::slider_float("font size", &g_Settings.esp.font_size, 8.0f, 24.0f, "%.0fpx");

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    {
                        static const char* k_box_modes[] = { "bounding", "corner", "3d" };
                        widgets::combo("box style", &g_Settings.esp.box_mode, k_box_modes, 3);
                    }

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    {
                        static const char* k_name_modes[] = { "display name", "username" };
                        widgets::combo("name type", &g_Settings.esp.name_mode, k_name_modes, 2);
                    }

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    {
                        static const char* k_dist_units[] = { "studs", "meters" };
                        widgets::combo("distance unit", &g_Settings.esp.distance_unit, k_dist_units, 2);
                    }

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    widgets::checkbox("distance check", &g_Settings.esp.distance_check);

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    widgets::slider_float("max distance", &g_Settings.esp.max_distance, 50.0f, 5000.0f, "%.0f");

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    widgets::checkbox("esp preview", &g_Settings.esp.preview);
                }
                widgets::end_child_panel();
            } else if (active_tab == 1) {

                if (draw_side_child("players_main", "players", full_child_pos, full_child_size)) {
                    if (PlayerHandler::GetPlayerCount() == 0) {
                        ImGui::SetCursorPosX(6.0f);
                        ImGui::TextUnformatted("no players cached");
                    } else {
                        PlayerHandler::ForEachPlayer([](const PlayerCache& player) {
                            ImGui::SetCursorPosX(6.0f);
                            ImGui::TextUnformatted(player.name.empty() ? "unknown" : player.name.c_str());
                        });
                    }
                }
                widgets::end_child_panel();
            } else if (active_tab == 2) {

                if (draw_side_child("settings_main", "menu", full_child_pos, full_child_size)) {
                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.0f);
                    widgets::keybind("menu key", &menu_key);

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                    widgets::checkbox("watermark", &watermark_enabled);

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    widgets::checkbox("explorer", &g_Settings.misc.explorer);

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    widgets::checkbox("custom support", &g_Settings.misc.custom_support);

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    {
                        auto& gui = g_Settings.gui;
                        static int s_last_theme = -1;
                        widgets::combo("theme", &gui.theme,
                                       colors::ThemeNames(), colors::ThemeNameCount());

                        if (gui.theme != s_last_theme) {
                            s_last_theme = gui.theme;
                            if (gui.theme != colors::Theme_Custom) {
                                colors::ApplyPreset(
                                    gui.theme,
                                    gui.accent, gui.text_active, gui.text_inactive,
                                    gui.outer_border, gui.inner_border, gui.panel_fill,
                                    gui.content_outer, gui.content_inner,
                                    gui.content_fill, gui.child_fill);
                            }
                            sync_gui_theme();
                        }
                    }

                    theme_color_row("accent",         g_Settings.gui.accent,         "gui_accent");
                    theme_color_row("text",           g_Settings.gui.text_active,    "gui_text");
                    theme_color_row("text dim",       g_Settings.gui.text_inactive,  "gui_text_dim");
                    theme_color_row("outer border",   g_Settings.gui.outer_border,   "gui_outer");
                    theme_color_row("inner border",   g_Settings.gui.inner_border,   "gui_inner");
                    theme_color_row("panel",          g_Settings.gui.panel_fill,     "gui_panel");
                    theme_color_row("content border", g_Settings.gui.content_outer,  "gui_c_outer");
                    theme_color_row("content inline", g_Settings.gui.content_inner,  "gui_c_inner");
                    theme_color_row("content",        g_Settings.gui.content_fill,   "gui_content");
                    theme_color_row("child",          g_Settings.gui.child_fill,     "gui_child");
                }
                widgets::end_child_panel();
            } else if (active_tab == 3) {
                const ImVec2 aim_child_size = make_side_child_size(2);
                const ImVec2 aim_left_pos(k_child_margin, k_child_margin);
                const ImVec2 aim_right_pos(
                    aim_left_pos.x + aim_child_size.x + k_side_child_gap,
                    k_child_margin);

                Cheat::Settings::AimbotConfig& cfg = g_Settings.aim.active();

                if (draw_side_child("aim_main", "aim", aim_left_pos, aim_child_size)) {
                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.0f);
                    widgets::keybind("aim key", &g_Settings.aim.bind, &g_Settings.aim.bind_mode);

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                    {
                        static const char* k_types[] = { "mouse", "camera", "silent" };
                        widgets::combo("type", &g_Settings.aim.type, k_types, 3);
                    }

                    if (g_Settings.aim.type == 2) {
                        ImGui::SetCursorPosX(6.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                        {
                            static const char* k_silent[] = {
                                "viewport", "mouse", "raycast", "magic bullet"
                            };
                            widgets::combo("silent method", &g_Settings.aim.silent_method,
                                k_silent, Cheat::Settings::SILENT_METHOD_COUNT);
                        }

                        if (g_Settings.aim.silent_method == Cheat::Settings::SILENT_RAYCAST) {
                            ImGui::SetCursorPosX(6.0f);
                            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                            widgets::checkbox_keybind(
                                "force magic bullet",
                                &g_Settings.aim.force_magic_bullet,
                                &g_Settings.aim.force_magic_key,
                                &g_Settings.aim.force_magic_mode);
                        }
                    } else {
                        ImGui::SetCursorPosX(6.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                        widgets::slider_float("smoothness x", &cfg.smooth_x, 0.1f, 5.0f, "%.1f");

                        ImGui::SetCursorPosX(6.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                        widgets::slider_float("smoothness y", &cfg.smooth_y, 0.1f, 5.0f, "%.1f");
                    }

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    row_checkbox_color("fov check", &cfg.fov_enabled, cfg.fov_color, "aim_fov_color");

                    if (cfg.fov_enabled) {
                        ImGui::SetCursorPosX(6.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                        {
                            static const char* k_fov_style[] = { "circle", "filled" };
                            widgets::combo("fov style", &cfg.fov_style, k_fov_style, 2);
                        }

                        ImGui::SetCursorPosX(6.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                        row_checkbox_color("fov outline", &cfg.fov_outline,
                                           cfg.fov_outline_color, "aim_fov_outline_color");

                        ImGui::SetCursorPosX(6.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                        widgets::slider_float("fov size", &cfg.fov_size, 10.0f, 600.0f, "%.0f");

                        ImGui::SetCursorPosX(6.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                        {
                            static const char* k_pos[] = { "center", "mouse" };
                            widgets::combo("fov pos", &cfg.fov_position, k_pos, 2);
                        }
                    }

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    widgets::checkbox("distance check", &cfg.distance_check);

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    widgets::slider_float("max distance", &cfg.max_distance, 50.0f, 5000.0f, "%.0f");

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    widgets::checkbox("visible only", &cfg.visible_only);

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                    widgets::checkbox("humanize", &cfg.humanize);

                    if (cfg.humanize) {
                        ImGui::SetCursorPosX(6.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                        widgets::slider_float("reaction", &cfg.reaction_ms, 0.0f, 400.0f, "%.0fms");
                    }

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                    widgets::checkbox("sticky target", &cfg.sticky);

                    if (cfg.sticky) {
                        ImGui::SetCursorPosX(6.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                        widgets::slider_float("sticky fov", &cfg.sticky_fov_scale, 1.0f, 4.0f, "%.1fx");
                    }
                }
                widgets::end_child_panel();

                if (draw_side_child("aim_extra", "extra", aim_right_pos, aim_child_size)) {
                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);
                    {
                        static const char* k_parts[] = {
                            "head", "upper torso", "lower torso", "hrp",
                            "left hand", "right hand", "left foot", "right foot"
                        };
                        widgets::multi_combo("body parts", cfg.parts, k_parts,
                            Cheat::Settings::AIM_PART_COUNT);
                    }

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    widgets::slider_float("switch time", &cfg.switch_time, 0.05f, 2.0f, "%.2fs");

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    widgets::checkbox("kill effects", &g_Settings.killfx.enabled);

                    if (g_Settings.killfx.enabled) {
                        if (g_Settings.killfx.effect < 0 ||
                            g_Settings.killfx.effect >= Visuals::KillEffects::EffectNameCount())
                            g_Settings.killfx.effect = 0;

                        ImGui::SetCursorPosX(6.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                        widgets::combo("kill fx", &g_Settings.killfx.effect,
                                       Visuals::KillEffects::EffectNames(),
                                       Visuals::KillEffects::EffectNameCount());
                    }

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    widgets::checkbox("hitmarkers", &g_Settings.hitmarker.enabled);

                    if (g_Settings.hitmarker.enabled) {
                        ImGui::SetCursorPosX(6.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                        widgets::slider_float("marker size", &g_Settings.hitmarker.size, 0.5f, 2.5f, "%.2f");

                        ImGui::SetCursorPosX(6.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                        widgets::slider_float("marker time", &g_Settings.hitmarker.duration, 0.2f, 1.5f, "%.2fs");
                    }

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    widgets::checkbox("hitsounds", &g_Settings.hitsound.enabled);

                    if (g_Settings.hitsound.enabled) {
                        ImGui::SetCursorPosX(6.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                        if (g_Settings.hitsound.index < 0 ||
                            g_Settings.hitsound.index >= Features::HitSounds::Count())
                            g_Settings.hitsound.index = 0;
                        if (widgets::combo("hitsound", &g_Settings.hitsound.index,
                                           Features::HitSounds::Names(),
                                           Features::HitSounds::Count())) {
                            Features::HitSounds::Play(g_Settings.hitsound.index);
                        }

                        ImGui::SetCursorPosX(6.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                        widgets::slider_float("hitsound volume", &g_Settings.hitsound.volume,
                                              0.0f, 100.0f, "%.0f%%");
                    }

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    widgets::checkbox("hit data", &g_Settings.hitdata.enabled);

                    if (g_Settings.hitdata.enabled) {
                        ImGui::SetCursorPosX(6.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                        widgets::multi_combo("data lines", g_Settings.hitdata.modes,
                                             Visuals::KillEffects::HitDataModeNames(),
                                             Visuals::KillEffects::HitDataModeNameCount());

                        ImGui::SetCursorPosX(6.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                        widgets::slider_float("data size", &g_Settings.hitdata.size, 10.0f, 28.0f, "%.0f");

                        ImGui::SetCursorPosX(6.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                        widgets::slider_float("data time", &g_Settings.hitdata.duration, 0.4f, 2.5f, "%.2fs");
                    }
                }
                widgets::end_child_panel();
            } else if (active_tab == 4) {

                const ImVec2 misc_child_size = make_side_child_size(2);
                const ImVec2 misc_left_pos(k_child_margin, k_child_margin);
                const ImVec2 misc_right_pos(
                    misc_left_pos.x + misc_child_size.x + k_side_child_gap,
                    k_child_margin);

                if (draw_side_child("misc_world", "world", misc_left_pos, misc_child_size)) {
                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);
                    widgets::checkbox("no shadow", &g_Settings.world.no_shadow);

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    widgets::slider_float("brightness", &g_Settings.world.brightness, 0.0f, 20.0f, "%.1f");

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    row_checkbox_color("fog", &g_Settings.world.fog,
                        g_Settings.world.fog_color, "world_fog_color");

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                    widgets::slider_float("fog start", &g_Settings.world.fog_start, 0.0f, 100000.0f, "%.0f");

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    widgets::slider_float("fog end", &g_Settings.world.fog_end, 0.0f, 100000.0f, "%.0f");
                }
                widgets::end_child_panel();

                if (draw_side_child("misc_local", "local", misc_right_pos, misc_child_size)) {
                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);
                    widgets::checkbox("fps unlocker", &g_Settings.misc.fps_unlock);
                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    widgets::slider_int("fps cap", &g_Settings.misc.fps_cap, 60, 1000, "%d");

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    widgets::checkbox("fov changer", &g_Settings.misc.fov);
                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    widgets::slider_float("fov value", &g_Settings.misc.fov_value, 10.0f, 120.0f, "%.0f");

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    widgets::checkbox("walkspeed", &g_Settings.misc.walkspeed);
                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    {
                        static const char* k_ws_modes[] = { "cframe", "velocity" };
                        widgets::combo("ws mode", &g_Settings.misc.walkspeed_mode, k_ws_modes, 2);
                    }
                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    widgets::slider_float("ws value", &g_Settings.misc.walkspeed_value, 16.0f, 200.0f, "%.0f");

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    widgets::checkbox("jump power", &g_Settings.misc.jump);
                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    widgets::slider_float("jump value", &g_Settings.misc.jump_power, 50.0f, 500.0f, "%.0f");

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    widgets::checkbox("fly", &g_Settings.misc.fly);
                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    {
                        static const char* k_fly_modes[] = { "cframe", "velocity" };
                        widgets::combo("fly mode", &g_Settings.misc.fly_mode, k_fly_modes, 2);
                    }
                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    widgets::slider_float("fly speed", &g_Settings.misc.fly_speed, 10.0f, 300.0f, "%.0f");

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 1.0f);
                    widgets::keybind("freecam", &g_Settings.misc.freecam_key, &g_Settings.misc.freecam_mode);

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    widgets::slider_float("freecam speed", &g_Settings.misc.freecam_speed, 10.0f, 300.0f, "%.0f");

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    widgets::slider_float("freecam sens", &g_Settings.misc.freecam_sens, 0.05f, 1.0f, "%.2f");

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                    widgets::checkbox("noclip", &g_Settings.misc.noclip);

                    ImGui::SetCursorPosX(6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                    widgets::checkbox("infinite jump", &g_Settings.misc.inf_jump);
                }
                widgets::end_child_panel();
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }
    ImGui::End();
    ImGui::PopStyleVar(3);

    if (!m_bMenuVisible && !animation_running && window_alpha <= 0.0f) {
        return;
    }

    constexpr float k_dock_gap = 6.0f;
    float dock_x = s_menu_pos.x + s_menu_size.x;

    if (g_Settings.esp.preview && active_tab != 3 &&
        (m_bMenuVisible || animation_running || window_alpha > 0.0f))
    {
        constexpr float k_preview_default_w = 300.0f;
        const float preview_x = dock_x + k_dock_gap;

        ImGui::SetNextWindowPos(ImVec2(preview_x, s_menu_pos.y), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(k_preview_default_w, s_menu_size.y), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSizeConstraints(
            ImVec2(200.0f, s_menu_size.y),
            ImVec2(520.0f, s_menu_size.y));
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, window_alpha);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_ResizeGrip,        ImVec4(0.15f, 0.15f, 0.15f, 0.35f));
        {
            ImVec4 a = colors::accent; a.w = 0.7f;
            ImGui::PushStyleColor(ImGuiCol_ResizeGripHovered, a);
            a.w = 1.0f;
            ImGui::PushStyleColor(ImGuiCol_ResizeGripActive, a);
        }

        const ImGuiWindowFlags k_preview_flags =
            ImGuiWindowFlags_NoTitleBar    |
            ImGuiWindowFlags_NoScrollbar   |
            ImGuiWindowFlags_NoScrollWithMouse |
            ImGuiWindowFlags_NoFocusOnAppearing;

        if (ImGui::Begin("##jewsploit_esp_preview", nullptr, k_preview_flags))
        {

            const ImVec2 cur = ImGui::GetWindowSize();
            if (ImFabs(cur.y - s_menu_size.y) > 0.5f)
                ImGui::SetWindowSize(ImVec2(cur.x, s_menu_size.y));

            dock_x = preview_x + ImGui::GetWindowSize().x;

            colors::draw_panel_background(window_alpha);

            constexpr float k_margin = 10.0f;
            const ImVec2 win_sz  = ImGui::GetWindowSize();
            const float  child_w = win_sz.x - k_margin * 2.0f;
            const float  child_h = win_sz.y - k_margin * 2.0f;

            const float title_font_size = fonts::tahoma && fonts::tahoma->LegacySize > 0.0f
                ? fonts::tahoma->LegacySize : 13.0f;

            ImGui::SetCursorPos(ImVec2(k_margin, k_margin));
            if (widgets::begin_child_panel(
                    "esp_preview_child",
                    ImVec2(child_w, child_h),
                    "esp preview",
                    fonts::tahoma,
                    title_font_size,
                    nullptr, nullptr, nullptr))
            {
                Cheat::Visuals::ESPPreview::Render();
            }
            widgets::end_child_panel();
        }
        ImGui::End();
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar(3);
    }

    if (active_tab == 4 && g_Settings.misc.custom_support &&
        (m_bMenuVisible || animation_running || window_alpha > 0.0f))
    {
        constexpr float k_cs_default_w = 280.0f;
        const float cs_x = dock_x + k_dock_gap;

        ImGui::SetNextWindowPos(ImVec2(cs_x, s_menu_pos.y), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(k_cs_default_w, s_menu_size.y), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSizeConstraints(
            ImVec2(200.0f, s_menu_size.y),
            ImVec2(480.0f, s_menu_size.y));
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, window_alpha);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_ResizeGrip,        ImVec4(0.15f, 0.15f, 0.15f, 0.35f));
        {
            ImVec4 a = colors::accent; a.w = 0.7f;
            ImGui::PushStyleColor(ImGuiCol_ResizeGripHovered, a);
            a.w = 1.0f;
            ImGui::PushStyleColor(ImGuiCol_ResizeGripActive, a);
        }

        const ImGuiWindowFlags k_cs_flags =
            ImGuiWindowFlags_NoTitleBar    |
            ImGuiWindowFlags_NoScrollbar   |
            ImGuiWindowFlags_NoScrollWithMouse |
            ImGuiWindowFlags_NoFocusOnAppearing;

        if (ImGui::Begin("##jewsploit_custom", nullptr, k_cs_flags))
        {
            const ImVec2 cur = ImGui::GetWindowSize();
            if (ImFabs(cur.y - s_menu_size.y) > 0.5f)
                ImGui::SetWindowSize(ImVec2(cur.x, s_menu_size.y));
            dock_x = cs_x + ImGui::GetWindowSize().x;

            colors::draw_panel_background(window_alpha);

            constexpr float k_margin = 10.0f;
            const ImVec2 win_sz  = ImGui::GetWindowSize();
            const float  child_w = win_sz.x - k_margin * 2.0f;
            const float  child_h = win_sz.y - k_margin * 2.0f;
            const float  tf = fonts::tahoma && fonts::tahoma->LegacySize > 0.0f
                ? fonts::tahoma->LegacySize : 13.0f;

            ImGui::SetCursorPos(ImVec2(k_margin, k_margin));
            if (widgets::begin_child_panel(
                    "custom_child", ImVec2(child_w, child_h),
                    "custom support", fonts::tahoma, tf, nullptr, nullptr, nullptr))
            {
                static char s_label[64] = "";
                ImGui::SetCursorPosX(6.0f);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);
                const bool submit = widgets::input_text(
                    "##cs_label", "target name", s_label, IM_ARRAYSIZE(s_label),
                    0.0f, ImGuiInputTextFlags_EnterReturnsTrue);

                ImGui::SetCursorPosX(6.0f);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 1.0f);
                if ((widgets::button("add target", ImVec2(90.0f, 0.0f)) || submit)
                    && s_label[0] != '\0') {
                    Cheat::CustomTarget t{};
                    ImFormatString(t.label, IM_ARRAYSIZE(t.label), "%s", s_label);
                    Cheat::g_CustomTargets.push_back(t);
                    s_label[0] = '\0';
                }

                ImGui::SetCursorPosX(4.0f);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);
                const float list_w = ImGui::GetContentRegionAvail().x - 2.0f;
                const float list_h = ImMax(120.0f, ImGui::GetContentRegionAvail().y - 4.0f);

                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
                if (ImGui::BeginChild("cs_scroll", ImVec2(list_w, list_h), false))
                {
                    int remove = -1;
                    for (std::size_t i = 0; i < Cheat::g_CustomTargets.size(); ++i) {
                        Cheat::CustomTarget& t = Cheat::g_CustomTargets[i];
                        ImGui::PushID(static_cast<int>(i));

                        const float blk_w = ImGui::GetContentRegionAvail().x - 2.0f;
                        ImGui::SetCursorPosX(2.0f);
                        if (widgets::begin_child_panel(
                                "cs_blk", ImVec2(blk_w, 270.0f),
                                t.label[0] ? t.label : "target",
                                fonts::tahoma, tf, nullptr, nullptr, nullptr))
                        {
                            ImGui::SetCursorPosX(6.0f);
                            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);
                            widgets::checkbox("enabled", &t.enabled);
                            ImGui::SameLine();
                            ImGui::SetCursorPosX(blk_w - 24.0f);
                            if (widgets::button("x", ImVec2(16.0f, 0.0f)))
                                remove = static_cast<int>(i);

                            ImGui::SetCursorPosX(6.0f);
                            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                            {
                                static const char* k_kind[] = { "folder", "model" };
                                widgets::combo("type", &t.kind, k_kind, 2);
                            }

                            ImGui::SetCursorPosX(6.0f);
                            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                            {
                                static const char* k_resolve[] = { "exact path", "by name" };
                                widgets::combo("resolve", &t.resolve, k_resolve, 2);
                            }

                            ImGui::SetCursorPosX(6.0f);
                            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 1.0f);
                            widgets::input_text("##cs_q",
                                t.resolve == 0 ? "paste path / address" : "paste name",
                                t.query, IM_ARRAYSIZE(t.query), 0.0f, 0);

                            ImGui::SetCursorPosX(6.0f);
                            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);
                            const float vis_w = ImGui::GetContentRegionAvail().x - 6.0f;
                            if (widgets::begin_child_panel(
                                    "cs_vis", ImVec2(vis_w, 118.0f),
                                    "visuals", fonts::tahoma, tf, nullptr, nullptr, nullptr))
                            {
                                ImGui::SetCursorPosX(6.0f);
                                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);
                                row_checkbox_color("box", &t.vis.box, t.vis.box_color, "cs_box");

                                ImGui::SetCursorPosX(6.0f);
                                ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                                row_checkbox_color("filled", &t.vis.filled, t.vis.fill_color, "cs_fill");

                                ImGui::SetCursorPosX(6.0f);
                                ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                                row_checkbox_color("name", &t.vis.name, t.vis.name_color, "cs_name");

                                ImGui::SetCursorPosX(6.0f);
                                ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                                row_checkbox_color("distance", &t.vis.distance, t.vis.distance_color, "cs_dist");

                                ImGui::SetCursorPosX(6.0f);
                                ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                                row_checkbox_color("tracer", &t.vis.tracer, t.vis.tracer_color, "cs_tracer");
                            }
                            widgets::end_child_panel();
                        }
                        widgets::end_child_panel();

                        ImGui::PopID();
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f);
                    }

                    if (remove >= 0)
                        Cheat::g_CustomTargets.erase(Cheat::g_CustomTargets.begin() + remove);
                }
                ImGui::EndChild();
                ImGui::PopStyleColor();
            }
            widgets::end_child_panel();
        }
        ImGui::End();
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar(3);
    }
}

void Menu::Shutdown()
{
    if (!m_bInitialized) return;

    Cheat::Features::Misc::Stop();
    Cheat::Visuals::ESPPreview::Shutdown();
    Cheat::Features::Explorer::Shutdown();

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

void Menu::InvalidateDeviceObjects()
{
    if (!m_bInitialized) return;
    ImGui_ImplDX11_InvalidateDeviceObjects();
}

void Menu::CreateDeviceObjects()
{
    if (!m_bInitialized) return;
    ImGui_ImplDX11_CreateDeviceObjects();
}

bool Menu::HandleMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (!m_bInitialized) return false;
    return ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);
}

}
}
