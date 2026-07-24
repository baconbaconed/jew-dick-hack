#pragma once

#include <vector>

namespace Cheat {

    struct Settings {
        struct {
            bool enabled{ false };
            bool draw_local{ false };
            bool box{ false };
            bool name{ false };
            bool skeleton{ false };
            bool chams{ false };

            int  chams_mode{ 0 };
            int  chams_shader{ 0 };
            bool preview{ false };

            bool healthbar{ false };
            bool health_text{ false };
            bool distance{ false };
            bool tool{ false };
            bool flags{ false };

            int   font{ 0 };
            float font_size{ 13.f };
            int   box_mode{ 0 };
            int   name_mode{ 0 };
            int   distance_unit{ 0 };
            bool  distance_check{ false };
            float max_distance{ 1000.f };

            float box_color[4]{ 1.f, 1.f, 1.f, 1.f };
            float name_color[4]{ 1.f, 1.f, 1.f, 1.f };
            float skeleton_color[4]{ 1.f, 1.f, 1.f, 1.f };
            float chams_outline_color[4]{ 1.f, 1.f, 1.f, 1.f };
            float chams_fill_color[4]{ 1.f, 1.f, 1.f, 0.4f };
            float distance_color[4]{ 1.f, 1.f, 1.f, 1.f };
            float tool_color[4]{ 1.f, 1.f, 1.f, 1.f };
        } esp;

        enum AimPart {
            AIM_HEAD = 0,
            AIM_UPPER_TORSO,
            AIM_LOWER_TORSO,
            AIM_HRP,
            AIM_LEFT_HAND,
            AIM_RIGHT_HAND,
            AIM_LEFT_FOOT,
            AIM_RIGHT_FOOT,
            AIM_PART_COUNT
        };

        struct AimbotConfig {
            bool  fov_enabled{ false };
            float fov_size{ 120.0f };
            int   fov_position{ 0 };
            bool  distance_check{ false };
            float max_distance{ 1000.0f };
            bool  visible_only{ false };

            bool  parts[AIM_PART_COUNT]{ false, false, false, false,
                                         false, false, false, false };
            float switch_time{ 0.40f };

            float smooth_x{ 1.0f };
            float smooth_y{ 1.0f };

            bool  humanize{ false };
            float reaction_ms{ 60.0f };
            bool  sticky{ false };
            float sticky_fov_scale{ 1.5f };

            float fov_color[4]{ 0.20f, 0.478f, 0.906f, 0.86f };
        };

        enum SilentMethod {
            SILENT_VIEWPORT = 0,
            SILENT_MOUSE,
            SILENT_RAYCAST,
            SILENT_MAGIC_BULLET,
            SILENT_METHOD_COUNT
        };

        struct {
            int  bind{ 0 };
            int  bind_mode{ 0 };
            int  type{ 0 };
            int  silent_method{ SILENT_RAYCAST };

            bool force_magic_bullet{ false };
            int  force_magic_key{ 0 };
            int  force_magic_mode{ 0 };

            AimbotConfig mouse;
            AimbotConfig camera;
            AimbotConfig silent;

            AimbotConfig& active() {
                if (type == 1) return camera;
                if (type == 2) return silent;
                return mouse;
            }

            bool silent_raycast() const {
                return type == 2 && silent_method == SILENT_RAYCAST;
            }
            bool silent_magic() const {
                return type == 2 && silent_method == SILENT_MAGIC_BULLET;
            }

            bool silent_uses_raycast_hook() const {
                return silent_raycast() || silent_magic();
            }
        } aim;

        struct {
            bool  no_shadow{ false }; float brightness{ 5.0f };
            bool  fog{ false };
            float fog_start{ 0.f };
            float fog_end{ 100000.f };
            float fog_color[4]{ 1.f, 1.f, 1.f, 1.f };
        } world;

        struct {
            bool enabled{ false };
            int  effect{ 0 };
        } killfx;

        struct {
            bool  enabled{ false };
            float size{ 1.0f };
            float duration{ 0.55f };
        } hitmarker;

        struct {
            bool  enabled{ false };
            int   index{ 0 };
            float volume{ 100.0f };
        } hitsound;

        enum HitDataMode {
            HITDATA_TYPE = 0,
            HITDATA_DAMAGE,
            HITDATA_HEALTH,
            HITDATA_DISTANCE,
            HITDATA_PART,
            HITDATA_MODE_COUNT
        };
        struct {
            bool  enabled{ false };
            bool  modes[HITDATA_MODE_COUNT]{ true, false, false, false, false };
            float duration{ 1.35f };
            float size{ 15.0f };
        } hitdata;

        struct {
            bool  fps_unlock{ false }; int   fps_cap{ 240 };
            bool  fov{ false };        float fov_value{ 70.f };
            bool  walkspeed{ false };  int   walkspeed_mode{ 0 };
            float walkspeed_value{ 32.f };
            bool  jump{ false };       float jump_power{ 50.f };
            bool  fly{ false };        int   fly_mode{ 0 };
            float fly_speed{ 60.f };

            bool  explorer{ false };
            bool  custom_support{ false };

            int   freecam_key{ 0 };
            int   freecam_mode{ 0 };
            float freecam_speed{ 60.f };
            float freecam_sens{ 0.30f };

            bool  noclip{ false };
            bool  inf_jump{ false };
        } misc;

        struct {
            int   theme{ 0 };
            float accent[4]{ 51.f / 255.f, 122.f / 255.f, 231.f / 255.f, 1.f };
            float text_active[4]{ 1.f, 1.f, 1.f, 1.f };
            float text_inactive[4]{ 136.f / 255.f, 136.f / 255.f, 136.f / 255.f, 1.f };
            float outer_border[4]{ 0.f, 0.f, 0.f, 1.f };
            float inner_border[4]{ 31.f / 255.f, 30.f / 255.f, 31.f / 255.f, 1.f };
            float panel_fill[4]{ 17.f / 255.f, 17.f / 255.f, 16.f / 255.f, 1.f };
            float content_outer[4]{ 31.f / 255.f, 30.f / 255.f, 31.f / 255.f, 1.f };
            float content_inner[4]{ 0.f, 0.f, 0.f, 1.f };
            float content_fill[4]{ 21.f / 255.f, 21.f / 255.f, 20.f / 255.f, 1.f };
            float child_fill[4]{ 15.f / 255.f, 14.f / 255.f, 14.f / 255.f, 1.f };
        } gui;
    };

    inline Settings g_Settings;

    struct CustomVisuals {
        bool  box{ false };       float box_color[4]{ 1.f, 1.f, 1.f, 1.f };
        bool  filled{ false };    float fill_color[4]{ 1.f, 1.f, 1.f, 0.25f };
        bool  name{ false };      float name_color[4]{ 1.f, 1.f, 1.f, 1.f };
        bool  distance{ false };  float distance_color[4]{ 1.f, 1.f, 1.f, 1.f };
        bool  tracer{ false };    float tracer_color[4]{ 1.f, 1.f, 1.f, 1.f };
    };

    struct CustomTarget {
        char label[64]{ "" };
        int  kind{ 0 };
        int  resolve{ 0 };
        char query[128]{ "" };
        bool enabled{ false };
        CustomVisuals vis;
    };
    inline std::vector<CustomTarget> g_CustomTargets;

}
