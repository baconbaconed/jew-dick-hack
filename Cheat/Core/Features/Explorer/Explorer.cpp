#define NOMINMAX
#define IMGUI_DEFINE_MATH_OPERATORS
#include "Explorer.h"

#include "../../Globals/Globals.h"
#include "../../Memory/Memory.h"
#include "../../Graphics.h"
#include "../../Roblox/Engine/Offsets/Offsets.h"
#include "../../Roblox/Engine/Classes/Classes.h"
#include "../../../Settings.h"

#include "../../../GUI/colors/colors.h"
#include "../../../GUI/resources/fonts/fonts.h"
#include "../../../GUI/widgets/widgets.h"
#include "../../../GUI/imgui/imgui.h"
#include "../../../GUI/imgui/imgui_internal.h"

#include <stb_image.h>

#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <string>
#include <vector>
#include <unordered_map>
#include <thread>
#include <atomic>
#include <mutex>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <Windows.h>

#include "../../../GUI/resources/icons/iconsdex.h"

#undef GetClassName

namespace {

struct IconTex {
    ID3D11ShaderResourceView* srv = nullptr;
    int w = 16, h = 16;
};

std::unordered_map<std::string, IconTex> g_icons;
bool g_iconsLoaded = false;

ID3D11ShaderResourceView* DecodePng(const unsigned char* png, int len, int& out_w, int& out_h)
{
    if (!Cheat::Core::g_Device) return nullptr;

    int ch = 0;
    unsigned char* pixels = stbi_load_from_memory(png, len, &out_w, &out_h, &ch, 4);
    if (!pixels) return nullptr;

    D3D11_TEXTURE2D_DESC td{};
    td.Width            = out_w;
    td.Height           = out_h;
    td.MipLevels        = 1;
    td.ArraySize        = 1;
    td.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage            = D3D11_USAGE_DEFAULT;
    td.BindFlags        = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA sd{};
    sd.pSysMem     = pixels;
    sd.SysMemPitch = out_w * 4;

    ID3D11Texture2D* tex = nullptr;
    HRESULT hr = Cheat::Core::g_Device->CreateTexture2D(&td, &sd, &tex);
    stbi_image_free(pixels);
    if (FAILED(hr) || !tex) return nullptr;

    ID3D11ShaderResourceView* srv = nullptr;
    hr = Cheat::Core::g_Device->CreateShaderResourceView(tex, nullptr, &srv);
    tex->Release();
    return SUCCEEDED(hr) ? srv : nullptr;
}

void RegisterIcon(const char* key, const unsigned char* png, int len)
{
    IconTex it;
    it.srv = DecodePng(png, len, it.w, it.h);
    if (it.srv) g_icons[key] = it;
}

const char* IconKeyForClass(const std::string& cls)
{

    if (cls == "LocalScript" || cls == "Script" || cls == "ModuleScript")
        return "localscript";

    if (cls == "Part" || cls == "TrussPart" || cls == "WedgePart" ||
        cls == "CornerWedgePart" || cls == "Seat" || cls == "VehicleSeat" ||
        cls == "SpawnLocation" || cls == "UnionOperation" ||
        cls == "NegateOperation" || cls == "IntersectOperation")
        return "part";
    if (cls == "MeshPart") return "meshpart";
    if (cls == "Terrain")  return "terrain";

    if (cls == "Workspace")            return "workspace";
    if (cls == "Folder" || cls == "Configuration") return "folder";
    if (cls == "Model" || cls == "Actor" || cls == "WorldModel") return "model";

    if (cls == "Camera")               return "camera";
    if (cls == "Lighting")             return "lightning";
    if (cls == "Players")              return "players";
    if (cls == "Player")               return "player";
    if (cls == "Humanoid")             return "humanoid";
    if (cls == "Backpack")             return "backpack";
    if (cls == "StarterGear")          return "startergear";
    if (cls == "Stats")                return "stats";
    if (cls == "StatsItem")            return "statsitem";
    if (cls == "GuiService")           return "guiservice";
    if (cls == "RunService")           return "runservice";
    if (cls == "LogService")           return "logservice";
    if (cls == "SoundService" || cls == "Sound") return "soundservice";
    if (cls == "MarketplaceService")   return "marketplaceservice";
    if (cls == "ContentProvider")      return "contentprovider";
    if (cls == "VideoCaptureService")  return "videocapture";

    if (cls == "PlayerGui" || cls == "StarterGui" || cls == "ScreenGui" ||
        cls == "CoreGui")
        return "playergui";
    if (cls == "Frame" || cls == "TextLabel" || cls == "TextButton" ||
        cls == "ImageLabel" || cls == "ImageButton" || cls == "TextBox")
        return "frame";

    if (cls == "BoolValue")   return "boolvalue";
    if (cls == "IntValue")    return "intvalue";
    if (cls == "NumberValue" || cls == "DoubleConstrainedValue") return "doubletype";
    return "typeshit";
}

ID3D11ShaderResourceView* IconSrv(const std::string& cls, int& w, int& h)
{
    const char* key = IconKeyForClass(cls);
    auto it = g_icons.find(key);
    if (it == g_icons.end()) it = g_icons.find("typeshit");
    if (it == g_icons.end()) { w = h = 16; return nullptr; }
    w = it->second.w; h = it->second.h;
    return it->second.srv;
}

bool IsScriptClass(const std::string& cls)
{
    return cls == "LocalScript" || cls == "Script" || cls == "ModuleScript";
}

struct Node {
    std::uint64_t address = 0;
    std::string   name;
    std::string   cls;
    std::vector<Node> children;
    bool loaded = false;
    bool open   = false;
};

Node          g_root;
std::uint64_t g_rootAddr = 0;

constexpr int kChildDrawCap = 800;

void LoadChildren(Node& n)
{
    if (n.loaded) return;
    n.loaded = true;
    n.children.clear();

    Cheat::Instance inst(n.address);
    for (const auto& c : inst.GetChildren()) {
        Node cn;
        cn.address = c.address;
        cn.name    = c.GetName();
        cn.cls     = c.GetClassName();
        n.children.push_back(std::move(cn));
    }
}

void EnsureRoot()
{
    const std::uint64_t dm = Cheat::Globals::InstanceDataModel.address;
    if (dm && dm != g_rootAddr) {
        g_rootAddr = dm;
        g_root = Node{};
        g_root.address = dm;
        g_root.cls     = "DataModel";
        g_root.name    = "game";
        g_root.open    = true;
    }
}

struct SearchResult {
    std::uint64_t address = 0;
    std::string   name;
    std::string   cls;
    std::string   path;
};

std::mutex                 g_searchMtx;
std::vector<SearchResult>  g_searchResults;
std::atomic<std::uint32_t> g_searchGen{ 0 };
std::atomic<bool>          g_searching{ false };
bool                       g_searchActive = false;

std::string ToLower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

void RunSearch(std::string query, std::uint64_t rootAddr, std::uint32_t gen)
{
    const std::string q = ToLower(query);
    std::vector<SearchResult> local;

    constexpr int kNodeBudget = 400000;
    constexpr int kMaxResults = 1000;

    struct Frame { std::uint64_t addr; std::string path; };
    std::vector<Frame> stack;
    stack.push_back({ rootAddr, "game" });

    int budget = kNodeBudget;
    while (!stack.empty()) {
        if (g_searchGen.load() != gen) return;
        if (budget-- <= 0) break;

        Frame f = std::move(stack.back());
        stack.pop_back();

        Cheat::Instance inst(f.addr);
        for (const auto& child : inst.GetChildren()) {
            if (g_searchGen.load() != gen) return;

            std::string name = child.GetName();
            std::string cls  = child.GetClassName();

            if (ToLower(name).find(q) != std::string::npos ||
                ToLower(cls).find(q)  != std::string::npos) {
                if (local.size() < kMaxResults) {
                    SearchResult r;
                    r.address = child.address;
                    r.name    = name;
                    r.cls     = cls;
                    r.path    = f.path + "." + name;
                    local.push_back(std::move(r));
                }
            }

            if ((int)stack.size() < kNodeBudget)
                stack.push_back({ child.address, f.path + "." + name });
        }
    }

    if (g_searchGen.load() != gen) return;
    {
        std::lock_guard<std::mutex> lk(g_searchMtx);
        g_searchResults = std::move(local);
    }
    g_searching.store(false);
}

void StartSearch(const char* query)
{
    const std::uint32_t gen = ++g_searchGen;
    g_searching.store(true);
    g_searchActive = true;
    {
        std::lock_guard<std::mutex> lk(g_searchMtx);
        g_searchResults.clear();
    }
    if (!g_rootAddr) { g_searching.store(false); return; }
    std::thread(RunSearch, std::string(query), g_rootAddr, gen).detach();
}

void StopSearch()
{
    ++g_searchGen;
    g_searching.store(false);
    g_searchActive = false;
    std::lock_guard<std::mutex> lk(g_searchMtx);
    g_searchResults.clear();
}

struct Target {
    std::uint64_t address = 0;
    std::string   name;
    std::string   cls;
    std::string   path;
    bool valid = false;
};

Target g_ctx;
Target g_detail;
bool   g_detailOpen = false;
bool   g_openCtxPopup = false;

std::vector<unsigned char> g_bcBytes;
std::uint64_t              g_bcForAddr = 0;
std::string               g_bcStatus;

std::string BuildPath(std::uint64_t address)
{

    std::vector<std::string> parts;
    Cheat::Instance inst(address);
    std::uint64_t cur = address;
    int guard = 64;
    while (cur && guard-- > 0) {
        Cheat::Instance node(cur);
        std::string nm = node.GetName();
        parts.push_back(nm.empty() ? "?" : nm);
        if (cur == g_rootAddr) break;
        auto parent = node.GetParent();
        if (!parent) break;
        cur = parent->address;
    }
    std::string out;
    for (auto it = parts.rbegin(); it != parts.rend(); ++it) {
        if (!out.empty()) out += ".";
        out += *it;
    }
    return out;
}

void DumpBytecode(const Target& t)
{
    g_bcBytes.clear();
    g_bcForAddr = t.address;
    g_bcStatus.clear();

    uintptr_t bcOffset = Offsets::LocalScript::ByteCode;
    if (t.cls == "ModuleScript") bcOffset = Offsets::ModuleScript::ByteCode;

    std::uint64_t bcObj = g_Memory.Read<std::uint64_t>(t.address + bcOffset);
    if (!g_Memory.IsValid(bcObj)) { g_bcStatus = "no bytecode object"; return; }

    std::uint64_t ptr  = g_Memory.Read<std::uint64_t>(bcObj + Offsets::ByteCode::Pointer);
    std::uint64_t size = g_Memory.Read<std::uint64_t>(bcObj + Offsets::ByteCode::Size);
    if (!g_Memory.IsValid(ptr) || size == 0 || size > (16u * 1024u * 1024u)) {
        g_bcStatus = "empty / unreadable bytecode";
        return;
    }

    g_bcBytes.resize(static_cast<size_t>(size));
    SIZE_T got = g_Memory.ReadRaw(ptr, g_bcBytes.data(), static_cast<SIZE_T>(size));
    g_bcBytes.resize(got);
    char buf[96];
    std::snprintf(buf, sizeof(buf), "%zu bytes (compressed Luau bytecode)", (size_t)got);
    g_bcStatus = buf;
}

void SaveBytecodeToFile(const Target& t)
{
    if (g_bcBytes.empty()) return;
    std::string safe;
    for (char c : t.name)
        safe += (std::isalnum((unsigned char)c) ? c : '_');
    if (safe.empty()) safe = "script";
    std::string fname = safe + ".luauc";

    std::ofstream f(fname, std::ios::binary);
    if (f) {
        f.write(reinterpret_cast<const char*>(g_bcBytes.data()),
                static_cast<std::streamsize>(g_bcBytes.size()));
        g_bcStatus = "saved: " + fname;
    } else {
        g_bcStatus = "failed to write file";
    }
}

float PanelFontSize()
{
    return fonts::ui_size();
}

void PushWindowChrome()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_ResizeGrip,        ImVec4(0.15f, 0.15f, 0.15f, 0.35f));
    ImGui::PushStyleColor(ImGuiCol_ResizeGripHovered, ImVec4(51/255.f, 122/255.f, 231/255.f, 0.7f));
    ImGui::PushStyleColor(ImGuiCol_ResizeGripActive,  ImVec4(51/255.f, 122/255.f, 231/255.f, 1.0f));
}
void PopWindowChrome()
{
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(2);
}

