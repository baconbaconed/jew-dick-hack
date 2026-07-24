#define NOMINMAX
#define IMGUI_DEFINE_MATH_OPERATORS
#include "ESPPreview.h"
#include "PreviewRenderer.h"
#include "ShaderChams.h"
#include "preview_model/preview_model_obj.h"
#include "preview_model/preview_model_texture.h"
#include "../../Graphics.h"
#include "../../../Settings.h"
#include "../../../GUI/colors/colors.h"
#include "../../../GUI/resources/fonts/fonts.h"
#include "../../../GUI/widgets/widgets.h"
#include "../../../GUI/imgui/imgui.h"
#include "../../../GUI/imgui/imgui_internal.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cmath>
#include <cfloat>
#include <vector>

namespace {
    Cheat::Core::PreviewRenderer g_Renderer;
    float g_LastTime = 0.0f;
    bool  g_InitDone = false;

    ImU32 Col(const float c[4])
    {
        return ImGui::ColorConvertFloat4ToU32(ImVec4(c[0], c[1], c[2], c[3]));
    }

    inline float Floor(float v) { return std::floor(v); }

    constexpr int k_box_edges[12][2] = {
        {0,1},{0,2},{0,4},{1,3},{1,5},{2,3},
        {2,6},{3,7},{4,5},{4,6},{5,7},{6,7}
    };

