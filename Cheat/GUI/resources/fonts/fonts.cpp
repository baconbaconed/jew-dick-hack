#include "fonts.h"
#include "imgui/misc/imgui_freetype.h"
#include "font_tahoma.h"
#include "font_tahoma_bold.h"

namespace fonts {
    ImFont* tahoma_bold = nullptr;
    ImFont* tahoma = nullptr;
    ImFont* esp = nullptr;
    ImFont* esp_bold = nullptr;

    void load(ImGuiIO& io) {
        const unsigned int mono_flags =
            ImGuiFreeTypeLoaderFlags_MonoHinting |
            ImGuiFreeTypeLoaderFlags_Monochrome;

        io.Fonts->SetFontLoader(ImGuiFreeType::GetFontLoader());
        io.Fonts->FontLoaderFlags = 0;

        ImFontConfig cfg{};
        cfg.PixelSnapH = true;
        cfg.OversampleH = 1;
        cfg.OversampleV = 1;
        cfg.RasterizerMultiply = 1.0f;
        cfg.FontLoaderFlags = mono_flags;
        cfg.FontDataOwnedByAtlas = false;

        tahoma = io.Fonts->AddFontFromMemoryTTF(Tahoma, sizeof(Tahoma), 13.0f, &cfg);

        ImFontConfig bold_cfg = cfg;
        bold_cfg.FontLoaderFlags =
            mono_flags | ImGuiFreeTypeLoaderFlags_ForceAutoHint;
        bold_cfg.RasterizerMultiply = 0.88f;
        tahoma_bold = io.Fonts->AddFontFromMemoryTTF(
            TahomaBold, sizeof(TahomaBold), 13.0f, &bold_cfg);

        esp      = tahoma;
        esp_bold = tahoma_bold;

        if (tahoma) {
            io.FontDefault = tahoma;
            return;
        }

        if (tahoma_bold) {
            io.FontDefault = tahoma_bold;
            return;
        }

        ImFontConfig fallback = cfg;
        fallback.SizePixels = 12.0f;
        io.FontDefault = io.Fonts->AddFontDefault(&fallback);
    }
}