void DrawFramedBox(ImDrawList* draw_list, const ImVec2& min, const ImVec2& max)
{
    const ImVec2 inner_min(min.x + 1.0f, min.y + 1.0f);
    const ImVec2 inner_max(max.x - 1.0f, max.y - 1.0f);
    const ImVec2 fill_min(inner_min.x + 1.0f, inner_min.y + 1.0f);
    const ImVec2 fill_max(inner_max.x - 1.0f, inner_max.y - 1.0f);

    draw_list->AddRectFilled(fill_min, fill_max, colors::widget_track_u32());
    draw_list->AddRect(inner_min, inner_max, colors::widget_inline_u32(), 0.0f, 0, 1.0f);
    draw_list->AddRect(min, max, colors::widget_outline_u32(), 0.0f, 0, 1.0f);
}

void TextLine(ImU32 color, const char* fmt, ...)
{
    char buf[512];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    ImFont* font = fonts::ui();
    const float fs = PanelFontSize();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    widgets::draw_outlined_text(dl, font, fs, ImVec2(ImFloor(pos.x), ImFloor(pos.y)), color, buf);
    const ImVec2 sz = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, buf);
    ImGui::Dummy(ImVec2(sz.x, sz.y + 3.0f));
}

constexpr float kRowH     = 18.0f;
constexpr float kIndentPx = 13.0f;
constexpr float kIconSz   = 14.0f;

