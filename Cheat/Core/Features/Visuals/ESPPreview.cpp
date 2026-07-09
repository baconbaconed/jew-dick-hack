#define NOMINMAX
#define IMGUI_DEFINE_MATH_OPERATORS
#include "ESPPreview.h"
#include "PreviewRenderer.h"
#include "preview_model/preview_model_obj.h"
#include "preview_model/preview_model_texture.h"
#include "../../Graphics.h"
#include "../../../Settings.h"
#include "../../../GUI/colors/colors.h"
#include "../../../GUI/resources/fonts/fonts.h"
#include "../../../GUI/widgets/widgets.h"
#include "../../../GUI/imgui/imgui_internal.h"

#include <filesystem>
#include <string>
#include <algorithm>
#include <cstdio>
#include <Windows.h>

static std::string ExeDir() {
    char buf[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    std::string p(buf);
    auto pos = p.find_last_of("\\/");
    return pos != std::string::npos ? p.substr(0, pos) : p;
}

namespace {
    Cheat::Core::PreviewRenderer g_Renderer;
    float                        g_LastTime = 0.0f;
    bool                         g_InitDone = false;
}

namespace Cheat::Visuals {

    void ESPPreview::Initialize()
    {
        if (g_InitDone) return;
        if (!Cheat::Core::g_Device || !Cheat::Core::g_DeviceContext) return;

        if (!g_Renderer.Initialize(
                Cheat::Core::g_Device, Cheat::Core::g_DeviceContext,
                {}, {}, 360, 560))
            return;

        bool obj_ok = (g_PreviewModelOBJSize > 0) &&
            g_Renderer.UploadModelFromMemory(
                reinterpret_cast<const char*>(g_PreviewModelOBJData),
                g_PreviewModelOBJSize);

        bool tex_ok = g_Renderer.LoadTextureFromMemory(
            g_PreviewModelTexture, g_PreviewModelTextureSize);

        if (!obj_ok || !tex_ok) {
            auto base = ExeDir();
            std::string obj = base + "\\preview_model\\model.obj";
            std::string tex = base + "\\preview_model\\texture.png";
            if (!std::filesystem::exists(obj)) {
                obj = "Cheat\\Core\\Features\\Visuals\\preview_model\\model.obj";
                tex = "Cheat\\Core\\Features\\Visuals\\preview_model\\texture.png";
            }
            if (!obj_ok) g_Renderer.UploadModel(obj);
            if (!tex_ok) g_Renderer.LoadTexture(tex);
        }

        g_InitDone = true;
    }

    void ESPPreview::Shutdown() { g_Renderer.Shutdown(); g_InitDone = false; }

    static void DrawCornerBox(ImDrawList* dl, ImVec2 tl, ImVec2 br, ImU32 color)
    {
        const float x1 = ImFloor(tl.x), y1 = ImFloor(tl.y);
        const float x2 = ImFloor(br.x), y2 = ImFloor(br.y);
        const float lw = (std::max)(2.0f, ImFloor((x2 - x1) * 0.25f));
        const float lh = (std::max)(2.0f, ImFloor((y2 - y1) * 0.25f));

        struct Seg { ImVec2 a, b; };
        const Seg segs[8] = {
            { {x1, y1}, {x1 + lw, y1} }, { {x1, y1}, {x1, y1 + lh} },
            { {x2 - lw, y1}, {x2, y1} }, { {x2, y1}, {x2, y1 + lh} },
            { {x1, y2 - lh}, {x1, y2} }, { {x1, y2}, {x1 + lw, y2} },
            { {x2, y2 - lh}, {x2, y2} }, { {x2 - lw, y2}, {x2, y2} },
        };
        const ImU32 black = IM_COL32(0, 0, 0, 255);
        for (const auto& sg : segs) dl->AddLine(sg.a, sg.b, black, 3.0f);
        for (const auto& sg : segs) dl->AddLine(sg.a, sg.b, color, 1.0f);
    }

    static void DrawPlainBox(ImDrawList* dl, ImVec2 tl, ImVec2 br, ImU32 color)
    {
        const float x1 = ImFloor(tl.x), y1 = ImFloor(tl.y);
        const float x2 = ImFloor(br.x), y2 = ImFloor(br.y);
        const ImU32 black = IM_COL32(0, 0, 0, 255);
        dl->AddRect(ImVec2(x1 - 1, y1 - 1), ImVec2(x2 + 1, y2 + 1), black, 0.0f, 0, 1.0f);
        dl->AddRect(ImVec2(x1 + 1, y1 + 1), ImVec2(x2 - 1, y2 - 1), black, 0.0f, 0, 1.0f);
        dl->AddRect(ImVec2(x1, y1), ImVec2(x2, y2), color, 0.0f, 0, 1.0f);
    }

    static ImU32 Col(const float c[4]) {
        return ImGui::ColorConvertFloat4ToU32(ImVec4(c[0], c[1], c[2], c[3]));
    }

    static void DrawHint(ImDrawList* dl, ImVec2 origin, float area_w, float y,
                         const char* text, ImU32 col)
    {
        ImFont* font = fonts::tahoma ? fonts::tahoma : ImGui::GetFont();
        const float fs = font && font->LegacySize > 0.0f ? font->LegacySize : 13.0f;
        const ImVec2 tsz = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, text);
        const ImVec2 pos(ImFloor(origin.x + (area_w - tsz.x) * 0.5f), ImFloor(y));
        widgets::draw_outlined_text(dl, font, fs, pos, col, text);
    }

    void ESPPreview::Render()
    {
        if (!g_InitDone) Initialize();

        const Cheat::Settings& s = Cheat::g_Settings;

        float now = static_cast<float>(ImGui::GetTime());
        float dt  = now - g_LastTime;
        if (dt > 0.1f || dt < 0.0f) dt = 0.016f;
        g_LastTime = now;

        ImFont* font = fonts::tahoma ? fonts::tahoma : ImGui::GetFont();
        const float fs = font && font->LegacySize > 0.0f ? font->LegacySize : 13.0f;

        ImFont* esp_font = (s.esp.font == 1)
            ? (fonts::esp_bold ? fonts::esp_bold : fonts::tahoma_bold)
            : (fonts::esp ? fonts::esp : fonts::tahoma);
        if (!esp_font) esp_font = ImGui::GetFont();
        const float esp_fs   = (std::max)(8.0f, s.esp.font_size);
        const float small_fs = (std::max)(9.0f, esp_fs - 3.0f);

        const ImVec2 avail  = ImGui::GetContentRegionAvail();
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        ImDrawList*  dl     = ImGui::GetWindowDrawList();

        const float kHintH   = fs + 2.0f;
        const float kHintGap = 2.0f;
        const float hintAreaH = kHintH * 2.0f + kHintGap + 4.0f;
        const float modelH    = avail.y - hintAreaH;

        ImGui::InvisibleButton("##preview_drag",
            ImVec2(avail.x, (std::max)(modelH, 1.0f)),
            ImGuiButtonFlags_MouseButtonLeft);

        const bool hovered  = ImGui::IsItemHovered();
        const bool dragging = ImGui::IsItemActive() &&
                              ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f);

        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            g_Renderer.SetAutoSpin(!g_Renderer.IsAutoSpinning());

        if (dragging) {
            const ImVec2 delta = ImGui::GetIO().MouseDelta;
            constexpr float kSens = 0.007f;
            g_Renderer.AddRotationDelta(delta.x * kSens, delta.y * kSens);
            g_Renderer.NotifyManualInput();
        }

        if (hovered) {
            const float wheel = ImGui::GetIO().MouseWheel;
            if (wheel != 0.0f)
                g_Renderer.AddZoom(wheel * 0.045f);
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
        }

        const ImVec2 imgMin = origin;
        const ImVec2 imgMax = ImVec2(origin.x + avail.x, origin.y + modelH);

        if (g_Renderer.IsReady()) {
            g_Renderer.Update(dt);

            dl->AddImage((ImTextureID)g_Renderer.GetTextureID(), imgMin, imgMax);

            float u0, v0, u1, v1;
            const bool boundsOK = g_Renderer.GetProjectedUVBounds(u0, v0, u1, v1);

            const float imgW = imgMax.x - imgMin.x;
            const float imgH = imgMax.y - imgMin.y;
            auto UVtoScreen = [&](float u, float v) -> ImVec2 {
                return ImVec2(imgMin.x + u * imgW, imgMin.y + v * imgH);
            };

            if (boundsOK) {
                const ImVec2 boxMin = UVtoScreen(u0, v0);
                const ImVec2 boxMax = UVtoScreen(u1, v1);
                const float  boxCentreX = (boxMin.x + boxMax.x) * 0.5f;

                constexpr float kFakeHpFrac = 0.72f;
                constexpr float kFakeHp     = 72.0f;
                constexpr float kFakeStuds  = 128.0f;

                if (s.esp.box) {
                    const ImU32 box_col = Col(s.esp.box_color);

                    if (s.esp.box_mode == 1)
                        DrawCornerBox(dl, boxMin, boxMax, box_col);
                    else
                        DrawPlainBox(dl, boxMin, boxMax, box_col);
                }

                if (s.esp.healthbar) {
                    const float bar_x2 = ImFloor(boxMin.x) - 4.0f;
                    const float bar_x1 = bar_x2 - 2.0f;
                    const float y1 = ImFloor(boxMin.y);
                    const float y2 = ImFloor(boxMax.y);
                    const float fill_top = y2 - (y2 - y1) * kFakeHpFrac;

                    dl->AddRectFilled(ImVec2(bar_x1 - 1, y1 - 1), ImVec2(bar_x2 + 1, y2 + 1), IM_COL32(0, 0, 0, 255));
                    const ImU32 hp_col = IM_COL32((int)(255 * (1.0f - kFakeHpFrac)), (int)(255 * kFakeHpFrac), 0, 255);
                    dl->AddRectFilled(ImVec2(bar_x1, fill_top), ImVec2(bar_x2, y2), hp_col);

                    if (s.esp.health_text) {
                        char hp_buf[16];
                        std::snprintf(hp_buf, sizeof(hp_buf), "%.0f", kFakeHp);
                        const ImVec2 tsz = esp_font->CalcTextSizeA(small_fs, FLT_MAX, 0.0f, hp_buf);
                        widgets::draw_outlined_text(dl, esp_font, small_fs,
                            ImVec2(ImFloor(bar_x1 - tsz.x - 2.0f), ImFloor(fill_top - tsz.y * 0.5f)),
                            IM_COL32(255, 255, 255, 255), hp_buf);
                    }
                } else if (s.esp.health_text) {
                    char hp_buf[16];
                    std::snprintf(hp_buf, sizeof(hp_buf), "%.0f hp", kFakeHp);
                    const ImVec2 tsz = esp_font->CalcTextSizeA(small_fs, FLT_MAX, 0.0f, hp_buf);
                    widgets::draw_outlined_text(dl, esp_font, small_fs,
                        ImVec2(ImFloor(boxMin.x - tsz.x - 4.0f), ImFloor(boxMin.y)),
                        IM_COL32(255, 255, 255, 255), hp_buf);
                }

                if (s.esp.name) {
                    const char* name = s.esp.name_mode == 0 ? "Big Yahu" : "big_yahu228";
                    const ImVec2 tsz = esp_font->CalcTextSizeA(esp_fs, FLT_MAX, 0.0f, name);
                    const ImVec2 tpos(
                        ImFloor(boxCentreX - tsz.x * 0.5f),
                        ImFloor(boxMin.y - esp_fs - 2.0f));
                    widgets::draw_outlined_text(dl, esp_font, esp_fs, tpos,
                        Col(s.esp.name_color), name);
                }

                float bottom_y = boxMax.y + 2.0f;

                if (s.esp.distance) {
                    char dist_buf[32];
                    if (s.esp.distance_unit == 1)
                        std::snprintf(dist_buf, sizeof(dist_buf), "%.0fm", kFakeStuds * 0.28f);
                    else
                        std::snprintf(dist_buf, sizeof(dist_buf), "%.0f studs", kFakeStuds);

                    const ImVec2 tsz = esp_font->CalcTextSizeA(small_fs, FLT_MAX, 0.0f, dist_buf);
                    widgets::draw_outlined_text(dl, esp_font, small_fs,
                        ImVec2(ImFloor(boxCentreX - tsz.x * 0.5f), ImFloor(bottom_y)),
                        Col(s.esp.distance_color), dist_buf);
                    bottom_y += tsz.y + 1.0f;
                }

                if (s.esp.tool) {
                    const char* tool_buf = "[Kosher Blade]";
                    const ImVec2 tsz = esp_font->CalcTextSizeA(small_fs, FLT_MAX, 0.0f, tool_buf);
                    widgets::draw_outlined_text(dl, esp_font, small_fs,
                        ImVec2(ImFloor(boxCentreX - tsz.x * 0.5f), ImFloor(bottom_y)),
                        Col(s.esp.tool_color), tool_buf);
                    bottom_y += tsz.y + 1.0f;
                }

                if (s.esp.flags) {
                    struct Flag { const char* text; ImU32 color; };
                    const Flag flag_list[2] = {
                        { "moving", IM_COL32(120, 220, 120, 255) },
                        { "speed",  IM_COL32(245, 220,  80, 255) },
                    };
                    const float flag_fs = (std::max)(9.0f, esp_fs * 0.8f);
                    float flag_y = boxMin.y;
                    for (const auto& f : flag_list) {
                        widgets::draw_outlined_text(dl, esp_font, flag_fs,
                            ImVec2(ImFloor(boxMax.x + 4.0f), ImFloor(flag_y)), f.color, f.text);
                        flag_y += flag_fs + 1.0f;
                    }
                }
            }

            if (s.esp.skeleton) {

                ImVec2 J[13];
                bool   ok[13] = {};
                for (int i = 0; i < 13; ++i) {
                    float u, v;
                    ok[i] = g_Renderer.GetProjectedJoint(i, u, v);
                    if (ok[i]) J[i] = UVtoScreen(u, v);
                }

                const ImU32 kBone = ImGui::ColorConvertFloat4ToU32(
                    ImVec4(s.esp.skeleton_color[0], s.esp.skeleton_color[1],
                           s.esp.skeleton_color[2], s.esp.skeleton_color[3]));
                constexpr float kW = 1.5f;

                auto bone = [&](int a, int b) {
                    if (ok[a] && ok[b]) dl->AddLine(J[a], J[b], kBone, kW);
                };

                bone(0,  1);

                bone(1,  8);

                bone(1,  2); bone(2,  4); bone(4,  6);

                bone(1,  3); bone(3,  5); bone(5,  7);

                bone(8,  9); bone(9, 11);

                bone(8, 10); bone(10,12);
            }

            const bool wantChams   = s.esp.chams;
            const bool wantOutline = s.esp.outline;
            if (wantChams || wantOutline) {
                std::vector<std::pair<float,float>> sil_uvs;
                if (g_Renderer.GetProjectedSilhouette(sil_uvs) && sil_uvs.size() >= 3) {
                    std::vector<ImVec2> sil_screen;
                    sil_screen.reserve(sil_uvs.size());
                    for (const auto& uv : sil_uvs)
                        sil_screen.emplace_back(UVtoScreen(uv.first, uv.second));

                    if (wantChams) {
                        const ImU32 fill_c = Col(s.esp.chams_fill_color);
                        const ImU32 out_c  = Col(s.esp.chams_outline_color);

                        if (s.esp.chams_mode != 0)
                            dl->AddConcavePolyFilled(sil_screen.data(), (int)sil_screen.size(), fill_c);
                        dl->AddPolyline(sil_screen.data(), (int)sil_screen.size(),
                                        out_c, ImDrawFlags_Closed, 1.5f);
                    }

                    if (wantOutline) {
                        const ImU32 out_c = ImGui::ColorConvertFloat4ToU32(ImVec4(
                            s.esp.outline_color[0], s.esp.outline_color[1],
                            s.esp.outline_color[2], s.esp.outline_color[3]));
                        dl->AddPolyline(sil_screen.data(), (int)sil_screen.size(),
                                        out_c, ImDrawFlags_Closed, 1.5f);
                    }
                }
            }
        } else {

            const char* msg = "loading model...";
            const ImVec2 tsz = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, msg);
            const ImVec2 tpos(
                ImFloor(imgMin.x + (avail.x - tsz.x) * 0.5f),
                ImFloor(imgMin.y + (modelH - tsz.y) * 0.5f));
            widgets::draw_outlined_text(dl, font, fs, tpos,
                IM_COL32(120, 120, 120, 255), msg);
        }

        const float hint1Y = origin.y + modelH + 3.0f;
        const float hint2Y = hint1Y + kHintH + kHintGap;

        DrawHint(dl, origin, avail.x, hint1Y,
            "drag  |  scroll to zoom",
            IM_COL32(55, 55, 55, 255));

        const bool spinning = g_Renderer.IsAutoSpinning();
        DrawHint(dl, origin, avail.x, hint2Y,
            spinning ? "double click to stop" : "double click to spin",
            spinning ? IM_COL32(51, 122, 231, 180) : IM_COL32(55, 55, 55, 255));

        ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + avail.y));
        ImGui::Dummy(ImVec2(0.0f, 0.0f));
    }

}
