#define NOMINMAX
#include "Aim.h"
#include "RaycastSilent.h"
#include "MagicBullet.h"
#include "../../Settings.h"
#include "../../Globals/Globals.h"
#include "../../Memory/Memory.h"
#include "../../Roblox/Engine/Offsets/Offsets.h"
#include "../../Roblox/Math/Math.h"
#include "../../Roblox/Engine/Classes/Classes.h"
#include "../../PlayerHandler/PlayerHandler.h"
#include "../../../Renderer/Renderer.h"
#include "../../../GUI/imgui/imgui.h"
#include <Windows.h>
#include <cmath>
#include <cfloat>
#include <algorithm>

namespace Cheat {
    namespace Features {

        namespace {
            using Config = Settings::AimbotConfig;

            bool s_was_pressed = false;
            bool s_toggled     = false;
            bool s_force_mb_was = false;
            bool s_force_mb_tog = false;

            std::uint64_t s_target      = 0;
            int           s_cycle       = 0;
            double        s_last_switch = 0.0;
            double        s_engage_at   = 0.0;
            std::uint64_t s_pending     = 0;
            int           s_aim_part    = Settings::AIM_HEAD;
            Vector3       s_aim_point{};

            const Instance* part_instance(const PlayerCache& c, int part) {
                switch (part) {
                    case Settings::AIM_HEAD:        return c.head.get();
                    case Settings::AIM_UPPER_TORSO: return c.upperTorso.get();
                    case Settings::AIM_LOWER_TORSO: return c.lowerTorso.get();
                    case Settings::AIM_HRP:         return c.humanoidRootPart.get();
                    case Settings::AIM_LEFT_HAND:   return c.leftHand.get();
                    case Settings::AIM_RIGHT_HAND:  return c.rightHand.get();
                    case Settings::AIM_LEFT_FOOT:   return c.leftFoot.get();
                    case Settings::AIM_RIGHT_FOOT:  return c.rightFoot.get();
                    default:                        return nullptr;
                }
            }

            int current_part(const Config& cfg) {
                int enabled[Settings::AIM_PART_COUNT];
                int n = 0;
                for (int i = 0; i < Settings::AIM_PART_COUNT; ++i)
                    if (cfg.parts[i]) enabled[n++] = i;
                if (n == 0) return Settings::AIM_HEAD;

                const double now = ImGui::GetTime();
                if (n > 1 && now - s_last_switch >= cfg.switch_time) {
                    s_cycle = (s_cycle + 1) % n;
                    s_last_switch = now;
                }
                if (s_cycle >= n) s_cycle = 0;
                return enabled[s_cycle];
            }

            ImVec2 overlay_size() {
                if (HWND oh = Renderer::GetHwnd()) {
                    RECT ocr{};
                    if (GetClientRect(oh, &ocr)) {
                        return ImVec2(
                            (float)(std::max)(1L, ocr.right - ocr.left),
                            (float)(std::max)(1L, ocr.bottom - ocr.top));
                    }
                }
                return ImGui::GetIO().DisplaySize;
            }

            ImVec2 cursor_client() {
                POINT p{};
                const ImVec2 sz = overlay_size();
                if (!GetCursorPos(&p))
                    return ImVec2(sz.x * 0.5f, sz.y * 0.5f);

                if (HWND overlay = Renderer::GetHwnd())
                    ScreenToClient(overlay, &p);

                float x = static_cast<float>(p.x);
                float y = static_cast<float>(p.y);
                if (x < 0.f || y < 0.f || x > sz.x || y > sz.y)
                    return ImVec2(sz.x * 0.5f, sz.y * 0.5f);
                return ImVec2(x, y);
            }

            ImVec2 fov_anchor(const Config& cfg) {
                const ImVec2 sz = overlay_size();
                if (cfg.fov_position == 1)
                    return cursor_client();
                return ImVec2(sz.x * 0.5f, sz.y * 0.5f);
            }