void DrawRowVisuals(const Node& n, float indent, bool showArrow, bool open)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p = ImGui::GetCursorScreenPos();

    float x = p.x + indent;

    if (showArrow) {
        const ImU32 acol = IM_COL32(150, 150, 155, 255);
        const float cy = p.y + kRowH * 0.5f;
        if (open) {
            dl->AddTriangleFilled(ImVec2(x, cy - 2.0f), ImVec2(x + 7.0f, cy - 2.0f),
                                  ImVec2(x + 3.5f, cy + 3.0f), acol);
        } else {
            dl->AddTriangleFilled(ImVec2(x + 1.0f, cy - 4.0f), ImVec2(x + 6.0f, cy),
                                  ImVec2(x + 1.0f, cy + 4.0f), acol);
        }
    }
    x += 11.0f;

    int iw = 16, ih = 16;
    ID3D11ShaderResourceView* srv = IconSrv(n.cls, iw, ih);
    const ImVec2 icoMin(x, p.y + (kRowH - kIconSz) * 0.5f);
    const ImVec2 icoMax(icoMin.x + kIconSz, icoMin.y + kIconSz);
    if (srv) dl->AddImage((ImTextureID)(uintptr_t)srv, icoMin, icoMax);
    x += kIconSz + 4.0f;

    ImFont* font = fonts::ui();
    const float fs = PanelFontSize();
    const char* label = n.name.empty() ? "?" : n.name.c_str();
    const ImVec2 tpos(ImFloor(x), ImFloor(p.y + (kRowH - fs) * 0.5f));
    widgets::draw_outlined_text(dl, font, fs, tpos, IM_COL32(220, 220, 224, 255), label);
}

