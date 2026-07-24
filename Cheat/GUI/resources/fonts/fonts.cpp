#include "fonts.h"
#include "imgui/misc/imgui_freetype.h"
#include "font_tahoma_bold.h"

namespace fonts {
    ImFont* imgui = nullptr;
    ImFont* tahoma_bold = nullptr;
    ImFont* tahoma = nullptr;
    ImFont* esp = nullptr;
    ImFont* esp_bold = nullptr;

    void load(ImGuiIO& io) {
        io.Fonts->SetFontLoader(ImGuiFreeType::GetFontLoader());
        io.Fonts->FontLoaderFlags = 0;

        const unsigned int mono_flags =
            ImGuiFreeTypeLoaderFlags_MonoHinting |
            ImGuiFreeTypeLoaderFlags_Monochrome;

        ImFontConfig mono{};
        mono.PixelSnapH = true;
        mono.OversampleH = 1;
        mono.OversampleV = 1;
        mono.RasterizerMultiply = 1.0f;
        mono.FontLoaderFlags = mono_flags;
        mono.SizePixels = 13.0f;
        imgui = io.Fonts->AddFontDefault(&mono);

        ImFontConfig bold{};
        bold.PixelSnapH = true;
        bold.OversampleH = 2;
        bold.OversampleV = 1;
        bold.RasterizerMultiply = 1.0f;
        bold.FontDataOwnedByAtlas = false;
        bold.FontLoaderFlags = ImGuiFreeTypeLoaderFlags_LightHinting;
        bold.SizePixels = 13.0f;
        tahoma_bold = io.Fonts->AddFontFromMemoryTTF(
            TahomaBold, sizeof(TahomaBold), 13.0f, &bold);

        tahoma = imgui;
        esp = imgui;
        esp_bold = tahoma_bold ? tahoma_bold : imgui;

        io.FontDefault = imgui ? imgui : tahoma_bold;
        if (!io.FontDefault)
            io.FontDefault = io.Fonts->AddFontDefault();
    }
}