    std::vector<ImVec2> ConvexHull(std::vector<ImVec2> pts)
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
            while (k >= 2 && cross(hull[k - 2], hull[k - 1], pts[i]) <= 0.0f) k--;
            hull[k++] = pts[i];
        }
        for (int i = (int)pts.size() - 2, t = k + 1; i >= 0; --i) {
            while (k >= t && cross(hull[k - 2], hull[k - 1], pts[i]) <= 0.0f) k--;
            hull[k++] = pts[i];
        }
        hull.resize(k > 0 ? k - 1 : 0);
        return hull;
    }

    bool SegInsidePoly(const ImVec2& a, const ImVec2& b,
                       const std::vector<ImVec2>& poly, float& out_t0, float& out_t1)
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
            const float s = side(c) >= 0.0f ? 1.0f : -1.0f;
            const float f0 = s * side(a), f1 = s * side(b), df = f1 - f0;
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

    std::vector<ImVec2> ClipHalfPlane(const std::vector<ImVec2>& poly,
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

    void SubtractPoly(std::vector<ImVec2> piece, const std::vector<ImVec2>& B,
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
            const float s = cs >= 0.0f ? 1.0f : -1.0f;
            auto outside = ClipHalfPlane(piece, p, q, -s);
            if (!outside.empty()) out.push_back(std::move(outside));
            piece = ClipHalfPlane(piece, p, q, s);
        }
    }

    void DrawSegmentOutsideUnion(ImDrawList* dl, const ImVec2& a, const ImVec2& b,
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
        auto lerp_pt = [&](float t) {
            return ImVec2(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t);
        };
        if (covered.empty()) {
            dl->AddLine(a, b, color, 1.0f);
            return;
        }
        std::sort(covered.begin(), covered.end());
        std::vector<std::pair<float, float>> merged;
        for (auto& seg : covered) {
            if (merged.empty() || seg.first > merged.back().second)
                merged.push_back(seg);
            else
                merged.back().second = (std::max)(merged.back().second, seg.second);
        }
        float cur = 0.0f;
        for (auto& m : merged) {
            if (m.first > cur)
                dl->AddLine(lerp_pt(cur), lerp_pt(m.first), color, 1.0f);
            cur = (std::max)(cur, m.second);
        }
        if (cur < 1.0f)
            dl->AddLine(lerp_pt(cur), b, color, 1.0f);
    }

    void DrawPreviewChams(ImDrawList* dl,
                          const std::vector<std::array<ImVec2, 8>>& parts,
                          int mode, int shader,
                          ImU32 outline_col, ImU32 fill_col)
    {
        if (parts.empty()) return;

        if (mode == 0) {
            for (const auto& pc : parts)
                for (const auto& e : k_box_edges)
                    dl->AddLine(pc[e[0]], pc[e[1]], outline_col, 1.0f);
            return;
        }

        if (mode == 1) {
            for (const auto& pc : parts) {
                std::vector<ImVec2> pts(pc.begin(), pc.end());
                auto hull = ConvexHull(std::move(pts));
                if (hull.size() >= 3)
                    dl->AddConvexPolyFilled(hull.data(), (int)hull.size(), fill_col);
            }
            for (const auto& pc : parts)
                for (const auto& e : k_box_edges)
                    dl->AddLine(pc[e[0]], pc[e[1]], outline_col, 1.0f);
            return;
        }

        std::vector<std::vector<ImVec2>> hulls;
        hulls.reserve(parts.size());
        for (const auto& pc : parts) {
            std::vector<ImVec2> pts(pc.begin(), pc.end());
            auto h = ConvexHull(std::move(pts));
            if (h.size() >= 3) hulls.push_back(std::move(h));
        }

        std::vector<std::vector<ImVec2>> clipped;
        clipped.reserve(hulls.size() * 2);
        for (int i = 0; i < (int)hulls.size(); ++i) {
            std::vector<std::vector<ImVec2>> pieces{ hulls[i] };
            for (int j = 0; j < i && !pieces.empty(); ++j) {
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

            Cheat::Visuals::ShaderChams::DrawFill(dl, clipped, (float)ImGui::GetTime(),
                                                  shader, false, false);
            const ImU32 shader_outline = Cheat::Visuals::ShaderChams::OutlineColor(shader, false);
            for (int i = 0; i < (int)hulls.size(); ++i) {
                const auto& hull = hulls[i];
                const int n = (int)hull.size();
                for (int e = 0; e < n; ++e)
                    DrawSegmentOutsideUnion(dl, hull[e], hull[(e + 1) % n], hulls, i, shader_outline);
            }
        } else {
            ImDrawListFlags backup = dl->Flags;
            dl->Flags &= ~ImDrawListFlags_AntiAliasedFill;
            for (const auto& piece : clipped)
                dl->AddConvexPolyFilled(piece.data(), (int)piece.size(), fill_col);
            dl->Flags = backup;
            for (int i = 0; i < (int)hulls.size(); ++i) {
                const auto& hull = hulls[i];
                const int n = (int)hull.size();
                for (int e = 0; e < n; ++e)
                    DrawSegmentOutsideUnion(dl, hull[e], hull[(e + 1) % n], hulls, i, outline_col);
            }
        }
    }

    void SnapEspBox(float min_x, float min_y, float max_x, float max_y,
                    float& x1, float& y1, float& x2, float& y2)
    {
        x1 = Floor(min_x);
        y1 = Floor(min_y);
        x2 = std::ceil(max_x);
        y2 = std::ceil(max_y);
        if (x2 <= x1) x2 = x1 + 1.0f;
        if (y2 <= y1) y2 = y1 + 1.0f;
    }

    void DrawPlainBox(ImDrawList* dl, ImVec2 tl, ImVec2 br, ImU32 color)
    {
        float x1, y1, x2, y2;
        SnapEspBox(tl.x, tl.y, br.x, br.y, x1, y1, x2, y2);
        dl->AddRect(ImVec2(x1 - 1, y1 - 1), ImVec2(x2 + 1, y2 + 1), IM_COL32(0, 0, 0, 255));
        dl->AddRect(ImVec2(x1 + 1, y1 + 1), ImVec2(x2 - 1, y2 - 1), IM_COL32(0, 0, 0, 255));
        dl->AddRect(ImVec2(x1, y1), ImVec2(x2, y2), color);
    }

    void DrawCornerBox(ImDrawList* dl, ImVec2 tl, ImVec2 br, ImU32 color)
    {
        float x1, y1, x2, y2;
        SnapEspBox(tl.x, tl.y, br.x, br.y, x1, y1, x2, y2);
        const float lw = (std::max)(2.0f, Floor((x2 - x1) * 0.25f));
        const float lh = (std::max)(2.0f, Floor((y2 - y1) * 0.25f));
        const ImU32 black = IM_COL32(0, 0, 0, 255);
        auto seg = [&](float ax, float ay, float bx, float by) {
            dl->AddLine(ImVec2(ax, ay), ImVec2(bx, by), black, 3.0f);
            dl->AddLine(ImVec2(ax, ay), ImVec2(bx, by), color, 1.0f);
        };
        seg(x1, y1, x1 + lw, y1); seg(x1, y1, x1, y1 + lh);
        seg(x2 - lw, y1, x2, y1); seg(x2, y1, x2, y1 + lh);
        seg(x1, y2 - lh, x1, y2); seg(x1, y2, x1 + lw, y2);
        seg(x2 - lw, y2, x2, y2); seg(x2, y2 - lh, x2, y2);
    }

    void DrawSkeletonLine(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 color)
    {
        dl->AddLine(a, b, IM_COL32(0, 0, 0, 255), 3.0f);
        dl->AddLine(a, b, color, 1.0f);
    }

    ImU32 TierDotColor(int tier, int alpha)
    {
        if (tier == Cheat::Settings::PART_PRIMARY)
            return IM_COL32(90, 160, 255, alpha);
        if (tier == Cheat::Settings::PART_SECONDARY)
            return IM_COL32(100, 220, 130, alpha);
        if (tier == Cheat::Settings::PART_TERTIARY)
            return IM_COL32(245, 210, 80, alpha);
        return IM_COL32(0, 0, 0, 0);
    }

    void DrawFadeDot(ImDrawList* dl, const ImVec2& c, int tier, bool hover)
    {
        if (tier == Cheat::Settings::PART_OFF) return;

        const float glow = hover ? 11.0f : 9.0f;
        const float core = hover ? 2.4f : 2.0f;
        const int steps = 14;
        for (int i = steps; i >= 1; --i) {
            const float t = (float)i / (float)steps;
            const float r = core + (glow - core) * t;
            const float falloff = (1.0f - t) * (1.0f - t);
            const int a = (int)((hover ? 42.0f : 28.0f) * falloff);
            if (a <= 0) continue;
            dl->AddCircleFilled(c, r, TierDotColor(tier, a), 32);
        }
        dl->AddCircleFilled(c, core, TierDotColor(tier, hover ? 160 : 110), 24);
        dl->AddCircleFilled(c, core * 0.45f, TierDotColor(tier, hover ? 210 : 150), 16);
    }
}