void RenderNode(Node& n, int depth)
{
    ImGui::PushID((void*)(uintptr_t)n.address);

    const float indent = depth * kIndentPx + 2.0f;
    const bool showArrow = (!n.loaded) || (!n.children.empty());

    const ImVec2 rowStart = ImGui::GetCursorScreenPos();
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(51/255.f, 122/255.f, 231/255.f, 0.25f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive,  ImVec4(51/255.f, 122/255.f, 231/255.f, 0.40f));
    const bool clicked = ImGui::Selectable("##row", false,
        ImGuiSelectableFlags_AllowDoubleClick, ImVec2(0.0f, kRowH));
    ImGui::PopStyleColor(2);

    const bool rightClicked = ImGui::IsItemClicked(ImGuiMouseButton_Right);

    ImGui::SetCursorScreenPos(rowStart);
    DrawRowVisuals(n, indent, showArrow, n.open);
    ImGui::SetCursorScreenPos(ImVec2(rowStart.x, rowStart.y + kRowH));

    if (clicked) {
        if (showArrow) {
            n.open = !n.open;
            if (n.open) LoadChildren(n);
        }
    }
    if (rightClicked) {
        g_ctx.address = n.address;
        g_ctx.name    = n.name;
        g_ctx.cls     = n.cls;
        g_ctx.path    = BuildPath(n.address);
        g_ctx.valid   = true;
        g_openCtxPopup = true;
    }

    if (n.open && n.loaded) {
        const int count = (int)n.children.size();
        const int shown = std::min(count, kChildDrawCap);
        for (int i = 0; i < shown; ++i)
            RenderNode(n.children[i], depth + 1);
        if (count > shown) {
            ImGui::SetCursorPosX(indent + 20.0f);
            TextLine(colors::text_inactive_u32(), "... %d more (use search)", count - shown);
        }
    }

    ImGui::PopID();
}

