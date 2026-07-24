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
#include <cstdint>

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
            std::uint32_t s_rng         = 0xA341316Cu;

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

            std::uint32_t next_rng() {
                s_rng ^= s_rng << 13;
                s_rng ^= s_rng >> 17;
                s_rng ^= s_rng << 5;
                return s_rng;
            }

            int part_tier_of(const Config& cfg, int part) {
                if (part < 0 || part >= Settings::AIM_PART_COUNT) return Settings::PART_OFF;
                int t = cfg.part_tier[part];
                if (t == Settings::PART_OFF && cfg.parts[part])
                    t = Settings::PART_PRIMARY;
                return t;
            }

            struct PartSample {
                int     part = Settings::AIM_HEAD;
                int     tier = Settings::PART_PRIMARY;
                Vector3 world{};
                Vector2 screen{};
                float   dist = FLT_MAX;
            };

            int pick_among(const Config& cfg, PartSample* list, int n, bool allow_reroll) {
                if (n <= 0) return -1;
                if (n == 1) return 0;

                if (cfg.part_select == Settings::PART_SELECT_CLOSEST) {
                    int best = 0;
                    for (int i = 1; i < n; ++i)
                        if (list[i].dist < list[best].dist) best = i;
                    return best;
                }

                const double now = ImGui::GetTime();
                if (allow_reroll && now - s_last_switch >= (std::max)(0.05f, cfg.switch_time)) {
                    if (cfg.part_select == Settings::PART_SELECT_RANDOM)
                        s_cycle = (int)(next_rng() % (std::uint32_t)n);
                    else
                        s_cycle = (s_cycle + 1) % n;
                    s_last_switch = now;
                }
                if (s_cycle < 0 || s_cycle >= n) s_cycle = 0;
                return s_cycle;
            }

            bool hitchance_ok(const Config& cfg) {
                if (!cfg.hitchance_enabled) return true;
                const float chance = std::clamp(cfg.hitchance, 1.0f, 100.0f);
                const float roll = (float)(next_rng() % 10000u) / 100.0f;
                return roll <= chance;
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

            bool game_client_screen_rect(RECT& out) {
                HWND hwnd = Renderer::GetGameHwnd();
                if (!hwnd || !IsWindow(hwnd))
                    hwnd = Renderer::GetHwnd();
                if (!hwnd || !IsWindow(hwnd))
                    return false;

                RECT cr{};
                if (!GetClientRect(hwnd, &cr))
                    return false;

                POINT tl{ cr.left, cr.top };
                POINT br{ cr.right, cr.bottom };
                ClientToScreen(hwnd, &tl);
                ClientToScreen(hwnd, &br);
                if (br.x - tl.x < 8 || br.y - tl.y < 8)
                    return false;

                out.left = tl.x;
                out.top = tl.y;
                out.right = br.x;
                out.bottom = br.y;
                return true;
            }

            bool s_cursor_clipped = false;

            void release_cursor_clip() {
                if (!s_cursor_clipped)
                    return;
                ClipCursor(nullptr);
                s_cursor_clipped = false;
            }

            void lock_cursor_to_game() {
                RECT r{};
                if (!game_client_screen_rect(r)) {
                    release_cursor_clip();
                    return;
                }
                ClipCursor(&r);
                s_cursor_clipped = true;

                POINT p{};
                if (!GetCursorPos(&p))
                    return;
                const LONG x = std::clamp(p.x, r.left, r.right - 1);
                const LONG y = std::clamp(p.y, r.top, r.bottom - 1);
                if (x != p.x || y != p.y)
                    SetCursorPos(x, y);
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
                std::uint64_t local_team_folder = 0;
            };

            bool is_local_entry(const PlayerCache& cache, const Scene& sc) {
                if (sc.local_player && cache.address == sc.local_player) return true;
                if (sc.local_char && cache.address == sc.local_char) return true;
                return false;
            }

            bool is_teammate(const PlayerCache& cache, const Scene& sc) {
                if (!g_Settings.misc.teamcheck) return false;
                return PlayerHandler::IsTeammate(cache, sc.local_team_folder);
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
                    if (g_Memory.IsValid(s.local_player)) {
                        s.local_char = g_Memory.Read<std::uint64_t>(
                            s.local_player + Offsets::Player::ModelInstance);
                        if (g_Settings.misc.teamcheck)
                            s.local_team_folder = PlayerHandler::ResolveTeamFolder(s.local_char);
                    }
                }
                s.ok = true;
                return s;
            }

            bool collect_parts(const PlayerCache& cache, const Config& cfg, const Scene& sc,
                               const Vector2& anchor_v, PartSample* out, int& out_n) {
                out_n = 0;
                bool configured = false;
                for (int i = 0; i < Settings::AIM_PART_COUNT; ++i) {
                    if (part_tier_of(cfg, i) != Settings::PART_OFF) {
                        configured = true;
                        break;
                    }
                }

                for (int part = 0; part < Settings::AIM_PART_COUNT; ++part) {
                    int tier = part_tier_of(cfg, part);
                    if (!configured)
                        tier = (part == Settings::AIM_HEAD) ? Settings::PART_PRIMARY : Settings::PART_OFF;
                    if (tier == Settings::PART_OFF) continue;

                    const Instance* p = part_instance(cache, part);
                    if (!p || !g_Memory.IsValid(p->address)) continue;

                    BasePart bp(p->address);
                    const Vector3 world = bp.GetPosition();
                    if (cfg.distance_check) {
                        if ((world - sc.cam_pos).Length() > cfg.max_distance) continue;
                    }

                    Vector2 screen{};
                    if (!world_to_screen(sc.view, sc.viewport, world, screen)) continue;

                    const float dx = screen.x - anchor_v.x;
                    const float dy = screen.y - anchor_v.y;
                    PartSample& s = out[out_n++];
                    s.part = part;
                    s.tier = tier;
                    s.world = world;
                    s.screen = screen;
                    s.dist = std::sqrt(dx * dx + dy * dy);
                }
                return out_n > 0;
            }

            bool choose_part(const Config& cfg, PartSample* samples, int n,
                             bool allow_reroll, PartSample& out) {
                if (n <= 0) return false;

                int best_tier = 99;
                for (int i = 0; i < n; ++i)
                    if (samples[i].tier > 0 && samples[i].tier < best_tier)
                        best_tier = samples[i].tier;

                PartSample tiered[Settings::AIM_PART_COUNT];
                int tn = 0;
                for (int i = 0; i < n; ++i)
                    if (samples[i].tier == best_tier)
                        tiered[tn++] = samples[i];

                const int idx = pick_among(cfg, tiered, tn, allow_reroll);
                if (idx < 0) return false;
                out = tiered[idx];
                return true;
            }

            bool select_target(const Config& cfg, const Scene& sc,
                               Vector3& out_world, Vector2& out_screen) {
                const ImVec2  anchor = fov_anchor(cfg);
                const Vector2 anchor_v(anchor.x, anchor.y);
                const float   fov_r  = cfg.fov_enabled ? (std::max)(1.0f, cfg.fov_size) : 1e9f;
                const float   sticky_r = fov_r * (std::max)(1.0f, cfg.sticky_fov_scale);

                bool          found_sticky = false;
                PartSample    sticky_pick{};
                bool          found_best = false;
                float         best_dist  = FLT_MAX;
                std::uint64_t best_addr  = 0;
                PartSample    best_pick{};

                if (s_target && (s_target == sc.local_player || s_target == sc.local_char))
                    s_target = 0;
                if (s_pending && (s_pending == sc.local_player || s_pending == sc.local_char))
                    s_pending = 0;

                PlayerHandler::ForEachPlayer([&](const PlayerCache& cache) {
                    if (is_local_entry(cache, sc)) return;
                    if (is_teammate(cache, sc)) return;

                    PartSample samples[Settings::AIM_PART_COUNT];
                    int sn = 0;
                    if (!collect_parts(cache, cfg, sc, anchor_v, samples, sn)) return;

                    PartSample pick{};
                    if (!choose_part(cfg, samples, sn, cache.address == s_target, pick))
                        return;

                    if (cfg.sticky && s_target != 0 && cache.address == s_target &&
                        pick.dist <= sticky_r) {
                        found_sticky = true;
                        sticky_pick = pick;
                    }

                    if (pick.dist > fov_r) return;
                    if (pick.dist < best_dist) {
                        best_dist = pick.dist;
                        best_addr = cache.address;
                        best_pick = pick;
                        found_best = true;
                    }
                });

                auto commit = [&](std::uint64_t addr, const PartSample& pick) -> bool {
                    if (!hitchance_ok(cfg)) return false;
                    s_target = addr;
                    s_pending = 0;
                    out_world = pick.world;
                    out_screen = pick.screen;
                    s_aim_part = pick.part;
                    s_aim_point = pick.world;
                    return true;
                };

                if (cfg.sticky && found_sticky)
                    return commit(s_target, sticky_pick);

                if (!found_best) {
                    s_pending = 0;
                    s_target  = 0;
                    return false;
                }

                if (!cfg.sticky)
                    return commit(best_addr, best_pick);

                const double now = ImGui::GetTime();
                if (cfg.humanize && cfg.reaction_ms > 0.0f && best_addr != s_target) {
                    if (best_addr != s_pending) {
                        s_pending   = best_addr;
                        s_engage_at = now + cfg.reaction_ms / 1000.0;
                        return false;
                    }
                    if (now < s_engage_at) return false;
                }

                return commit(best_addr, best_pick);
            }

            void apply_mouse(const Config& cfg, const Vector2& target_screen) {
                lock_cursor_to_game();

                const ImVec2 cur = fov_anchor(cfg);

                const float sx = (std::max)(0.1f, cfg.smooth_x);
                const float sy = (std::max)(0.1f, cfg.smooth_y);
                float dx = (target_screen.x - cur.x) / sx;
                float dy = (target_screen.y - cur.y) / sy;

                constexpr float k_max_step = 80.0f;
                dx = std::clamp(dx, -k_max_step, k_max_step);
                dy = std::clamp(dy, -k_max_step, k_max_step);

                int idx = static_cast<int>(std::lround(dx));
                int idy = static_cast<int>(std::lround(dy));
                if (idx == 0 && idy == 0) return;

                POINT screen{};
                RECT bounds{};
                if (GetCursorPos(&screen) && game_client_screen_rect(bounds)) {
                    constexpr LONG k_pad = 2;
                    const LONG left = bounds.left + k_pad;
                    const LONG top = bounds.top + k_pad;
                    const LONG right = (std::max)(left, bounds.right - k_pad - 1);
                    const LONG bottom = (std::max)(top, bounds.bottom - k_pad - 1);

                    const LONG nx = std::clamp(screen.x + idx, left, right);
                    const LONG ny = std::clamp(screen.y + idy, top, bottom);
                    idx = static_cast<int>(nx - screen.x);
                    idy = static_cast<int>(ny - screen.y);
                    if (idx == 0 && idy == 0) return;
                }

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
                release_cursor_clip();
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
                release_cursor_clip();
                return;
            }

            const Scene sc = resolve_scene();
            if (!sc.ok) {
                s_target = 0;
                RaycastSilent::SetActive(false);
                release_cursor_clip();
                return;
            }

            Vector3 world{};
            Vector2 screen{};
            if (!select_target(cfg, sc, world, screen)) {
                s_target = 0;
                RaycastSilent::SetActive(false);
                release_cursor_clip();
                return;
            }

            if (g_Settings.aim.type == 0) {
                RaycastSilent::SetActive(false);
                apply_mouse(cfg, screen);
            } else if (g_Settings.aim.type == 1) {
                release_cursor_clip();
                RaycastSilent::SetActive(false);
                apply_camera(cfg, sc, world);
            } else if (g_Settings.aim.silent_method == Settings::SILENT_RAYCAST) {
                release_cursor_clip();

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
                release_cursor_clip();
                MagicBullet::SetActive(true, world);
            } else {
                release_cursor_clip();
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
