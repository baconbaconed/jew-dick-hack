#include "ESP.h"
#include "ShaderChams.h"
#include "../Aim/Aim.h"
#include "../../Globals/Globals.h"
#include "../../Roblox/Math/Math.h"
#include "../../Memory/Memory.h"
#include "../../Roblox/Engine/Offsets/Offsets.h"
#include "../../../Settings.h"
#include "../../../Renderer/Renderer.h"
#include "../../../GUI/resources/fonts/fonts.h"
#include <iostream>
#include <algorithm>
#include <vector>
#include <cmath>
#include <cstdio>

static void DrawTextWithOutline(ImDrawList* draw_list, ImFont* font, float font_size,
                                ImVec2 pos, ImU32 color, const char* text)
{
    ImU32 shadow = IM_COL32(0, 0, 0, 255);
    font_size = fonts::snap_px(font_size);
    float x = std::floor(pos.x);
    float y = std::floor(pos.y);

    for (int i = -1; i <= 1; i++) {
        for (int j = -1; j <= 1; j++) {
            if (i == 0 && j == 0) continue;
            draw_list->AddText(font, font_size, ImVec2(x + i, y + j), shadow, text);
        }
    }
    draw_list->AddText(font, font_size, ImVec2(x, y), color, text);
}

static void SnapEspBox(float min_x, float min_y, float max_x, float max_y,
                       float& x1, float& y1, float& x2, float& y2)
{
    x1 = std::floor(min_x);
    y1 = std::floor(min_y);
    x2 = std::ceil(max_x);
    y2 = std::ceil(max_y);
    if (x2 <= x1) x2 = x1 + 1.0f;
    if (y2 <= y1) y2 = y1 + 1.0f;
}

static void DrawBox(ImDrawList* draw_list, ImVec2 top_left, ImVec2 bottom_right, ImU32 color)
{
    float x1, y1, x2, y2;
    SnapEspBox(top_left.x, top_left.y, bottom_right.x, bottom_right.y, x1, y1, x2, y2);

    draw_list->AddRect(ImVec2(x1 - 1, y1 - 1), ImVec2(x2 + 1, y2 + 1), IM_COL32(0, 0, 0, 255), 0.0f, 0, 1.0f);

    draw_list->AddRect(ImVec2(x1 + 1, y1 + 1), ImVec2(x2 - 1, y2 - 1), IM_COL32(0, 0, 0, 255), 0.0f, 0, 1.0f);

    draw_list->AddRect(ImVec2(x1, y1), ImVec2(x2, y2), color, 0.0f, 0, 1.0f);
}

static void DrawCornerBox(ImDrawList* draw_list, ImVec2 top_left, ImVec2 bottom_right, ImU32 color)
{
    float x1, y1, x2, y2;
    SnapEspBox(top_left.x, top_left.y, bottom_right.x, bottom_right.y, x1, y1, x2, y2);
    const float lw = (std::max)(2.0f, std::floor((x2 - x1) * 0.25f));
    const float lh = (std::max)(2.0f, std::floor((y2 - y1) * 0.25f));

    struct Seg { ImVec2 a, b; };
    const Seg segs[8] = {

        { {x1, y1}, {x1 + lw, y1} }, { {x1, y1}, {x1, y1 + lh} },

        { {x2 - lw, y1}, {x2, y1} }, { {x2, y1}, {x2, y1 + lh} },

        { {x1, y2 - lh}, {x1, y2} }, { {x1, y2}, {x1 + lw, y2} },

        { {x2, y2 - lh}, {x2, y2} }, { {x2 - lw, y2}, {x2, y2} },
    };
    const ImU32 black = IM_COL32(0, 0, 0, 255);
    for (const auto& s : segs) draw_list->AddLine(s.a, s.b, black, 3.0f);
    for (const auto& s : segs) draw_list->AddLine(s.a, s.b, color, 1.0f);
}

static void DrawSkeletonLine(ImDrawList* draw_list, ImVec2 a, ImVec2 b, ImU32 color)
{

    draw_list->AddLine(a, b, IM_COL32(0, 0, 0, 255), 3.0f);
    draw_list->AddLine(a, b, color, 1.0f);
}

static std::vector<ImVec2> ConvexHull(std::vector<ImVec2> pts)
{
    if (pts.size() < 3) return pts;

    std::sort(pts.begin(), pts.end(), [](const ImVec2& a, const ImVec2& b) {
        return a.x < b.x || (a.x == b.x && a.y < b.y);
    });

    auto cross = [](const ImVec2& o, const ImVec2& a, const ImVec2& b) {
        return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
    };

    std::vector<ImVec2> hull(pts.size() * 2);
    int k = 0;
    for (size_t i = 0; i < pts.size(); ++i) {
        while (k >= 2 && cross(hull[k-2], hull[k-1], pts[i]) <= 0.0f) k--;
        hull[k++] = pts[i];
    }
    for (int i = (int)pts.size() - 2, t = k + 1; i >= 0; --i) {
        while (k >= t && cross(hull[k-2], hull[k-1], pts[i]) <= 0.0f) k--;
        hull[k++] = pts[i];
    }
    hull.resize(k > 0 ? k - 1 : 0);
    return hull;
}

