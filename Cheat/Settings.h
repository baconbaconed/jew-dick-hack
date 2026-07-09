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
            bool outline{ false };
            bool chams{ false };
            int  chams_mode{ 0 };
            bool preview{ true };

            bool healthbar{ false };
            bool health_text{ false };
            bool distance{ false };
            bool tool{ false };
            bool flags{ false };

            int   font{ 0 };
            float font_size{ 14.f };
            int   box_mode{ 0 };
            int   name_mode{ 0 };
            int   distance_unit{ 0 };
            bool  distance_check{ false };
            float max_distance{ 1000.f };

            float box_color[4]{ 1.f, 1.f, 1.f, 1.f };
            float name_color[4]{ 1.f, 1.f, 1.f, 1.f };
            float skeleton_color[4]{ 1.f, 1.f, 1.f, 1.f };
            float outline_color[4]{ 1.f, 1.f, 1.f, 1.f };
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

        static constexpr int kAimCurvePoints = 6;

        static float sample_curve(const float* pts, float t) {
            t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
            const float f = t * (kAimCurvePoints - 1);
            const int   i = static_cast<int>(f);
            if (i >= kAimCurvePoints - 1) return pts[kAimCurvePoints - 1];
            const float frac = f - i;
            return pts[i] + (pts[i + 1] - pts[i]) * frac;
        }

        struct AimbotConfig {
            bool  fov_enabled{ true };
            float fov_size{ 120.0f };
            int   fov_position{ 0 };
            bool  distance_check{ false };
            float max_distance{ 1000.0f };
            bool  visible_only{ true };

            bool  parts[AIM_PART_COUNT]{ true, false, false, false,
                                         false, false, false, false };
            float switch_time{ 0.40f };

            float smoothness{ 6.0f };

            bool  humanize{ false };
            float reaction_ms{ 60.0f };
            float jitter{ 1.5f };
            bool  sticky{ true };
            float sticky_fov_scale{ 1.5f };

            float jitter_curve[kAimCurvePoints]{ 0.15f, 0.30f, 0.50f, 0.68f, 0.85f, 1.0f };
            float smooth_curve[kAimCurvePoints]{ 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };

            float fov_color[4]{ 0.20f, 0.478f, 0.906f, 0.86f };
        };

        struct {
            int  bind{ 0 };
            int  bind_mode{ 0 };
            int  type{ 0 };
            bool params_window{ false };

            AimbotConfig mouse;
            AimbotConfig camera;

            AimbotConfig& active() {
                return type == 1 ? camera : mouse;
            }
        } aim;

        struct {
            bool  time{ false };   float time_value{ 14.f };
            bool  grass{ false };  float grass_length{ 0.5f };
            bool  stars{ false };  int   star_count{ 3000 };
            bool  sun{ false };    float sun_size{ 21.f };
            bool  moon{ false };   float moon_size{ 11.f };
        } world;

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
            bool  panel{ false };
            bool  fullbright{ false };  float brightness{ 3.0f };
            bool  ambient{ false };     float ambient_color[4]{ 1.f, 1.f, 1.f, 1.f };
            bool  outdoor{ false };     float outdoor_color[4]{ 1.f, 1.f, 1.f, 1.f };
            bool  fog{ false };         float fog_end{ 100000.f };
            bool  fog_color_on{ false };float fog_color[4]{ 1.f, 1.f, 1.f, 1.f };
        } wvis;
    };

    inline Settings g_Settings;

    struct CustomVisuals {
        bool  box{ true };        float box_color[4]{ 1.f, 1.f, 1.f, 1.f };
        bool  filled{ false };    float fill_color[4]{ 1.f, 1.f, 1.f, 0.25f };
        bool  name{ false };      float name_color[4]{ 1.f, 1.f, 1.f, 1.f };
        bool  distance{ false };  float distance_color[4]{ 1.f, 1.f, 1.f, 1.f };
        bool  tracer{ false };    float tracer_color[4]{ 1.f, 1.f, 1.f, 1.f };
    };

    struct CustomTarget {
        char label[64]{ "" };
        int  kind{ 0 };            // 0 = folder, 1 = model
        int  resolve{ 0 };         // 0 = exact path/address, 1 = by name
        char query[128]{ "" };
        bool enabled{ true };
        CustomVisuals vis;
    };
    inline std::vector<CustomTarget> g_CustomTargets;

}
