#include "WorldVisuals.h"
#include "../../Memory/Memory.h"
#include "../../Roblox/Engine/Offsets/Offsets.h"
#include "../../Roblox/Engine/Classes/Classes.h"
#include "../../../Settings.h"
#include <algorithm>

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
            static bool         s_engaged = false;
            static std::uint8_t s_saved_shadows = 1;
            static float        s_saved_brightness = 1.0f;
            static float        s_saved_exposure = 0.0f;
            static float        s_saved_env_diff = 1.0f;
            static Color3       s_saved_ambient;
            static Color3       s_saved_outdoor;

            if (!g_Memory.IsValid(lighting)) return;

            const auto& w = Cheat::g_Settings.world;
            bool dirty = false;

            if (w.no_shadow) {
                if (!s_engaged) {
                    s_engaged          = true;
                    s_saved_shadows    = g_Memory.Read<std::uint8_t>(lighting + Offsets::Lighting::GlobalShadows);
                    s_saved_brightness = g_Memory.Read<float>(lighting + Offsets::Lighting::Brightness);
                    s_saved_exposure   = g_Memory.Read<float>(lighting + Offsets::Lighting::ExposureCompensation);
                    s_saved_env_diff   = g_Memory.Read<float>(lighting + Offsets::Lighting::EnvironmentDiffuseScale);
                    s_saved_ambient    = g_Memory.Read<Color3>(lighting + Offsets::Lighting::Ambient);
                    s_saved_outdoor    = g_Memory.Read<Color3>(lighting + Offsets::Lighting::OutdoorAmbient);
                }

                const float t     = (std::max)(0.f, w.brightness);
                const float bri   = t * 2.5f;
                const float expo  = t * 0.35f;
                const float amb_s = 1.f + t * 0.15f;
                const float env_d = 1.f + t * 0.2f;

                dirty |= WriteIfChanged<std::uint8_t>(lighting + Offsets::Lighting::GlobalShadows, 0);
                dirty |= WriteIfChanged<float>(lighting + Offsets::Lighting::Brightness, bri);
                dirty |= WriteIfChanged<float>(lighting + Offsets::Lighting::ExposureCompensation, expo);
                dirty |= WriteIfChanged<float>(lighting + Offsets::Lighting::EnvironmentDiffuseScale, env_d);
                dirty |= WriteIfChanged<Color3>(lighting + Offsets::Lighting::Ambient,
                    Color3(amb_s, amb_s, amb_s));
                dirty |= WriteIfChanged<Color3>(lighting + Offsets::Lighting::OutdoorAmbient,
                    Color3(amb_s, amb_s, amb_s));
            } else if (s_engaged) {
                s_engaged = false;
                WriteIfChanged<std::uint8_t>(lighting + Offsets::Lighting::GlobalShadows, s_saved_shadows);
                WriteIfChanged<float>(lighting + Offsets::Lighting::Brightness, s_saved_brightness);
                WriteIfChanged<float>(lighting + Offsets::Lighting::ExposureCompensation, s_saved_exposure);
                WriteIfChanged<float>(lighting + Offsets::Lighting::EnvironmentDiffuseScale, s_saved_env_diff);
                WriteIfChanged<Color3>(lighting + Offsets::Lighting::Ambient, s_saved_ambient);
                WriteIfChanged<Color3>(lighting + Offsets::Lighting::OutdoorAmbient, s_saved_outdoor);
                dirty = true;
            }

            if (w.fog) {
                const float start = (std::max)(0.f, w.fog_start);
                const float end   = (std::max)(start + 1.f, w.fog_end);
                dirty |= WriteIfChanged<float>(lighting + Offsets::Lighting::FogStart, start);
                dirty |= WriteIfChanged<float>(lighting + Offsets::Lighting::FogEnd, end);
                dirty |= WriteIfChanged<Color3>(lighting + Offsets::Lighting::FogColor,
                    Color3(w.fog_color[0], w.fog_color[1], w.fog_color[2]));
            }

            if (dirty) Invalidate();
        }

    }
}