void DrawContextMenu()
{
    if (g_openCtxPopup) {
        ImGui::OpenPopup("##explorer_ctx");
        g_openCtxPopup = false;
    }

    constexpr float kItemH   = 16.0f;
    constexpr float kPadX    = 5.0f;
    constexpr float kMenuW   = 150.0f;
    constexpr float kHeaderH = 34.0f;

    enum Action { kProps, kBytecode, kCopyName, kCopyClass, kCopyPath, kCopyAddr };
    struct Item { const char* label; Action action; };

    const bool isScript = IsScriptClass(g_ctx.cls);
    Item items[6];
    int itemCount = 0;
    items[itemCount++] = { "properties", kProps };
    if (isScript) items[itemCount++] = { "view bytecode", kBytecode };
    items[itemCount++] = { "copy name",    kCopyName };
    items[itemCount++] = { "copy class",   kCopyClass };
    items[itemCount++] = { "copy path",    kCopyPath };
    items[itemCount++] = { "copy address", kCopyAddr };

    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 0.0f);

    ImGui::SetNextWindowSize(ImVec2(kMenuW, kHeaderH + itemCount * kItemH + 3.0f));
    if (ImGui::BeginPopup("##explorer_ctx", ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove)) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 win_min = ImGui::GetWindowPos();
        const ImVec2 win_max(win_min.x + ImGui::GetWindowWidth(),
                             win_min.y + ImGui::GetWindowHeight());
        DrawFramedBox(dl, win_min, win_max);

        ImFont* font = fonts::ui();
        const float fs = PanelFontSize();

        widgets::draw_outlined_text(dl, font, fs,
            ImVec2(ImFloor(win_min.x + kPadX), ImFloor(win_min.y + 3.0f)),
            colors::accent_u32(),
            g_ctx.name.empty() ? "?" : g_ctx.name.c_str());
        widgets::draw_outlined_text(dl, font, fs,
            ImVec2(ImFloor(win_min.x + kPadX), ImFloor(win_min.y + 3.0f + fs + 2.0f)),
            colors::text_inactive_u32(),
            g_ctx.cls.c_str());
        dl->AddLine(ImVec2(win_min.x + 2.0f, win_min.y + kHeaderH - 1.0f),
                    ImVec2(win_max.x - 2.0f, win_min.y + kHeaderH - 1.0f),
                    colors::widget_inline_u32());

        for (int i = 0; i < itemCount; ++i) {
            ImGui::SetCursorPos(ImVec2(0.0f, kHeaderH + i * kItemH));
            char btn_id[32];
            std::snprintf(btn_id, sizeof(btn_id), "##ctx_%d", i);
            const bool clicked = ImGui::InvisibleButton(btn_id, ImVec2(kMenuW, kItemH));
            const bool hovered = ImGui::IsItemHovered();
            const ImVec2 item_min = ImGui::GetItemRectMin();

            if (hovered) {
                dl->AddRectFilled(ImVec2(item_min.x + 2.0f, item_min.y),
                                  ImVec2(item_min.x + kMenuW - 2.0f, item_min.y + kItemH),
                                  colors::widget_track_hover_u32());
            }

            const ImVec2 tsz = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, items[i].label);
            widgets::draw_outlined_text(dl, font, fs,
                ImVec2(ImFloor(item_min.x + kPadX),
                       ImFloor(item_min.y + (kItemH - tsz.y) * 0.5f)),
                hovered ? colors::text_active_u32() : colors::text_inactive_u32(),
                items[i].label);

            if (!clicked) continue;

            switch (items[i].action) {
            case kProps:
                g_detail = g_ctx;
                g_detail.valid = true;
                g_detailOpen = true;
                g_bcForAddr = 0;
                g_bcBytes.clear();
                g_bcStatus.clear();
                break;
            case kBytecode:
                g_detail = g_ctx;
                g_detail.valid = true;
                g_detailOpen = true;
                DumpBytecode(g_detail);
                break;
            case kCopyName:  ImGui::SetClipboardText(g_ctx.name.c_str());  break;
            case kCopyClass: ImGui::SetClipboardText(g_ctx.cls.c_str());   break;
            case kCopyPath:  ImGui::SetClipboardText(g_ctx.path.c_str());  break;
            case kCopyAddr: {
                char buf[32];
                std::snprintf(buf, sizeof(buf), "0x%llX", (unsigned long long)g_ctx.address);
                ImGui::SetClipboardText(buf);
                break;
            }
            }
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}