static constexpr int k_box_edges[12][2] = {
    {0,1},{0,2},{0,4},{1,3},{1,5},{2,3},
    {2,6},{3,7},{4,5},{4,6},{5,7},{6,7}
};

static bool SegInsidePoly(const ImVec2& a, const ImVec2& b,
                          const std::vector<ImVec2>& poly,
                          float& out_t0, float& out_t1)
{
    const int n = (int)poly.size();
    if (n < 3) return false;

    ImVec2 c(0, 0);
    for (const auto& p : poly) { c.x += p.x; c.y += p.y; }
    c.x /= n; c.y /= n;

    float t0 = 0.0f, t1 = 1.0f;
    constexpr float eps = 0.25f;

    for (int i = 0; i < n; ++i) {
        const ImVec2& p = poly[i];
        const ImVec2& q = poly[(i + 1) % n];
        const float ex = q.x - p.x, ey = q.y - p.y;

        auto side = [&](const ImVec2& v) { return ex * (v.y - p.y) - ey * (v.x - p.x); };
        const float s  = side(c) >= 0.0f ? 1.0f : -1.0f;

        const float f0 = s * side(a);
        const float f1 = s * side(b);
        const float df = f1 - f0;

        if (fabsf(df) < 1e-6f) {
            if (f0 < -eps) return false;
            continue;
        }
        const float tc = (-eps - f0) / df;
        if (df > 0.0f) { if (tc > t0) t0 = tc; }
        else           { if (tc < t1) t1 = tc; }
        if (t0 >= t1) return false;
    }

    out_t0 = t0 < 0.0f ? 0.0f : t0;
    out_t1 = t1 > 1.0f ? 1.0f : t1;
    return out_t1 > out_t0;
}

static std::vector<ImVec2> ClipHalfPlane(const std::vector<ImVec2>& poly,
                                         const ImVec2& p, const ImVec2& q, float s)
{
    std::vector<ImVec2> out;
    const int n = (int)poly.size();
    if (n < 3) return out;
    out.reserve(n + 2);

    auto f = [&](const ImVec2& v) {
        return s * ((q.x - p.x) * (v.y - p.y) - (q.y - p.y) * (v.x - p.x));
    };

    for (int i = 0; i < n; ++i) {
        const ImVec2& a = poly[i];
        const ImVec2& b = poly[(i + 1) % n];
        const float fa = f(a), fb = f(b);
        if (fa >= 0.0f) out.push_back(a);
        if ((fa < 0.0f) != (fb < 0.0f)) {
            const float t = fa / (fa - fb);
            out.push_back(ImVec2(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t));
        }
    }
    if (out.size() < 3) out.clear();
    return out;
}

static void SubtractPoly(std::vector<ImVec2> piece, const std::vector<ImVec2>& B,
                         std::vector<std::vector<ImVec2>>& out)
{
    const int n = (int)B.size();
    if (n < 3) { if (piece.size() >= 3) out.push_back(std::move(piece)); return; }

    ImVec2 c(0, 0);
    for (const auto& v : B) { c.x += v.x; c.y += v.y; }
    c.x /= n; c.y /= n;

    for (int i = 0; i < n && piece.size() >= 3; ++i) {
        const ImVec2& p = B[i];
        const ImVec2& q = B[(i + 1) % n];
        const float cs = (q.x - p.x) * (c.y - p.y) - (q.y - p.y) * (c.x - p.x);
        const float s  = cs >= 0.0f ? 1.0f : -1.0f;

        auto outside = ClipHalfPlane(piece, p, q, -s);
        if (!outside.empty()) out.push_back(std::move(outside));
        piece = ClipHalfPlane(piece, p, q, s);
    }

}

static void DrawSegmentOutsideUnion(ImDrawList* dl, const ImVec2& a, const ImVec2& b,
                                    const std::vector<std::vector<ImVec2>>& polys,
                                    int skip, ImU32 color)
{

    std::vector<std::pair<float, float>> covered;
    for (int i = 0; i < (int)polys.size(); ++i) {
        if (i == skip) continue;
        float t0, t1;
        if (SegInsidePoly(a, b, polys[i], t0, t1))
            covered.emplace_back(t0, t1);
    }

    auto lerp_pt = [&](float t) { return ImVec2(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t); };

    if (covered.empty()) {
        dl->AddLine(a, b, color, 1.0f);
        return;
    }

    std::sort(covered.begin(), covered.end());

    constexpr float min_piece = 0.002f;
    float cursor = 0.0f;
    for (const auto& iv : covered) {
        if (iv.first > cursor + min_piece)
            dl->AddLine(lerp_pt(cursor), lerp_pt(iv.first), color, 1.0f);
        if (iv.second > cursor) cursor = iv.second;
        if (cursor >= 1.0f) break;
    }
    if (cursor < 1.0f - min_piece)
        dl->AddLine(lerp_pt(cursor), lerp_pt(1.0f), color, 1.0f);
}