            void draw_fov(const Config& cfg) {
                if (!cfg.fov_enabled) return;
                ImDrawList* dl = ImGui::GetBackgroundDrawList();
                if (!dl) return;

                const ImVec2 center = fov_anchor(cfg);
                const float  radius = (std::max)(1.0f, cfg.fov_size);
                const ImU32  outline = ImGui::ColorConvertFloat4ToU32(ImVec4(
                    cfg.fov_outline_color[0], cfg.fov_outline_color[1],
                    cfg.fov_outline_color[2], cfg.fov_outline_color[3]));

                if (cfg.fov_style == 1) {
                    const float fill_a = cfg.fov_color[3] * 0.35f;
                    const float rim_a  = (std::min)(1.0f, cfg.fov_color[3]);
                    const ImU32 fill = ImGui::ColorConvertFloat4ToU32(ImVec4(
                        cfg.fov_color[0], cfg.fov_color[1], cfg.fov_color[2], fill_a));
                    const ImU32 rim = ImGui::ColorConvertFloat4ToU32(ImVec4(
                        cfg.fov_color[0], cfg.fov_color[1], cfg.fov_color[2], rim_a));
                    dl->AddCircleFilled(center, radius, fill, 64);
                    dl->AddCircle(center, radius, rim, 64, 1.75f);
                } else {
                    const ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(
                        cfg.fov_color[0], cfg.fov_color[1], cfg.fov_color[2], cfg.fov_color[3]));
                    dl->AddCircle(center, radius, col, 64, 1.5f);
                }

                if (cfg.fov_outline) {
                    dl->AddCircle(center, radius + 1.0f, outline, 64, 2.0f);
                    dl->AddCircle(center, radius - 1.0f,
                        IM_COL32(0, 0, 0, (int)(cfg.fov_outline_color[3] * 100.0f)), 64, 1.0f);
                }
            }

            bool world_to_screen(const Matrix4x4& m, const Vector2& dim,
                                 const Vector3& p, Vector2& out) {
                const float w = p.x*m.m[3][0] + p.y*m.m[3][1] + p.z*m.m[3][2] + m.m[3][3];
                if (w < 0.01f) return false;
                const float x = p.x*m.m[0][0] + p.y*m.m[0][1] + p.z*m.m[0][2] + m.m[0][3];
                const float y = p.x*m.m[1][0] + p.y*m.m[1][1] + p.z*m.m[1][2] + m.m[1][3];
                const float inv = 1.0f / w;
                out.x = (dim.x * 0.5f) + (x * inv * dim.x * 0.5f);
                out.y = (dim.y * 0.5f) - (y * inv * dim.y * 0.5f);

                if (HWND oh = Renderer::GetHwnd()) {
                    RECT ocr{};
                    if (GetClientRect(oh, &ocr) && dim.x > 1.0f && dim.y > 1.0f) {
                        out.x *= (float)(ocr.right - ocr.left) / dim.x;
                        out.y *= (float)(ocr.bottom - ocr.top) / dim.y;
                    }
                }
                return true;
            }

            struct Scene {
                bool          ok = false;
                Matrix4x4     view{};
                Vector2       viewport{};
                Camera        camera{ 0 };
                Vector3       cam_pos{};
                std::uint64_t local_player = 0;
                std::uint64_t local_char   = 0;
            };

            bool is_local_entry(const PlayerCache& cache, const Scene& sc) {
                if (sc.local_player && cache.address == sc.local_player) return true;
                if (sc.local_char && cache.address == sc.local_char) return true;
                return false;
            }

            Scene resolve_scene() {
                Scene s;
                if (!Globals::Workspace || !Globals::Players) return s;
                auto cam = Globals::Workspace->GetCurrentCamera();
                if (!cam) return s;

                s.camera   = Camera(cam->address);
                s.viewport = s.camera.GetViewportSize();
                s.cam_pos  = s.camera.GetPosition();

                static uintptr_t base = g_Memory.GetModuleBase();
                if (!base) base = g_Memory.GetModuleBase();
                const uintptr_t ve = g_Memory.Read<uintptr_t>(base + Offsets::VisualEngine::Pointer);
                s.view = g_Memory.Read<Matrix4x4>(ve + Offsets::VisualEngine::ViewMatrix);

                if (g_Memory.IsValid(Globals::Players->address)) {
                    s.local_player = g_Memory.Read<std::uint64_t>(
                        Globals::Players->address + Offsets::Player::LocalPlayer);
                    if (g_Memory.IsValid(s.local_player))
                        s.local_char = g_Memory.Read<std::uint64_t>(
                            s.local_player + Offsets::Player::ModelInstance);
                }
                s.ok = true;
                return s;
            }

