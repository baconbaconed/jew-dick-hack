#define IMGUI_DEFINE_MATH_OPERATORS
#include "InfoPanel.h"
#include "../colors/colors.h"
#include "../resources/fonts/fonts.h"
#include "../widgets/widgets.h"
#include "../imgui/imgui_internal.h"
#include "../../Core/PlayerHandler/PlayerHandler.h"
#include "../../Core/Globals/Globals.h"
#include "../../Core/Roblox/Engine/Classes/Classes.h"
#include <cmath>
#include <cstdint>
#include <cstdio>

namespace {

    constexpr ImU32 k_row_hovered  = IM_COL32(255, 255, 255,  12);
    constexpr ImU32 k_row_selected = IM_COL32( 51, 122, 231,  35);
    constexpr ImU32 k_row_sep      = IM_COL32(255, 255, 255,   8);
    constexpr ImU32 k_label_col    = IM_COL32(136, 136, 136, 255);
    constexpr ImU32 k_value_col    = IM_COL32(255, 255, 255, 255);
    constexpr ImU32 k_hp_bar_bg    = IM_COL32( 30,  30,  30, 200);
    constexpr ImU32 k_hp_bar_low   = IM_COL32(220,  55,  55, 255);
    constexpr ImU32 k_hp_bar_mid   = IM_COL32(230, 180,  30, 255);
    constexpr ImU32 k_hp_bar_full  = IM_COL32( 55, 200,  90, 255);

    float GetDistance(const Vector3& pos) {
        if (!Cheat::Globals::Workspace) return -1.0f;
        auto cam_ptr = Cheat::Globals::Workspace->GetCurrentCamera();
        if (!cam_ptr) return -1.0f;
        Camera cam(cam_ptr->address);
        Vector3 cp = cam.GetPosition();
        float dx = pos.x - cp.x, dy = pos.y - cp.y, dz = pos.z - cp.z;
        return std::sqrtf(dx*dx + dy*dy + dz*dz);
    }

    void DetailRow(ImDrawList* dl, ImFont* font, float fs,
                   float x, float& y, float w,
                   const char* label, const char* value)
    {
        const ImVec2 lsz = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, label);
        const ImVec2 vsz = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, value);

        dl->AddText(font, fs, ImVec2(ImFloor(x + 6.0f), ImFloor(y)), k_label_col, label);
        dl->AddText(font, fs, ImVec2(ImFloor(x + w - vsz.x - 6.0f), ImFloor(y)), k_value_col, value);
        (void)lsz;
        y += fs + 2.0f;
    }

    void HpBar(ImDrawList* dl, float x, float y, float w, float hp, float maxhp) {
        if (maxhp <= 0.0f) return;
        const float frac = std::fmaxf(0.0f, std::fminf(hp / maxhp, 1.0f));
        const float bh   = 4.0f;

        ImU32 bar_col;
        if (frac > 0.6f)      bar_col = k_hp_bar_full;
        else if (frac > 0.3f) bar_col = k_hp_bar_mid;
        else                  bar_col = k_hp_bar_low;

        const float bx = x + 6.0f, bw = w - 12.0f;
        dl->AddRectFilled(ImVec2(bx, y),           ImVec2(bx + bw, y + bh),        k_hp_bar_bg, 2.0f);
        dl->AddRectFilled(ImVec2(bx, y),           ImVec2(bx + bw * frac, y + bh), bar_col,     2.0f);
    }
}

namespace Cheat::GUI {