static float g_esp_scale_x = 1.0f;
static float g_esp_scale_y = 1.0f;

bool WorldToScreen(const Matrix4x4& matrix, const Vector2& dimensions, const Vector3& position, Vector2& screen)
{
    float w = position.x * matrix.m[3][0] + position.y * matrix.m[3][1] + position.z * matrix.m[3][2] + matrix.m[3][3];
    if (w < 0.01f) return false;

    float x = position.x * matrix.m[0][0] + position.y * matrix.m[0][1] + position.z * matrix.m[0][2] + matrix.m[0][3];
    float y = position.x * matrix.m[1][0] + position.y * matrix.m[1][1] + position.z * matrix.m[1][2] + matrix.m[1][3];

    float invw = 1.0f / w;
    x *= invw; y *= invw;

    screen.x = ((dimensions.x / 2) + (x * dimensions.x / 2)) * g_esp_scale_x;
    screen.y = ((dimensions.y / 2) - (y * dimensions.y / 2)) * g_esp_scale_y;
    return true;
}

void Cheat::Visuals::ESP::Render()
{
    if (!Cheat::g_Settings.esp.enabled) return;
    if (!Cheat::Globals::Workspace || !Cheat::Globals::InstanceDataModel.address) return;

    auto camera_ptr = Cheat::Globals::Workspace->GetCurrentCamera();
    if (!camera_ptr) return;

    Camera camera(camera_ptr->address);
    Vector2 viewport = camera.GetViewportSize();
    const Vector3 cam_pos = camera.GetPosition();
    auto draw_list = ImGui::GetBackgroundDrawList();

    g_esp_scale_x = 1.0f;
    g_esp_scale_y = 1.0f;
    float overlay_w = viewport.x, overlay_h = viewport.y;
    if (HWND oh = Renderer::GetHwnd()) {
        RECT ocr{};
        if (GetClientRect(oh, &ocr) && viewport.x > 1.0f && viewport.y > 1.0f) {
            overlay_w = (float)(ocr.right - ocr.left);
            overlay_h = (float)(ocr.bottom - ocr.top);
            g_esp_scale_x = overlay_w / viewport.x;
            g_esp_scale_y = overlay_h / viewport.y;
        }
    }

    ImFont* esp_font = (Cheat::g_Settings.esp.font == 1)
        ? (fonts::tahoma_bold ? fonts::tahoma_bold : fonts::tahoma)
        : (fonts::tahoma ? fonts::tahoma : fonts::tahoma_bold);
    if (!esp_font) esp_font = ImGui::GetFont();

    const float esp_fs = fonts::snap_px(Cheat::g_Settings.esp.font_size);

    ImDrawListFlags backup_flags = draw_list->Flags;
    draw_list->Flags &= ~ImDrawListFlags_AntiAliasedLines;

    static const uintptr_t s_module_base = g_Memory.GetModuleBase();
    uintptr_t visual_engine = g_Memory.Read<uintptr_t>(s_module_base + Offsets::VisualEngine::Pointer);
    Matrix4x4 vm = g_Memory.Read<Matrix4x4>(visual_engine + Offsets::VisualEngine::ViewMatrix);

    std::uint64_t local_player_addr = 0;
    if (Cheat::Globals::Players && g_Memory.IsValid(Cheat::Globals::Players->address))
        local_player_addr = g_Memory.Read<std::uint64_t>(
            Cheat::Globals::Players->address + Offsets::Player::LocalPlayer);

    PlayerHandler::ForEachPlayer([&](const PlayerCache& cache)
    {
        if (!cache.humanoidRootPart || !cache.head) return;

        if (!Cheat::g_Settings.esp.draw_local && cache.address == local_player_addr)
            return;

        BasePart root(cache.humanoidRootPart->address);
        const Vector3 root_pos = root.GetPosition();
        const float dist_studs = cam_pos.DistanceTo(root_pos);

        if (Cheat::g_Settings.esp.distance_check &&
            dist_studs > Cheat::g_Settings.esp.max_distance)
            return;

        {
            Vector2 root_screen;
            if (!WorldToScreen(vm, viewport, root_pos, root_screen))
                return;
            constexpr float kCullMargin = 400.0f;
            if (root_screen.x < -kCullMargin || root_screen.x > overlay_w + kCullMargin ||
                root_screen.y < -kCullMargin || root_screen.y > overlay_h + kCullMargin)
                return;
        }

        std::vector<std::shared_ptr<Instance>> parts = {
            cache.head, cache.humanoidRootPart, cache.upperTorso, cache.lowerTorso,
            cache.leftUpperArm, cache.rightUpperArm, cache.leftLowerArm, cache.rightLowerArm,
            cache.leftUpperLeg, cache.rightUpperLeg, cache.leftLowerLeg, cache.rightLowerLeg,
            cache.leftFoot, cache.rightFoot, cache.leftHand, cache.rightHand
        };

        float min_x = 10000.0f, max_x = -10000.0f;
        float min_y = 10000.0f, max_y = -10000.0f;
        bool any_visible = false;

        Vector3 wmin{ 1e9f, 1e9f, 1e9f }, wmax{ -1e9f, -1e9f, -1e9f };

        struct PartCorners { ImVec2 pt[8]; bool full; };
        std::vector<PartCorners> chams_parts;
        const bool want_chams = Cheat::g_Settings.esp.chams;

        struct PartData { Vector3 pos; Vector3 sz; Matrix4x4 rot; const Instance* inst; };
        std::vector<PartData> part_data;
        part_data.reserve(parts.size());

        for (auto const& part : parts)
        {
            if (!part) continue;

            BasePart bp(part->address);
            Vector3 pos = bp.GetPosition();
            Vector3 sz = bp.GetSize();
            Matrix4x4 rot = bp.GetRotation();

            part_data.push_back({ pos, sz, rot, part.get() });

            const bool is_hrp = (cache.humanoidRootPart &&
                                 part.get() == cache.humanoidRootPart.get());

            Vector3 half = { sz.x / 2.0f, sz.y / 2.0f, sz.z / 2.0f };

            Vector3 corners[8] = {
                { -half.x, -half.y, -half.z }, { -half.x, -half.y,  half.z },
                { -half.x,  half.y, -half.z }, { -half.x,  half.y,  half.z },
                {  half.x, -half.y, -half.z }, {  half.x, -half.y,  half.z },
                {  half.x,  half.y, -half.z }, {  half.x,  half.y,  half.z }
            };

            PartCorners pc{}; pc.full = true;
            int idx = 0;

            for (auto& corner : corners)
            {
                Vector3 world_corner = {
                    pos.x + (rot.m[0][0] * corner.x + rot.m[0][1] * corner.y + rot.m[0][2] * corner.z),
                    pos.y + (rot.m[1][0] * corner.x + rot.m[1][1] * corner.y + rot.m[1][2] * corner.z),
                    pos.z + (rot.m[2][0] * corner.x + rot.m[2][1] * corner.y + rot.m[2][2] * corner.z)
                };

                wmin.x = (std::min)(wmin.x, world_corner.x); wmax.x = (std::max)(wmax.x, world_corner.x);
                wmin.y = (std::min)(wmin.y, world_corner.y); wmax.y = (std::max)(wmax.y, world_corner.y);
                wmin.z = (std::min)(wmin.z, world_corner.z); wmax.z = (std::max)(wmax.z, world_corner.z);

                Vector2 screen_pos;
                if (WorldToScreen(vm, viewport, world_corner, screen_pos)) {
                    min_x = (std::min)(min_x, screen_pos.x);
                    max_x = (std::max)(max_x, screen_pos.x);
                    min_y = (std::min)(min_y, screen_pos.y);
                    max_y = (std::max)(max_y, screen_pos.y);
                    any_visible = true;
                    pc.pt[idx] = ImVec2(screen_pos.x, screen_pos.y);
                } else {
                    pc.full = false;
                }
                ++idx;
            }

            if (want_chams && pc.full && !is_hrp)
                chams_parts.push_back(pc);
        }

        if (!any_visible || min_x > 5000.0f) return;

        const bool aim_target =
            Cheat::Features::Aim::CurrentTarget() != 0 &&
            cache.address == Cheat::Features::Aim::CurrentTarget();
        constexpr ImU32 k_aim_red      = IM_COL32(255, 45, 45, 255);
        constexpr ImU32 k_aim_red_fill = IM_COL32(255, 45, 45, 110);

        if (want_chams && !chams_parts.empty()) {
            const auto& oc = Cheat::g_Settings.esp.chams_outline_color;
            const auto& fc = Cheat::g_Settings.esp.chams_fill_color;
            const ImU32 outline_col = aim_target ? k_aim_red : IM_COL32(
                (int)(oc[0]*255),(int)(oc[1]*255),(int)(oc[2]*255),(int)(oc[3]*255));
            const ImU32 fill_col = aim_target ? k_aim_red_fill : IM_COL32(
                (int)(fc[0]*255),(int)(fc[1]*255),(int)(fc[2]*255),(int)(fc[3]*255));
            const int mode = Cheat::g_Settings.esp.chams_mode;

            if (mode == 0) {

                for (const auto& pc : chams_parts)
                    for (const auto& e : k_box_edges)
                        draw_list->AddLine(pc.pt[e[0]], pc.pt[e[1]], outline_col, 1.0f);
            }
            else if (mode == 1) {

                for (const auto& pc : chams_parts) {
                    std::vector<ImVec2> pts(pc.pt, pc.pt + 8);
                    auto hull = ConvexHull(pts);
                    if (hull.size() >= 3)
                        draw_list->AddConvexPolyFilled(hull.data(), (int)hull.size(), fill_col);
                }
                for (const auto& pc : chams_parts)
                    for (const auto& e : k_box_edges)
                        draw_list->AddLine(pc.pt[e[0]], pc.pt[e[1]], outline_col, 1.0f);
            }
            else {

                std::vector<std::vector<ImVec2>> hulls;
                hulls.reserve(chams_parts.size());
                for (const auto& pc : chams_parts) {
                    std::vector<ImVec2> pts(pc.pt, pc.pt + 8);
                    hulls.push_back(ConvexHull(pts));
                }

                std::vector<std::vector<ImVec2>> clipped;
                clipped.reserve(hulls.size() * 2);
                for (int i = 0; i < (int)hulls.size(); ++i) {
                    if (hulls[i].size() < 3) continue;

                    std::vector<std::vector<ImVec2>> pieces{ hulls[i] };
                    for (int j = 0; j < i && !pieces.empty(); ++j) {
                        if (hulls[j].size() < 3) continue;
                        std::vector<std::vector<ImVec2>> next;
                        for (auto& piece : pieces)
                            SubtractPoly(std::move(piece), hulls[j], next);
                        pieces = std::move(next);
                    }
                    for (auto& piece : pieces)
                        if (piece.size() >= 3)
                            clipped.push_back(std::move(piece));
                }

                if (mode == 3) {

                    const int shader = Cheat::g_Settings.esp.chams_shader;
                    ShaderChams::DrawFill(draw_list, clipped, (float)ImGui::GetTime(),
                                          shader, aim_target);
                    const ImU32 shader_outline = aim_target
                        ? k_aim_red
                        : ShaderChams::OutlineColor(shader, false);
                    for (int i = 0; i < (int)hulls.size(); ++i) {
                        const auto& hull = hulls[i];
                        const int n = (int)hull.size();
                        if (n < 2) continue;
                        for (int e = 0; e < n; ++e)
                            DrawSegmentOutsideUnion(draw_list,
                                hull[e], hull[(e + 1) % n],
                                hulls, i, shader_outline);
                    }
                } else {
                    ImDrawListFlags fill_backup = draw_list->Flags;
                    draw_list->Flags &= ~ImDrawListFlags_AntiAliasedFill;

                    for (const auto& piece : clipped)
                        draw_list->AddConvexPolyFilled(piece.data(), (int)piece.size(), fill_col);

                    draw_list->Flags = fill_backup;

                    for (int i = 0; i < (int)hulls.size(); ++i) {
                        const auto& hull = hulls[i];
                        const int n = (int)hull.size();
                        if (n < 2) continue;
                        for (int e = 0; e < n; ++e)
                            DrawSegmentOutsideUnion(draw_list,
                                hull[e], hull[(e + 1) % n],
                                hulls, i, outline_col);
                    }
                }
            }
        }

        const bool want_health = Cheat::g_Settings.esp.healthbar ||
                                 Cheat::g_Settings.esp.health_text ||
                                 Cheat::g_Settings.esp.flags;
        float hp = 0.0f, max_hp = 100.0f;
        if (want_health && cache.humanoid) {
            Humanoid hum(cache.humanoid->address);
            hp = hum.GetHealth();
            max_hp = hum.GetMaxHealth();
            if (max_hp <= 0.0f) max_hp = 100.0f;
        }
        const float hp_frac = hp <= 0.0f ? 0.0f : (hp >= max_hp ? 1.0f : hp / max_hp);

        float bx1, by1, bx2, by2;
        SnapEspBox(min_x, min_y, max_x, max_y, bx1, by1, bx2, by2);
        const float bcx = std::floor((bx1 + bx2) * 0.5f);

        if (Cheat::g_Settings.esp.box) {
            const auto& bc = Cheat::g_Settings.esp.box_color;
            ImU32 box_color = aim_target ? k_aim_red : IM_COL32(
                (int)(bc[0] * 255), (int)(bc[1] * 255), (int)(bc[2] * 255), (int)(bc[3] * 255));
            const int box_mode = Cheat::g_Settings.esp.box_mode;

            bool drew_3d = false;
            if (box_mode == 2) {

                ImVec2 pts[8];
                bool all_ok = true;
                for (int i = 0; i < 8 && all_ok; ++i) {
                    const Vector3 c = {
                        (i & 4) ? wmax.x : wmin.x,
                        (i & 2) ? wmax.y : wmin.y,
                        (i & 1) ? wmax.z : wmin.z
                    };
                    Vector2 sp;
                    if (WorldToScreen(vm, viewport, c, sp))
                        pts[i] = ImVec2(sp.x, sp.y);
                    else
                        all_ok = false;
                }
                if (all_ok) {
                    const ImU32 black = IM_COL32(0, 0, 0, 255);
                    for (const auto& e : k_box_edges)
                        draw_list->AddLine(pts[e[0]], pts[e[1]], black, 3.0f);
                    for (const auto& e : k_box_edges)
                        draw_list->AddLine(pts[e[0]], pts[e[1]], box_color, 1.0f);
                    drew_3d = true;
                }
            }

            if (box_mode == 1)
                DrawCornerBox(draw_list, ImVec2(bx1, by1), ImVec2(bx2, by2), box_color);
            else if (!drew_3d && box_mode != 1)
                DrawBox(draw_list, ImVec2(bx1, by1), ImVec2(bx2, by2), box_color);
        }

        if (Cheat::g_Settings.esp.healthbar) {

            const float bar_x2 = bx1 - 3.0f;
            const float bar_x1 = bar_x2 - 2.0f;
            const float bar_h = by2 - by1;
            const float fill_h = std::floor(bar_h * hp_frac + 0.5f);
            const float fill_top = by2 - fill_h;

            draw_list->AddRectFilled(ImVec2(bar_x1 - 1.0f, by1 - 1.0f),
                                     ImVec2(bar_x2 + 1.0f, by2 + 1.0f),
                                     IM_COL32(0, 0, 0, 255));

            const ImU32 hp_col = IM_COL32((int)(255 * (1.0f - hp_frac)), (int)(255 * hp_frac), 0, 255);
            if (fill_h > 0.0f)
                draw_list->AddRectFilled(ImVec2(bar_x1, fill_top), ImVec2(bar_x2, by2), hp_col);

            if (Cheat::g_Settings.esp.health_text && hp_frac < 1.0f) {
                char hp_buf[16];
                std::snprintf(hp_buf, sizeof(hp_buf), "%.0f", hp);
                const ImVec2 tsz = esp_font->CalcTextSizeA(esp_fs, FLT_MAX, 0.0f, hp_buf);
                DrawTextWithOutline(draw_list, esp_font, esp_fs,
                    ImVec2(std::floor(bar_x1 - tsz.x - 2.0f),
                           std::floor(fill_top - tsz.y * 0.5f)),
                    IM_COL32(255, 255, 255, 255), hp_buf);
            }
        } else if (Cheat::g_Settings.esp.health_text) {

            char hp_buf[16];
            std::snprintf(hp_buf, sizeof(hp_buf), "%.0f hp", hp);
            const ImVec2 tsz = esp_font->CalcTextSizeA(esp_fs, FLT_MAX, 0.0f, hp_buf);
            DrawTextWithOutline(draw_list, esp_font, esp_fs,
                ImVec2(std::floor(bx1 - tsz.x - 4.0f), by1),
                IM_COL32(255, 255, 255, 255), hp_buf);
        }

        if (Cheat::g_Settings.esp.name) {
            const std::string& shown = (Cheat::g_Settings.esp.name_mode == 0 && !cache.displayName.empty())
                ? cache.displayName
                : cache.name;
            const char* name = shown.c_str();
            const float text_width = esp_font->CalcTextSizeA(esp_fs, FLT_MAX, 0.0f, name).x;
            const auto& nc = Cheat::g_Settings.esp.name_color;
            const ImU32 name_col = aim_target ? k_aim_red : IM_COL32(
                (int)(nc[0]*255),(int)(nc[1]*255),(int)(nc[2]*255),(int)(nc[3]*255));
            DrawTextWithOutline(draw_list, esp_font, esp_fs,
                               ImVec2(std::floor(bcx - text_width * 0.5f), by1 - esp_fs - 2.0f),
                               name_col,
                               name);
        }

        float bottom_y = by2 + 2.0f;

        if (Cheat::g_Settings.esp.distance) {
            char dist_buf[32];
            if (Cheat::g_Settings.esp.distance_unit == 1)
                std::snprintf(dist_buf, sizeof(dist_buf), "%.0fm", dist_studs * 0.28f);
            else
                std::snprintf(dist_buf, sizeof(dist_buf), "%.0f studs", dist_studs);

            const auto& dc = Cheat::g_Settings.esp.distance_color;
            const ImVec2 tsz = esp_font->CalcTextSizeA(esp_fs, FLT_MAX, 0.0f, dist_buf);
            DrawTextWithOutline(draw_list, esp_font, esp_fs,
                ImVec2(std::floor(bcx - tsz.x * 0.5f), std::floor(bottom_y)),
                IM_COL32((int)(dc[0]*255),(int)(dc[1]*255),(int)(dc[2]*255),(int)(dc[3]*255)),
                dist_buf);
            bottom_y += tsz.y + 2.0f;
        }

        if (Cheat::g_Settings.esp.tool && !cache.toolName.empty()) {
            char tool_buf[128];
            std::snprintf(tool_buf, sizeof(tool_buf), "[%s]", cache.toolName.c_str());

            const auto& tc = Cheat::g_Settings.esp.tool_color;
            const ImVec2 tsz = esp_font->CalcTextSizeA(esp_fs, FLT_MAX, 0.0f, tool_buf);
            DrawTextWithOutline(draw_list, esp_font, esp_fs,
                ImVec2(std::floor(bcx - tsz.x * 0.5f), std::floor(bottom_y)),
                IM_COL32((int)(tc[0]*255),(int)(tc[1]*255),(int)(tc[2]*255),(int)(tc[3]*255)),
                tool_buf);
            bottom_y += tsz.y + 2.0f;
        }

        if (Cheat::g_Settings.esp.flags && cache.humanoid) {
            struct Flag { char text[24]; ImU32 color; };
            Flag flag_list[6];
            int flag_count = 0;
            auto add_flag = [&](const char* t, ImU32 c) {
                if (flag_count >= 6) return;
                std::snprintf(flag_list[flag_count].text, sizeof(flag_list[flag_count].text), "%s", t);
                flag_list[flag_count].color = c;
                ++flag_count;
            };

            Humanoid hum(cache.humanoid->address);
            const std::int32_t state = hum.GetStateId();
            const bool dead = (state == 15) || (want_health && hp <= 0.0f);

            if (dead) {
                add_flag("dead", IM_COL32(237, 66, 69, 255));
            } else {
                switch (state) {
                case 13: add_flag("sitting",  IM_COL32(255, 170, 60, 255));  break;
                case 3:  add_flag("jumping",  IM_COL32(90, 200, 245, 255));  break;
                case 5:  add_flag("falling",  IM_COL32(90, 200, 245, 255));  break;
                case 4:  add_flag("swimming", IM_COL32(80, 150, 245, 255));  break;
                case 12: add_flag("climbing", IM_COL32(80, 150, 245, 255));  break;
                case 6:  add_flag("flying",   IM_COL32(190, 120, 245, 255)); break;
                case 0:
                case 1:  add_flag("ragdoll",  IM_COL32(255, 170, 60, 255));  break;
                default: {

                    const Vector3 vel = root.GetAssemblyLinearVelocity();
                    const float h_speed = std::sqrt(vel.x * vel.x + vel.z * vel.z);
                    if (h_speed > 1.0f) add_flag("moving",   IM_COL32(120, 220, 120, 255));
                    else                add_flag("standing", IM_COL32(160, 160, 160, 255));
                    break;
                }
                }

                const float walkspeed = hum.GetWalkSpeed();
                if (walkspeed > 16.5f)
                    add_flag("speed", IM_COL32(245, 220, 80, 255));
            }

            float flag_y = by1;
            for (int i = 0; i < flag_count; ++i) {
                DrawTextWithOutline(draw_list, esp_font, esp_fs,
                    ImVec2(std::floor(bx2 + 4.0f), std::floor(flag_y)),
                    flag_list[i].color, flag_list[i].text);
                flag_y += esp_fs + 2.0f;
            }
        }

        if (Cheat::g_Settings.esp.skeleton) {
            const auto& sc = Cheat::g_Settings.esp.skeleton_color;
            const ImU32 skel_color = aim_target ? k_aim_red : IM_COL32(
                (int)(sc[0]*255),(int)(sc[1]*255),(int)(sc[2]*255),(int)(sc[3]*255));

            auto find_part = [&](const std::shared_ptr<Instance>& part) -> const PartData* {
                if (!part) return nullptr;
                for (const auto& pd : part_data)
                    if (pd.inst == part.get()) return &pd;
                return nullptr;
            };

            auto part_pos = [&](const std::shared_ptr<Instance>& part, Vector3& out) -> bool {
                const PartData* pd = find_part(part);
                if (!pd) return false;
                out = pd->pos;
                return true;
            };

            auto part_axis = [&](const std::shared_ptr<Instance>& part, Vector3& top, Vector3& bottom) -> bool {
                const PartData* pd = find_part(part);
                if (!pd) return false;
                const float hy = pd->sz.y / 2.0f;
                const Vector3 up = { pd->rot.m[0][1] * hy, pd->rot.m[1][1] * hy, pd->rot.m[2][1] * hy };
                top    = { pd->pos.x + up.x, pd->pos.y + up.y, pd->pos.z + up.z };
                bottom = { pd->pos.x - up.x, pd->pos.y - up.y, pd->pos.z - up.z };
                return true;
            };

            auto line_w2s = [&](const Vector3& a, const Vector3& b) {
                Vector2 sa, sb;
                if (WorldToScreen(vm, viewport, a, sa) && WorldToScreen(vm, viewport, b, sb))
                    DrawSkeletonLine(draw_list, ImVec2(sa.x, sa.y), ImVec2(sb.x, sb.y), skel_color);
            };

            auto lerp3 = [](const Vector3& a, const Vector3& b, float t) -> Vector3 {
                return { a.x + (b.x - a.x) * t,
                         a.y + (b.y - a.y) * t,
                         a.z + (b.z - a.z) * t };
            };

            auto point_at_height = [&](const Vector3& top, const Vector3& bottom,
                                       const Vector3& ref) -> Vector3 {
                const float dy = top.y - bottom.y;
                if (fabsf(dy) < 0.001f) return lerp3(top, bottom, 0.5f);
                const float t = (top.y - ref.y) / dy;
                return lerp3(top, bottom, t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t));
            };

            if (cache.isR6) {

                constexpr float kShoulderDrop = 0.18f;

                Vector3 torso_top, torso_bot;
                const bool torso_ok = part_axis(cache.upperTorso, torso_top, torso_bot);
                Vector3 shoulder_c{};
                if (torso_ok) {
                    shoulder_c = lerp3(torso_top, torso_bot, kShoulderDrop);
                    line_w2s(shoulder_c, torso_bot);
                }

                Vector3 t, b;
                if (part_axis(cache.leftUpperArm, t, b)) {
                    const Vector3 joint = torso_ok ? point_at_height(t, b, shoulder_c) : t;
                    if (torso_ok) line_w2s(shoulder_c, joint);
                    line_w2s(joint, b);
                }
                if (part_axis(cache.rightUpperArm, t, b)) {
                    const Vector3 joint = torso_ok ? point_at_height(t, b, shoulder_c) : t;
                    if (torso_ok) line_w2s(shoulder_c, joint);
                    line_w2s(joint, b);
                }
                if (part_axis(cache.leftUpperLeg, t, b)) {
                    if (torso_ok) line_w2s(torso_bot, t);
                    line_w2s(t, b);
                }
                if (part_axis(cache.rightUpperLeg, t, b)) {
                    if (torso_ok) line_w2s(torso_bot, t);
                    line_w2s(t, b);
                }
            } else {

                auto bone = [&](const std::shared_ptr<Instance>& pa,
                                const std::shared_ptr<Instance>& pb) {
                    Vector3 a, b;
                    if (part_pos(pa, a) && part_pos(pb, b)) line_w2s(a, b);
                };

                constexpr float kShoulderDrop15 = 0.15f;

                Vector3 ut_top, ut_bot;
                const bool ut_ok = part_axis(cache.upperTorso, ut_top, ut_bot);
                Vector3 shoulder_c{};
                if (ut_ok) shoulder_c = lerp3(ut_top, ut_bot, kShoulderDrop15);

                auto shoulder_bone = [&](const std::shared_ptr<Instance>& arm) {
                    Vector3 a_top, a_bot, a_c;
                    if (ut_ok && part_axis(arm, a_top, a_bot) && part_pos(arm, a_c)) {
                        const Vector3 joint = point_at_height(a_top, a_bot, shoulder_c);
                        line_w2s(shoulder_c, joint);
                        line_w2s(joint, a_c);
                    } else {
                        bone(cache.upperTorso, arm);
                    }
                };

                bone(cache.head,          cache.upperTorso);
                bone(cache.upperTorso,    cache.lowerTorso);

                shoulder_bone(cache.leftUpperArm);
                bone(cache.leftUpperArm,  cache.leftLowerArm);
                bone(cache.leftLowerArm,  cache.leftHand);

                shoulder_bone(cache.rightUpperArm);
                bone(cache.rightUpperArm, cache.rightLowerArm);
                bone(cache.rightLowerArm, cache.rightHand);

                bone(cache.lowerTorso,    cache.leftUpperLeg);
                bone(cache.leftUpperLeg,  cache.leftLowerLeg);
                bone(cache.leftLowerLeg,  cache.leftFoot);

                bone(cache.lowerTorso,    cache.rightUpperLeg);
                bone(cache.rightUpperLeg, cache.rightLowerLeg);
                bone(cache.rightLowerLeg, cache.rightFoot);
            }
        }
    });

    draw_list->Flags = backup_flags;
}