            bool select_target(const Config& cfg, const Scene& sc,
                               Vector3& out_world, Vector2& out_screen) {
                const int     part   = current_part(cfg);
                const ImVec2  anchor = fov_anchor(cfg);
                const Vector2 anchor_v(anchor.x, anchor.y);
                const float   fov_r  = cfg.fov_enabled ? (std::max)(1.0f, cfg.fov_size) : 1e9f;
                const float   sticky_r = fov_r * (std::max)(1.0f, cfg.sticky_fov_scale);

                bool    found_sticky = false;
                Vector3 sticky_world{};
                Vector2 sticky_screen{};

                bool          found_best = false;
                float         best_dist  = FLT_MAX;
                std::uint64_t best_addr  = 0;
                Vector3       best_world{};
                Vector2       best_screen{};

                if (s_target && (s_target == sc.local_player || s_target == sc.local_char))
                    s_target = 0;
                if (s_pending && (s_pending == sc.local_player || s_pending == sc.local_char))
                    s_pending = 0;

                PlayerHandler::ForEachPlayer([&](const PlayerCache& cache) {
                    if (is_local_entry(cache, sc)) return;

                    const Instance* p = part_instance(cache, part);
                    if (!p || !g_Memory.IsValid(p->address)) return;

                    BasePart      bp(p->address);
                    const Vector3 world = bp.GetPosition();

                    if (cfg.distance_check) {
                        const float dist3 = (world - sc.cam_pos).Length();
                        if (dist3 > cfg.max_distance) return;
                    }

                    Vector2 screen{};
                    if (!world_to_screen(sc.view, sc.viewport, world, screen)) return;

                    const float dx = screen.x - anchor_v.x;
                    const float dy = screen.y - anchor_v.y;
                    const float d  = std::sqrt(dx * dx + dy * dy);

                    if (cfg.sticky && s_target != 0 && cache.address == s_target &&
                        d <= sticky_r) {
                        found_sticky  = true;
                        sticky_world  = world;
                        sticky_screen = screen;
                    }

                    if (d > fov_r) return;
                    if (d < best_dist) {
                        best_dist   = d;
                        best_addr   = cache.address;
                        best_world  = world;
                        best_screen = screen;
                        found_best  = true;
                    }
                });

                if (cfg.sticky && found_sticky) {
                    out_world  = sticky_world;
                    out_screen = sticky_screen;
                    s_aim_part  = part;
                    s_aim_point = sticky_world;
                    return true;
                }

                if (!found_best) {
                    s_pending = 0;
                    s_target  = 0;
                    return false;
                }

                if (!cfg.sticky) {
                    s_target   = best_addr;
                    s_pending  = 0;
                    out_world  = best_world;
                    out_screen = best_screen;
                    s_aim_part  = part;
                    s_aim_point = best_world;
                    return true;
                }

                const double now = ImGui::GetTime();
                if (cfg.humanize && cfg.reaction_ms > 0.0f && best_addr != s_target) {
                    if (best_addr != s_pending) {
                        s_pending   = best_addr;
                        s_engage_at = now + cfg.reaction_ms / 1000.0;
                        return false;
                    }
                    if (now < s_engage_at) return false;
                }

                s_target   = best_addr;
                s_pending  = 0;
                out_world  = best_world;
                out_screen = best_screen;
                s_aim_part  = part;
                s_aim_point = best_world;
                return true;
            }

            void apply_mouse(const Config& cfg, const Vector2& target_screen) {
                const ImVec2 cur = fov_anchor(cfg);

                const float sx = (std::max)(0.1f, cfg.smooth_x);
                const float sy = (std::max)(0.1f, cfg.smooth_y);
                float dx = (target_screen.x - cur.x) / sx;
                float dy = (target_screen.y - cur.y) / sy;

                constexpr float k_max_step = 80.0f;
                dx = std::clamp(dx, -k_max_step, k_max_step);
                dy = std::clamp(dy, -k_max_step, k_max_step);

                const int idx = static_cast<int>(std::lround(dx));
                const int idy = static_cast<int>(std::lround(dy));
                if (idx == 0 && idy == 0) return;

                INPUT in{};
                in.type = INPUT_MOUSE;
                in.mi.dx = idx;
                in.mi.dy = idy;
                in.mi.dwFlags = MOUSEEVENTF_MOVE;
                SendInput(1, &in, sizeof(in));
            }