namespace Cheat::Visuals {

    void ESPPreview::Initialize()
    {
        if (g_InitDone) return;
        if (!Cheat::Core::g_Device || !Cheat::Core::g_DeviceContext) return;

        if (!g_Renderer.Initialize(Cheat::Core::g_Device, Cheat::Core::g_DeviceContext, 600, 900))
            return;

        if (g_PreviewModelOBJSize > 0)
            g_Renderer.LoadModelFromMemory(g_PreviewModelOBJData, g_PreviewModelOBJSize);
        if (g_PreviewModelTextureSize > 0)
            g_Renderer.LoadTextureFromMemory(g_PreviewModelTexture, g_PreviewModelTextureSize);

        g_InitDone = true;
    }

    void ESPPreview::Shutdown()
    {
        g_Renderer.Shutdown();
        g_InitDone = false;
    }

    void ESPPreview::Render()
    {
        if (!g_InitDone) Initialize();

        const auto& s = Cheat::g_Settings;
        float now = (float)ImGui::GetTime();
        float dt = now - g_LastTime;
        if (dt > 0.1f || dt < 0.0f) dt = 0.016f;
        g_LastTime = now;

        ImFont* font = fonts::ui();
        const float fs = fonts::ui_size(font);
        ImFont* esp_font = fonts::selected();
        if (!esp_font) esp_font = ImGui::GetFont();
        const float esp_fs = fonts::snap_px(s.esp.font_size);

        const ImVec2 avail = ImGui::GetContentRegionAvail();
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();

        const float hintH = fs * 2.0f + 8.0f;
        const float modelH = (std::max)(1.0f, avail.y - hintH);

        ImGui::InvisibleButton("##preview_drag", ImVec2(avail.x, modelH));
        const bool hovered = ImGui::IsItemHovered();
        static bool s_dragged = false;
        if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Right))
            g_Renderer.SetAutoSpin(!g_Renderer.IsAutoSpinning());
        if (ImGui::IsItemActivated())
            s_dragged = false;
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 3.0f)) {
            s_dragged = true;
            g_Renderer.SetAutoSpin(false);
            g_Renderer.AddRotationDelta(ImGui::GetIO().MouseDelta.x * 0.007f,
                                        ImGui::GetIO().MouseDelta.y * 0.007f);
            g_Renderer.NotifyManualInput();
        }
        if (hovered && ImGui::GetIO().MouseWheel != 0.0f)
            g_Renderer.AddZoom(ImGui::GetIO().MouseWheel * 0.045f);

        const ImVec2 imgMin = origin;
        const ImVec2 imgMax(origin.x + avail.x, origin.y + modelH);
        const bool clicked_part = hovered && !s_dragged &&
            ImGui::IsMouseReleased(ImGuiMouseButton_Left) &&
            !ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);

        if (!g_Renderer.IsReady()) {
            const char* msg = "preview model missing";
            const ImVec2 tsz = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, msg);
            dl->AddText(font, fs,
                ImVec2(imgMin.x + (avail.x - tsz.x) * 0.5f, imgMin.y + modelH * 0.45f),
                colors::text_inactive_u32(), msg);
        } else {
            g_Renderer.Update(dt);
            dl->AddImage((ImTextureID)g_Renderer.GetTextureID(), imgMin, imgMax);

            float u0, v0, u1, v1;
            if (g_Renderer.GetProjectedUVBounds(u0, v0, u1, v1)) {
                const float imgW = imgMax.x - imgMin.x;
                const float imgH = imgMax.y - imgMin.y;
                auto UV = [&](float u, float v) {
                    return ImVec2(imgMin.x + u * imgW, imgMin.y + v * imgH);
                };
                const ImVec2 boxMin = UV(u0, v0);
                const ImVec2 boxMax = UV(u1, v1);
                float bx1, by1, bx2, by2;
                SnapEspBox(boxMin.x, boxMin.y, boxMax.x, boxMax.y, bx1, by1, bx2, by2);
                const float bcx = Floor((bx1 + bx2) * 0.5f);

                constexpr float k_hp = 72.0f;
                constexpr float k_max_hp = 100.0f;
                constexpr float k_dist = 26.0f;
                const float hp_frac = k_hp / k_max_hp;

                if (s.esp.chams) {
                    std::vector<std::array<std::pair<float, float>, 8>> uv_boxes;
                    if (g_Renderer.GetProjectedPartBoxes(uv_boxes)) {
                        std::vector<std::array<ImVec2, 8>> parts;
                        parts.reserve(uv_boxes.size());
                        for (const auto& box : uv_boxes) {
                            std::array<ImVec2, 8> pc{};
                            for (int i = 0; i < 8; ++i)
                                pc[i] = UV(box[i].first, box[i].second);
                            parts.push_back(pc);
                        }
                        DrawPreviewChams(dl, parts, s.esp.chams_mode, s.esp.chams_shader,
                            Col(s.esp.chams_outline_color), Col(s.esp.chams_fill_color));
                    }
                }

                if (s.esp.box) {
                    if (s.esp.box_mode == 1)
                        DrawCornerBox(dl, ImVec2(bx1, by1), ImVec2(bx2, by2), Col(s.esp.box_color));
                    else
                        DrawPlainBox(dl, ImVec2(bx1, by1), ImVec2(bx2, by2), Col(s.esp.box_color));
                }

                if (s.esp.healthbar) {

                    const float bar_x2 = bx1 - 3.0f;
                    const float bar_x1 = bar_x2 - 2.0f;
                    const float bar_h = by2 - by1;
                    const float fill_h = Floor(bar_h * hp_frac + 0.5f);
                    const float fill_top = by2 - fill_h;

                    dl->AddRectFilled(ImVec2(bar_x1 - 1.0f, by1 - 1.0f),
                                      ImVec2(bar_x2 + 1.0f, by2 + 1.0f),
                                      IM_COL32(0, 0, 0, 255));
                    const ImU32 hp_col = IM_COL32((int)(255 * (1.0f - hp_frac)),
                                                  (int)(255 * hp_frac), 0, 255);
                    if (fill_h > 0.0f)
                        dl->AddRectFilled(ImVec2(bar_x1, fill_top), ImVec2(bar_x2, by2), hp_col);

                    if (s.esp.health_text && hp_frac < 1.0f) {
                        char hp_buf[16];
                        std::snprintf(hp_buf, sizeof(hp_buf), "%.0f", k_hp);
                        const ImVec2 tsz = esp_font->CalcTextSizeA(esp_fs, FLT_MAX, 0.0f, hp_buf);
                        widgets::draw_outlined_text(dl, esp_font, esp_fs,
                            ImVec2(Floor(bar_x1 - tsz.x - 2.0f), Floor(fill_top - tsz.y * 0.5f)),
                            IM_COL32(255, 255, 255, 255), hp_buf);
                    }
                } else if (s.esp.health_text) {
                    char hp_buf[16];
                    std::snprintf(hp_buf, sizeof(hp_buf), "%.0f hp", k_hp);
                    const ImVec2 tsz = esp_font->CalcTextSizeA(esp_fs, FLT_MAX, 0.0f, hp_buf);
                    widgets::draw_outlined_text(dl, esp_font, esp_fs,
                        ImVec2(Floor(bx1 - tsz.x - 4.0f), by1),
                        IM_COL32(255, 255, 255, 255), hp_buf);
                }

                if (s.esp.name) {
                    const char* name = s.esp.name_mode == 0 ? "Big Yahu" : "big_yahu228";
                    const ImVec2 tsz = esp_font->CalcTextSizeA(esp_fs, FLT_MAX, 0.0f, name);
                    widgets::draw_outlined_text(dl, esp_font, esp_fs,
                        ImVec2(Floor(bcx - tsz.x * 0.5f), by1 - esp_fs - 2.0f),
                        Col(s.esp.name_color), name);
                }

                float bottom_y = by2 + 2.0f;
                if (s.esp.distance) {
                    char dist_buf[32];
                    if (s.esp.distance_unit == 1)
                        std::snprintf(dist_buf, sizeof(dist_buf), "%.0fm", k_dist * 0.28f);
                    else
                        std::snprintf(dist_buf, sizeof(dist_buf), "%.0f studs", k_dist);
                    const ImVec2 tsz = esp_font->CalcTextSizeA(esp_fs, FLT_MAX, 0.0f, dist_buf);
                    widgets::draw_outlined_text(dl, esp_font, esp_fs,
                        ImVec2(Floor(bcx - tsz.x * 0.5f), Floor(bottom_y)),
                        Col(s.esp.distance_color), dist_buf);
                    bottom_y += tsz.y + 2.0f;
                }

                if (s.esp.tool) {
                    const char* tool = "[ClassicSword]";
                    const ImVec2 tsz = esp_font->CalcTextSizeA(esp_fs, FLT_MAX, 0.0f, tool);
                    widgets::draw_outlined_text(dl, esp_font, esp_fs,
                        ImVec2(Floor(bcx - tsz.x * 0.5f), Floor(bottom_y)),
                        Col(s.esp.tool_color), tool);
                }

                if (s.esp.flags) {
                    struct Flag { const char* text; ImU32 color; };
                    const Flag flag_list[] = {
                        { "standing", IM_COL32(160, 160, 160, 255) },
                        { "speed",    IM_COL32(245, 220, 80, 255) },
                    };
                    float flag_y = by1;
                    for (const auto& f : flag_list) {
                        widgets::draw_outlined_text(dl, esp_font, esp_fs,
                            ImVec2(Floor(bx2 + 4.0f), Floor(flag_y)), f.color, f.text);
                        flag_y += esp_fs + 2.0f;
                    }
                }

                if (s.esp.skeleton) {
                    std::vector<float> segs;
                    if (g_Renderer.GetProjectedR6Skeleton(segs)) {
                        const ImU32 bone = Col(s.esp.skeleton_color);
                        for (size_t i = 0; i + 3 < segs.size(); i += 4) {
                            DrawSkeletonLine(dl,
                                UV(segs[i], segs[i + 1]),
                                UV(segs[i + 2], segs[i + 3]),
                                bone);
                        }
                    }
                }

                {
                    auto& acfg = Cheat::g_Settings.aim.active();
                    std::vector<std::pair<int, std::pair<float, float>>> centers;
                    if (g_Renderer.GetProjectedAimPartCenters(centers)) {
                        const ImVec2 mouse = ImGui::GetIO().MousePos;
                        constexpr float k_hit_r = 18.0f;
                        int best_click = -1;
                        float best_d2 = k_hit_r * k_hit_r;

                        for (const auto& entry : centers) {
                            const int part = entry.first;
                            if (part < 0 || part >= Cheat::Settings::AIM_PART_COUNT) continue;
                            const ImVec2 c = UV(entry.second.first, entry.second.second);
                            const float dx = mouse.x - c.x;
                            const float dy = mouse.y - c.y;
                            const float d2 = dx * dx + dy * dy;
                            if (hovered && d2 < best_d2) {
                                best_d2 = d2;
                                best_click = part;
                            }
                        }

                        for (const auto& entry : centers) {
                            const int part = entry.first;
                            if (part < 0 || part >= Cheat::Settings::AIM_PART_COUNT) continue;
                            const int tier = acfg.part_tier[part];
                            if (tier == Cheat::Settings::PART_OFF) continue;
                            const ImVec2 c = UV(entry.second.first, entry.second.second);
                            DrawFadeDot(dl, c, tier, part == best_click);
                        }

                        if (clicked_part && best_click >= 0) {
                            int& tier = acfg.part_tier[best_click];
                            tier = (tier + 1) % 4;
                            acfg.SyncPartsFromTiers();
                        }
                    }
                }
            }
        }

        const float hy = origin.y + modelH + 2.0f;
        auto hint = [&](const char* t, float y) {
            const ImVec2 tsz = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, t);
            dl->AddText(font, fs, ImVec2(origin.x + (avail.x - tsz.x) * 0.5f, y),
                        colors::text_inactive_u32(), t);
        };
        hint("click part: blue > green > yellow", hy);
        hint("drag rotate | double rmb spin | scroll", hy + fs + 2.0f);
    }

}