    void InfoPanel::Render(float child_w, float avail_h)
    {
        ImFont* font = fonts::tahoma ? fonts::tahoma : ImGui::GetFont();
        const float fs = font && font->LegacySize > 0.0f ? font->LegacySize : 13.0f;
        const float row_h = fs + 6.0f;

        PlayerCache sel;
        if (s_selected != 0) {
            sel = PlayerHandler::GetCachedPlayer(s_selected);
            if (sel.address == 0)
                s_selected = 0;
        }

        const bool has_sel  = s_selected != 0;
        const float detail_h = has_sel ? (fs * 9.0f + 28.0f) : 0.0f;
        const float list_h   = avail_h - detail_h - (has_sel ? 4.0f : 0.0f);

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::SetCursorPosX(0.0f);
        if (ImGui::BeginChild("##pl_list", ImVec2(child_w - 4.0f, list_h), false,
                ImGuiWindowFlags_NoScrollbar))
        {
            const std::size_t total = PlayerHandler::GetPlayerCount();
            if (total == 0) {
                ImGui::SetCursorPosX(6.0f);
                ImGui::TextUnformatted("no players");
            } else {
                char cnt_buf[32];
                ImFormatString(cnt_buf, IM_ARRAYSIZE(cnt_buf), "total: %zu", total);
                ImGui::SetCursorPosX(6.0f);
                ImGui::TextUnformatted(cnt_buf);

                ImDrawList* dl = ImGui::GetWindowDrawList();

                PlayerHandler::ForEachPlayer([&](const PlayerCache& player) {
                    const std::uint64_t addr = player.address;
                    const bool selected = (addr == s_selected);
                    const ImVec2 row_min = ImGui::GetCursorScreenPos();
                    const ImVec2 row_max(row_min.x + child_w - 4.0f, row_min.y + row_h);

                    if (selected)
                        dl->AddRectFilled(row_min, row_max, k_row_selected);

                    ImGui::SetCursorPosX(0.0f);
                    char sel_id[32];
                    ImFormatString(sel_id, IM_ARRAYSIZE(sel_id), "##pl_%llx", (unsigned long long)addr);
                    if (ImGui::Selectable(sel_id, selected,
                            ImGuiSelectableFlags_None, ImVec2(child_w - 4.0f, row_h)))
                    {
                        s_selected = selected ? 0 : addr;
                    }

                    if (ImGui::IsItemHovered() && !selected)
                        dl->AddRectFilled(row_min, row_max, k_row_hovered);

                    const char* name = player.name.empty() ? "unknown" : player.name.c_str();
                    const ImVec2 txt_pos(row_min.x + 6.0f,
                                        row_min.y + (row_h - fs) * 0.5f);
                    dl->AddText(font, fs, ImFloor(txt_pos), k_value_col, name);

                    dl->AddLine(ImVec2(row_min.x + 4.0f, row_max.y - 1.0f),
                                ImVec2(row_max.x - 4.0f, row_max.y - 1.0f),
                                k_row_sep, 1.0f);
                });
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();

        if (!has_sel || sel.address == 0) return;

        const PlayerCache& p = sel;

        float hp = -1.0f, maxhp = 100.0f;
        float walkspeed = -1.0f;
        bool  walking   = false;
        std::string display_name;
        Vector3 root_pos{};
        bool    has_root = false;

        if (p.humanoid) {
            Humanoid hum(p.humanoid->address);
            hp          = hum.GetHealth();
            maxhp       = hum.GetMaxHealth();
            walkspeed   = hum.GetWalkSpeed();
            walking     = hum.IsWalking();
            display_name = hum.GetDisplayName();
        }
        if (p.humanoidRootPart) {
            BasePart bp(p.humanoidRootPart->address);
            root_pos = bp.GetPosition();
            has_root = true;
        }
        float dist = has_root ? GetDistance(root_pos) : -1.0f;

        ImGui::SetCursorPosX(0.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg,
            ImVec4(21.0f/255.f, 21.0f/255.f, 20.0f/255.f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        if (ImGui::BeginChild("##pl_detail",
                ImVec2(child_w - 4.0f, detail_h), false,
                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
        {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const ImVec2 org  = ImGui::GetWindowPos();
            const ImVec2 wsz  = ImGui::GetWindowSize();
            const float  w    = wsz.x;
            float        y    = org.y + 5.0f;

            dl->AddLine(ImVec2(org.x + 4.0f, org.y),
                        ImVec2(org.x + w - 4.0f, org.y),
                        IM_COL32(51, 122, 231, 80), 1.0f);

            {
                const char* lbl = "username";
                const char* val = p.name.empty() ? "unknown" : p.name.c_str();
                DetailRow(dl, font, fs, org.x, y, w, lbl, val);
            }

            if (!display_name.empty() && display_name != p.name) {
                char val[64]; ImFormatString(val, IM_ARRAYSIZE(val), "%s", display_name.c_str());
                DetailRow(dl, font, fs, org.x, y, w, "display", val);
            }

            if (hp >= 0.0f) {
                char val[32]; ImFormatString(val, IM_ARRAYSIZE(val), "%.0f / %.0f", hp, maxhp);
                DetailRow(dl, font, fs, org.x, y, w, "health", val);
                HpBar(dl, org.x, y, w, hp, maxhp);
                y += 8.0f;
            }

            if (dist >= 0.0f) {
                char val[32]; ImFormatString(val, IM_ARRAYSIZE(val), "%.1f studs", dist);
                DetailRow(dl, font, fs, org.x, y, w, "distance", val);
            }

            if (walkspeed >= 0.0f) {
                char val[32]; ImFormatString(val, IM_ARRAYSIZE(val), "%.0f", walkspeed);
                DetailRow(dl, font, fs, org.x, y, w, "walkspeed", val);
            }

            {
                DetailRow(dl, font, fs, org.x, y, w, "moving",
                    walking ? "yes" : "no");
            }

            if (has_root) {
                char val[64];
                ImFormatString(val, IM_ARRAYSIZE(val), "%.0f  %.0f  %.0f",
                    root_pos.x, root_pos.y, root_pos.z);
                DetailRow(dl, font, fs, org.x, y, w, "pos", val);
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }

}