            void apply_camera(const Config& cfg, const Scene& sc, const Vector3& target_world) {
                if (!g_Memory.IsValid(sc.camera.address)) return;

                Vector3 want = (target_world - sc.cam_pos);
                if (want.LengthSquared() < 1e-6f) return;
                want.Normalize();

                const Matrix4x4 cur = sc.camera.GetRotation();
                Vector3 cur_look(-cur.m[0][2], -cur.m[1][2], -cur.m[2][2]);
                if (cur_look.LengthSquared() < 1e-6f) cur_look = want;
                cur_look.Normalize();

                const float tx = 1.0f / (std::max)(0.1f, cfg.smooth_x);
                const float ty = 1.0f / (std::max)(0.1f, cfg.smooth_y);
                const float tz = (tx + ty) * 0.5f;
                Vector3 look(
                    cur_look.x + (want.x - cur_look.x) * tx,
                    cur_look.y + (want.y - cur_look.y) * ty,
                    cur_look.z + (want.z - cur_look.z) * tz);
                if (look.LengthSquared() < 1e-6f) return;
                look.Normalize();

                const Vector3 world_up(0.0f, 1.0f, 0.0f);
                Vector3 right = look.Cross(world_up);
                if (right.LengthSquared() < 1e-6f) right = Vector3(1.0f, 0.0f, 0.0f);
                right.Normalize();
                Vector3 up = right.Cross(look);
                const Vector3 back = -look;

                Matrix4x4 rot;
                rot.m[0][0] = right.x; rot.m[0][1] = up.x; rot.m[0][2] = back.x;
                rot.m[1][0] = right.y; rot.m[1][1] = up.y; rot.m[1][2] = back.y;
                rot.m[2][0] = right.z; rot.m[2][1] = up.z; rot.m[2][2] = back.z;
                sc.camera.SetRotation(rot);
            }

        }

        void Aim::Render() {
            Config& cfg = g_Settings.aim.active();

            RaycastSilent::Ensure(true);

            draw_fov(cfg);

            const int key = g_Settings.aim.bind;
            if (key == 0) {
                s_was_pressed = false;
                s_toggled = false;
                RaycastSilent::SetActive(false);
                return;
            }

            const bool pressed = (GetAsyncKeyState(key) & 0x8000) != 0;
            bool       active  = false;
            if (g_Settings.aim.bind_mode == 1) {
                if (pressed && !s_was_pressed) s_toggled = !s_toggled;
                active = s_toggled;
            } else {
                active = pressed;
            }
            s_was_pressed = pressed;

            if (!active) {
                s_target = 0;
                s_pending = 0;
                RaycastSilent::SetActive(false);
                return;
            }

            const Scene sc = resolve_scene();
            if (!sc.ok) {
                s_target = 0;
                RaycastSilent::SetActive(false);
                return;
            }

            Vector3 world{};
            Vector2 screen{};
            if (!select_target(cfg, sc, world, screen)) {
                s_target = 0;
                RaycastSilent::SetActive(false);
                return;
            }

            if (g_Settings.aim.type == 0) {
                RaycastSilent::SetActive(false);
                apply_mouse(cfg, screen);
            } else if (g_Settings.aim.type == 1) {
                RaycastSilent::SetActive(false);
                apply_camera(cfg, sc, world);
            } else if (g_Settings.aim.silent_method == Settings::SILENT_RAYCAST) {

                bool force_mb = false;
                const int fk = g_Settings.aim.force_magic_key;
                if (fk != 0) {
                    const bool fdown = (GetAsyncKeyState(fk) & 0x8000) != 0;
                    if (g_Settings.aim.force_magic_mode == 1) {
                        if (fdown && !s_force_mb_was)
                            s_force_mb_tog = !s_force_mb_tog;
                        force_mb = s_force_mb_tog;
                    } else {
                        force_mb = fdown;
                    }
                    s_force_mb_was = fdown;
                    g_Settings.aim.force_magic_bullet = force_mb;
                } else if (g_Settings.aim.force_magic_bullet) {
                    force_mb = true;
                    s_force_mb_was = false;
                } else {
                    s_force_mb_was = false;
                    s_force_mb_tog = false;
                }

                if (force_mb)
                    MagicBullet::SetActive(true, world);
                else
                    RaycastSilent::SetActive(true, world, false);
            } else if (g_Settings.aim.silent_method == Settings::SILENT_MAGIC_BULLET) {
                MagicBullet::SetActive(true, world);
            } else {

                RaycastSilent::SetActive(false);
            }
        }

        std::uint64_t Aim::CurrentTarget() { return s_target; }
        int     Aim::CurrentAimPart()  { return s_aim_part; }
        Vector3 Aim::CurrentAimPoint() { return s_aim_point; }

        void Aim::mouse()    {}
        void Aim::viewport() {}

    }
}
