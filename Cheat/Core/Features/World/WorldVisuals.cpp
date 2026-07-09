#include "WorldVisuals.h"
#include "../../Memory/Memory.h"
#include "../../Roblox/Engine/Offsets/Offsets.h"
#include "../../Roblox/Engine/Classes/Classes.h"
#include "../../../Settings.h"
#include <cmath>

namespace Cheat {
    namespace Features {

        namespace {
            std::uint64_t RenderView()
            {
                static const uintptr_t base = g_Memory.GetModuleBase();
                if (!base) return 0;
                const std::uint64_t ve = g_Memory.Read<std::uint64_t>(base + Offsets::VisualEngine::Pointer);
                if (!g_Memory.IsValid(ve)) return 0;
                const std::uint64_t rv = g_Memory.Read<std::uint64_t>(ve + Offsets::VisualEngine::RenderView);
                return g_Memory.IsValid(rv) ? rv : 0;
            }

            void Invalidate()
            {
                if (const std::uint64_t rv = RenderView())
                    g_Memory.Write<std::uint8_t>(rv + Offsets::RenderView::LightingValid, 0);
            }

            template <typename T>
            bool WriteIfChanged(std::uint64_t addr, T value)
            {
                if (!g_Memory.IsValid(addr)) return false;
                const T cur = g_Memory.Read<T>(addr);
                if (cur == value) return false;
                g_Memory.Write<T>(addr, value);
                return true;
            }
        }

        void WorldVisuals::Apply(std::uint64_t lighting)
        {
            static bool         s_fb_engaged = false;
            static std::uint8_t s_saved_shadows = 1;
            static float        s_saved_brightness = 1.0f;
            static Color3       s_saved_ambient;
            static Color3       s_saved_outdoor;

            if (!g_Memory.IsValid(lighting)) return;

            const auto& v = Cheat::g_Settings.wvis;
            bool dirty = false;

            if (v.fullbright) {
                if (!s_fb_engaged) {
                    s_fb_engaged       = true;
                    s_saved_shadows    = g_Memory.Read<std::uint8_t>(lighting + Offsets::Lighting::GlobalShadows);
                    s_saved_brightness = g_Memory.Read<float>(lighting + Offsets::Lighting::Brightness);
                    s_saved_ambient    = g_Memory.Read<Color3>(lighting + Offsets::Lighting::Ambient);
                    s_saved_outdoor    = g_Memory.Read<Color3>(lighting + Offsets::Lighting::OutdoorAmbient);
                }
                dirty |= WriteIfChanged<std::uint8_t>(lighting + Offsets::Lighting::GlobalShadows, 0);
                dirty |= WriteIfChanged<float>(lighting + Offsets::Lighting::Brightness, v.brightness);
                dirty |= WriteIfChanged<Color3>(lighting + Offsets::Lighting::Ambient,
                    Color3(1.f, 1.f, 1.f));
                dirty |= WriteIfChanged<Color3>(lighting + Offsets::Lighting::OutdoorAmbient,
                    Color3(1.f, 1.f, 1.f));
            } else {
                if (s_fb_engaged) {
                    s_fb_engaged = false;
                    WriteIfChanged<std::uint8_t>(lighting + Offsets::Lighting::GlobalShadows, s_saved_shadows);
                    WriteIfChanged<float>(lighting + Offsets::Lighting::Brightness, s_saved_brightness);
                    WriteIfChanged<Color3>(lighting + Offsets::Lighting::Ambient, s_saved_ambient);
                    WriteIfChanged<Color3>(lighting + Offsets::Lighting::OutdoorAmbient, s_saved_outdoor);
                    dirty = true;
                }
                if (v.ambient)
                    dirty |= WriteIfChanged<Color3>(lighting + Offsets::Lighting::Ambient,
                        Color3(v.ambient_color[0], v.ambient_color[1], v.ambient_color[2]));
                if (v.outdoor)
                    dirty |= WriteIfChanged<Color3>(lighting + Offsets::Lighting::OutdoorAmbient,
                        Color3(v.outdoor_color[0], v.outdoor_color[1], v.outdoor_color[2]));
            }

            if (v.fog)
                dirty |= WriteIfChanged<float>(lighting + Offsets::Lighting::FogEnd, v.fog_end);
            if (v.fog_color_on)
                dirty |= WriteIfChanged<Color3>(lighting + Offsets::Lighting::FogColor,
                    Color3(v.fog_color[0], v.fog_color[1], v.fog_color[2]));

            if (dirty) Invalidate();
        }

    }
}
