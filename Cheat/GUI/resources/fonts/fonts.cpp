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
        const unsigned int freetype_flags =
            ImGuiFreeTypeLoaderFlags_MonoHinting | ImGuiFreeTypeLoaderFlags_Monochrome;

        io.Fonts->SetFontLoader(ImGuiFreeType::GetFontLoader());

        io.Fonts->FontLoaderFlags = 0;

        ImFontConfig cfg{};
        cfg.PixelSnapH = true;
        cfg.OversampleH = 2;
        cfg.OversampleV = 1;
        cfg.RasterizerMultiply = 1.05f;
        cfg.FontLoaderFlags = freetype_flags;
        cfg.FontDataOwnedByAtlas = false;

        tahoma = io.Fonts->AddFontFromMemoryTTF(Tahoma, sizeof(Tahoma), 13.0f, &cfg);

        ImFontConfig bold_cfg = cfg;
        tahoma_bold = io.Fonts->AddFontFromMemoryTTF(TahomaBold, sizeof(TahomaBold), 13.0f, &bold_cfg);

        ImFontConfig esp_cfg{};
        esp_cfg.PixelSnapH = true;
        esp_cfg.OversampleH = 2;
        esp_cfg.OversampleV = 1;
        esp_cfg.FontDataOwnedByAtlas = false;
        esp_cfg.FontLoaderFlags = ImGuiFreeTypeLoaderFlags_LightHinting;

        esp      = io.Fonts->AddFontFromMemoryTTF(Tahoma,     sizeof(Tahoma),     14.0f, &esp_cfg);
        esp_bold = io.Fonts->AddFontFromMemoryTTF(TahomaBold, sizeof(TahomaBold), 14.0f, &esp_cfg);

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