void DrawDetailWindow(float window_alpha)
{
    if (!g_detailOpen || !g_detail.valid) return;

    const bool isScript = IsScriptClass(g_detail.cls);
    ImGui::SetNextWindowSize(
        isScript ? ImVec2(300.0f, 300.0f) : ImVec2(240.0f, 150.0f),
        ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(200.0f, 110.0f), ImVec2(700.0f, 800.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, window_alpha);
    PushWindowChrome();

    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse;

    bool open = true;
    if (ImGui::Begin("##explorer_detail", &open, flags)) {
        colors::draw_panel_background(window_alpha);

        constexpr float kMargin = 10.0f;
        const ImVec2 sz = ImGui::GetWindowSize();
        ImGui::SetCursorPos(ImVec2(kMargin, kMargin));

        if (widgets::begin_child_panel(
                "detail_child",
                ImVec2(sz.x - kMargin * 2.0f, sz.y - kMargin * 2.0f),
                "properties", fonts::ui_bold(), PanelFontSize(),
                nullptr, nullptr, nullptr)) {

            ImGui::SetCursorPosX(6.0f);
            TextLine(colors::accent_u32(),
                "%s", g_detail.name.empty() ? "?" : g_detail.name.c_str());

            ImGui::SetCursorPosX(6.0f);
            TextLine(colors::text_active_u32(), "class: %s", g_detail.cls.c_str());
            ImGui::SetCursorPosX(6.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, colors::text_inactive);
            ImGui::TextWrapped("path: %s", g_detail.path.c_str());
            ImGui::PopStyleColor();
            ImGui::SetCursorPosX(6.0f);
            TextLine(colors::text_active_u32(),
                "address: 0x%llX", (unsigned long long)g_detail.address);

            Cheat::Instance inst(g_detail.address);
            ImGui::SetCursorPosX(6.0f);
            TextLine(colors::text_active_u32(), "children: %d", (int)inst.GetChildren().size());

            if (g_detail.cls == "Humanoid") {
                Cheat::Humanoid hum(g_detail.address);
                ImGui::SetCursorPosX(6.0f);
                TextLine(colors::text_active_u32(),
                    "health: %.1f / %.1f", hum.GetHealth(), hum.GetMaxHealth());
                ImGui::SetCursorPosX(6.0f);
                TextLine(colors::text_active_u32(),
                    "walkspeed: %.1f  jump: %.1f", hum.GetWalkSpeed(), hum.GetJumpPower());
            }

            if (IsScriptClass(g_detail.cls)) {
                ImGui::Separator();
                ImGui::SetCursorPosX(6.0f);
                if (widgets::button("dump")) DumpBytecode(g_detail);
                ImGui::SameLine();
                if (widgets::button("save"))   SaveBytecodeToFile(g_detail);
                ImGui::SameLine();
                if (widgets::button("copy hex")) {
                    std::string hex;
                    hex.reserve(g_bcBytes.size() * 2);
                    static const char* d = "0123456789ABCDEF";
                    for (unsigned char b : g_bcBytes) {
                        hex += d[b >> 4];
                        hex += d[b & 0xF];
                    }
                    ImGui::SetClipboardText(hex.c_str());
                }

                ImGui::SetCursorPosX(6.0f);
                TextLine(colors::text_inactive_u32(), "%s", g_bcStatus.empty()
                    ? "bytecode is compressed; not source" : g_bcStatus.c_str());

                ImGui::SetCursorPosX(6.0f);
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.25f));
                if (ImGui::BeginChild("##bc_hex", ImVec2(-6.0f, -6.0f), true,
                        ImGuiWindowFlags_HorizontalScrollbar)) {
                    const size_t maxShow = std::min<size_t>(g_bcBytes.size(), 4096);
                    char line[80];
                    for (size_t i = 0; i < maxShow; i += 16) {
                        std::string row;
                        for (size_t j = 0; j < 16 && i + j < maxShow; ++j) {
                            std::snprintf(line, sizeof(line), "%02X ", g_bcBytes[i + j]);
                            row += line;
                        }
                        ImGui::TextUnformatted(row.c_str());
                    }
                    if (g_bcBytes.size() > maxShow)
                        TextLine(colors::text_inactive_u32(), "... (%zu more bytes)",
                                 g_bcBytes.size() - maxShow);
                }
                ImGui::EndChild();
                ImGui::PopStyleColor();
            }
        }
        widgets::end_child_panel();
    }
    ImGui::End();
    PopWindowChrome();
    ImGui::PopStyleVar();

    if (!open) g_detailOpen = false;
}

}

namespace Cheat::Features {

void Explorer::Initialize()
{
    if (g_iconsLoaded) return;
    if (!Cheat::Core::g_Device) return;
    g_iconsLoaded = true;

    RegisterIcon("workspace",          workspace,          (int)sizeof(workspace));
    RegisterIcon("folder",             folder,             (int)sizeof(folder));
    RegisterIcon("camera",             camera,             (int)sizeof(camera));
    RegisterIcon("lightning",          lightning,          (int)sizeof(lightning));
    RegisterIcon("humanoid",           humanoid,           (int)sizeof(humanoid));
    RegisterIcon("part",               part,               (int)sizeof(part));
    RegisterIcon("players",            players,            (int)sizeof(players));
    RegisterIcon("meshpart",           meshpart,           (int)sizeof(meshpart));
    RegisterIcon("player",             player,             (int)sizeof(player));
    RegisterIcon("model",              model,              (int)sizeof(model));
    RegisterIcon("terrain",            terrain,            (int)sizeof(terrain));
    RegisterIcon("localscript",        localscript,        (int)sizeof(localscript));
    RegisterIcon("localscripts",       localscripts,       (int)sizeof(localscripts));
    RegisterIcon("playergui",          playergui,          (int)sizeof(playergui));
    RegisterIcon("stats",              stats,              (int)sizeof(stats));
    RegisterIcon("guiservice",         guiservice,         (int)sizeof(guiservice));
    RegisterIcon("videocapture",       videocapture,       (int)sizeof(videocapture));
    RegisterIcon("runservice",         runservice,         (int)sizeof(runservice));
    RegisterIcon("frame",              frame,              (int)sizeof(frame));
    RegisterIcon("csd",                csd,                (int)sizeof(csd));
    RegisterIcon("contentprovider",    contentprovider,    (int)sizeof(contentprovider));
    RegisterIcon("nonreplicated",      nonreplicated,      (int)sizeof(nonreplicated));
    RegisterIcon("startergear",        startergear,        (int)sizeof(startergear));
    RegisterIcon("timerdevice",        timerdevice,        (int)sizeof(timerdevice));
    RegisterIcon("backpack",           backpack,           (int)sizeof(backpack));
    RegisterIcon("marketplaceservice", marketplaceservice, (int)sizeof(marketplaceservice));
    RegisterIcon("soundservice",       soundservice,       (int)sizeof(soundservice));
    RegisterIcon("logservice",         logservice,         (int)sizeof(logservice));
    RegisterIcon("statsitem",          statsitem,          (int)sizeof(statsitem));
    RegisterIcon("boolvalue",          boolvalue,          (int)sizeof(boolvalue));
    RegisterIcon("intvalue",           intvalue,           (int)sizeof(intvalue));
    RegisterIcon("doubletype",         doubletype,         (int)sizeof(doubletype));
    RegisterIcon("typeshit",           typeshit,           (int)sizeof(typeshit));
}

void Explorer::Shutdown()
{
    StopSearch();
    for (auto& kv : g_icons)
        if (kv.second.srv) kv.second.srv->Release();
    g_icons.clear();
    g_iconsLoaded = false;
}

void Explorer::Render(float alpha)
{
    if (!Cheat::g_Settings.misc.explorer) return;
    if (alpha <= 0.001f) return;

    Initialize();
    EnsureRoot();

    ImGui::SetNextWindowSize(ImVec2(360.0f, 460.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(260.0f, 220.0f), ImVec2(1000.0f, 1000.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
    PushWindowChrome();

    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse;

    bool open = true;
    if (ImGui::Begin("##jewsploit_explorer", &open, flags)) {
        colors::draw_panel_background(alpha);

        constexpr float kMargin = 10.0f;
        const ImVec2 sz = ImGui::GetWindowSize();
        ImGui::SetCursorPos(ImVec2(kMargin, kMargin));

        if (widgets::begin_child_panel(
                "explorer_child",
                ImVec2(sz.x - kMargin * 2.0f, sz.y - kMargin * 2.0f),
                "explorer", fonts::ui_bold(), PanelFontSize(),
                nullptr, nullptr, nullptr)) {

            static char query[128] = "";

            ImGui::SetCursorPos(ImVec2(6.0f, ImGui::GetCursorPosY() + 4.0f));
            const float search_w = ImGui::GetContentRegionAvail().x - 6.0f;
            const bool entered = widgets::input_text("##explorer_search", "search...",
                query, IM_ARRAYSIZE(query), search_w, ImGuiInputTextFlags_EnterReturnsTrue);

            if (entered && query[0]) StartSearch(query);
            if (g_searchActive && query[0] == '\0') StopSearch();

            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
            if (ImGui::BeginChild("##explorer_scroll",
                    ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_NoScrollbar)) {

                if (g_searchActive) {
                    std::lock_guard<std::mutex> lk(g_searchMtx);
                    if (g_searching.load()) {
                        ImGui::SetCursorPosX(6.0f);
                        TextLine(colors::text_inactive_u32(), "searching...");
                    }
                    if (g_searchResults.empty() && !g_searching.load()) {
                        ImGui::SetCursorPosX(6.0f);
                        TextLine(colors::text_inactive_u32(), "no matches");
                    }

                    ImGuiListClipper clipper;
                    clipper.Begin((int)g_searchResults.size(), kRowH);
                    while (clipper.Step()) {
                        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                            const SearchResult& r = g_searchResults[i];
                            ImGui::PushID(i);
                            const ImVec2 rowStart = ImGui::GetCursorScreenPos();
                            ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
                                ImVec4(51/255.f, 122/255.f, 231/255.f, 0.25f));
                            const bool clicked = ImGui::Selectable("##sr", false,
                                ImGuiSelectableFlags_AllowDoubleClick, ImVec2(0.0f, kRowH));
                            ImGui::PopStyleColor();
                            const bool rc = ImGui::IsItemClicked(ImGuiMouseButton_Right);

                            ImDrawList* dl = ImGui::GetWindowDrawList();
                            int iw, ih;
                            ID3D11ShaderResourceView* srv = IconSrv(r.cls, iw, ih);
                            const ImVec2 icoMin(rowStart.x + 4.0f,
                                                rowStart.y + (kRowH - kIconSz) * 0.5f);
                            if (srv) dl->AddImage((ImTextureID)(uintptr_t)srv, icoMin,
                                ImVec2(icoMin.x + kIconSz, icoMin.y + kIconSz));
                            ImFont* font = fonts::ui();
                            const float fs = PanelFontSize();
                            widgets::draw_outlined_text(dl, font, fs,
                                ImVec2(icoMin.x + kIconSz + 4.0f,
                                       ImFloor(rowStart.y + (kRowH - fs) * 0.5f)),
                                IM_COL32(220, 220, 224, 255),
                                r.name.empty() ? "?" : r.name.c_str());
                            ImGui::SetCursorScreenPos(ImVec2(rowStart.x, rowStart.y + kRowH));

                            if (clicked || rc) {
                                g_ctx.address = r.address;
                                g_ctx.name    = r.name;
                                g_ctx.cls     = r.cls;
                                g_ctx.path    = r.path;
                                g_ctx.valid   = true;
                                g_openCtxPopup = true;
                            }
                            ImGui::PopID();
                        }
                    }
                    clipper.End();
                } else {
                    if (g_root.address) {
                        if (!g_root.loaded) LoadChildren(g_root);
                        for (auto& child : g_root.children)
                            RenderNode(child, 0);
                    } else {
                        ImGui::SetCursorPosX(6.0f);
                        TextLine(colors::text_inactive_u32(), "not attached / no datamodel");
                    }
                }

                DrawContextMenu();
            }
            ImGui::EndChild();
            ImGui::PopStyleColor();
        }
        widgets::end_child_panel();
    }
    ImGui::End();
    PopWindowChrome();
    ImGui::PopStyleVar();

    if (!open) Cheat::g_Settings.misc.explorer = false;

    DrawDetailWindow(alpha);
}

}
