#include "Misc.h"
#include "../../Globals/Globals.h"
#include "../../Memory/Memory.h"
#include "../../Roblox/Engine/Offsets/Offsets.h"
#include "../../Roblox/Engine/Classes/Classes.h"
#include "../World/WorldVisuals.h"
#include "../Local/LocalMods.h"
#include "../../../Settings.h"
#include <Windows.h>
#include <timeapi.h>
#include <thread>
#include <atomic>
#include <chrono>
#include <cmath>

#pragma comment(lib, "winmm.lib")

#undef GetClassName

namespace {

struct MiscCache {
    std::uint64_t datamodel = 0;
    std::uint64_t lighting = 0;
    std::uint64_t root     = 0;
    std::uint64_t humanoid = 0;
    std::uint64_t character = 0;
    std::uint64_t last_refresh = 0;
};
MiscCache g_cache;

constexpr float kPi = 3.14159265358979f;

template <typename T>
bool WriteIfChanged(std::uint64_t addr, T value)
{
    const T cur = g_Memory.Read<T>(addr);
    if (cur == value) return false;
    g_Memory.Write<T>(addr, value);
    return true;
}

std::uint64_t FindServiceByClass(const char* cls)
{
    for (const auto& child : Cheat::Globals::InstanceDataModel.GetChildren())
        if (child.GetClassName() == cls)
            return child.address;
    return 0;
}

std::uint64_t LocalCharacter()
{
    if (!Cheat::Globals::Players || !g_Memory.IsValid(Cheat::Globals::Players->address))
        return 0;

    std::uint64_t local = g_Memory.Read<std::uint64_t>(
        Cheat::Globals::Players->address + Offsets::Player::LocalPlayer);
    if (!g_Memory.IsValid(local))
        return 0;

    std::uint64_t character = g_Memory.Read<std::uint64_t>(
        local + Offsets::Player::ModelInstance);
    return g_Memory.IsValid(character) ? character : 0;
}

void Refresh()
{
    const std::uint64_t now = GetTickCount64();
    if (now - g_cache.last_refresh < 400 && g_cache.last_refresh != 0)
        return;
    g_cache.last_refresh = now;

    if (g_cache.datamodel != Cheat::Globals::InstanceDataModel.address) {
        g_cache.datamodel = Cheat::Globals::InstanceDataModel.address;
        g_cache.lighting = 0;
    }

    if (!g_Memory.IsValid(g_cache.lighting))
        g_cache.lighting = FindServiceByClass("Lighting");

    std::uint64_t character = LocalCharacter();
    if (g_Memory.IsValid(character)) {
        Cheat::Instance ch(character);
        std::uint64_t root = 0, hum = 0;
        for (const auto& part : ch.GetChildren()) {
            if (!hum && part.GetClassName() == "Humanoid") hum = part.address;
            if (!root && part.GetName() == "HumanoidRootPart") root = part.address;
            if (root && hum) break;
        }
        if (!root && hum) {
            std::uint64_t r = Cheat::Humanoid(hum).GetRootPartAddress();
            if (g_Memory.IsValid(r)) root = r;
        }
        g_cache.root = root;
        g_cache.humanoid = hum;
        g_cache.character = character;
    } else {
        g_cache.root = 0;
        g_cache.humanoid = 0;
        g_cache.character = 0;
    }
}

bool CameraBasis(Vector3& fwd, Vector3& right, Vector3& up)
{
    if (!Cheat::Globals::Workspace) return false;
    auto cam = Cheat::Globals::Workspace->GetCurrentCamera();
    if (!cam) return false;

    Cheat::Camera camera(cam->address);
    Matrix4x4 r = camera.GetRotation();

    fwd   = Vector3(-r.m[0][2], -r.m[1][2], -r.m[2][2]);
    right = Vector3( r.m[0][0],  r.m[1][0],  r.m[2][0]);
    up    = Vector3( r.m[0][1],  r.m[1][1],  r.m[2][1]);
    return true;
}

void Freecam(float dt)
{
    static bool    s_active   = false;
    static bool    s_prevKey  = false;
    static bool    s_engaged  = false;
    static Vector3 s_savedOffset;
    static bool    s_savedPlatform = false;
    static bool    s_savedAutoRot  = true;
    static Vector3 s_off;
    static Vector3 s_world;

    const auto& m = Cheat::g_Settings.misc;
    const int key = m.freecam_key;

    bool want = false;
    if (key != 0) {
        const bool down = (GetAsyncKeyState(key) & 0x8000) != 0;
        if (m.freecam_mode == 1) {
            if (down && !s_prevKey) s_active = !s_active;
            want = s_active;
        } else {
            want = down;
        }
        s_prevKey = down;
    } else {
        s_active = false;
    }

    const std::uint64_t hum = g_cache.humanoid;

    if (!want || !g_Memory.IsValid(hum)) {
        if (s_engaged && g_Memory.IsValid(hum)) {
            g_Memory.Write<Vector3>(hum + Offsets::Humanoid::CameraOffset, s_savedOffset);
            g_Memory.Write<bool>(hum + Offsets::Humanoid::PlatformStand, s_savedPlatform);
            g_Memory.Write<bool>(hum + Offsets::Humanoid::AutoRotate, s_savedAutoRot);
        }
        s_engaged = false;
        return;
    }

    if (!s_engaged) {
        s_engaged       = true;
        s_savedOffset   = g_Memory.Read<Vector3>(hum + Offsets::Humanoid::CameraOffset);
        s_savedPlatform = g_Memory.Read<bool>(hum + Offsets::Humanoid::PlatformStand);
        s_savedAutoRot  = g_Memory.Read<bool>(hum + Offsets::Humanoid::AutoRotate);
        s_off           = Vector3(0.f, 0.f, 0.f);
        s_world         = Vector3(0.f, 0.f, 0.f);
    }

    g_Memory.Write<bool>(hum + Offsets::Humanoid::PlatformStand, true);
    g_Memory.Write<bool>(hum + Offsets::Humanoid::AutoRotate, false);

    if (g_Memory.IsValid(g_cache.root)) {
        Cheat::BasePart root(g_cache.root);
        root.SetAssemblyLinearVelocity(Vector3(0.f, 0.f, 0.f));
    }

    Vector3 fwd, right, up;
    if (!CameraBasis(fwd, right, up)) return;

    const Vector3 world_up(0.f, 1.f, 0.f);

    Vector3 world_dir(0.f, 0.f, 0.f);
    if (GetAsyncKeyState('W') & 0x8000) world_dir += fwd;
    if (GetAsyncKeyState('S') & 0x8000) world_dir -= fwd;
    if (GetAsyncKeyState('D') & 0x8000) world_dir += right;
    if (GetAsyncKeyState('A') & 0x8000) world_dir -= right;
    if (GetAsyncKeyState(VK_SPACE)   & 0x8000) world_dir += world_up;
    if (GetAsyncKeyState(VK_CONTROL) & 0x8000) world_dir -= world_up;

    float speed = m.freecam_speed;
    if (GetAsyncKeyState(VK_SHIFT) & 0x8000) speed *= 3.0f;

    if (world_dir.LengthSquared() > 0.01f)
        s_world += world_dir.Normalized() * (speed * dt);

    Vector3 cr_right(1.f, 0.f, 0.f), cr_up(0.f, 1.f, 0.f), cr_back(0.f, 0.f, 1.f);
    if (g_Memory.IsValid(g_cache.root)) {
        Cheat::BasePart root(g_cache.root);
        Matrix4x4 cr = root.GetRotation();
        cr_right = Vector3(cr.m[0][0], cr.m[1][0], cr.m[2][0]);
        cr_up    = Vector3(cr.m[0][1], cr.m[1][1], cr.m[2][1]);
        cr_back  = Vector3(cr.m[0][2], cr.m[1][2], cr.m[2][2]);
    }

    Vector3 local(
        s_world.Dot(cr_right),
        s_world.Dot(cr_up),
        s_world.Dot(cr_back));

    s_off = s_savedOffset + local;
    g_Memory.Write<Vector3>(hum + Offsets::Humanoid::CameraOffset, s_off);
}

}

void Cheat::Features::Misc::Tick(float dt)
{
    if (!Cheat::Globals::InstanceDataModel.address) return;

    const auto& m = Cheat::g_Settings.misc;

    Refresh();

    {
        static std::uint64_t s_scheduler    = 0;
        static bool          s_is_delay     = false;
        static double        s_orig_value   = 0.0;
        static bool          s_has_orig     = false;
        static double        s_last_written = 0.0;

        if (s_scheduler == 0) {
            static const uintptr_t base = g_Memory.GetModuleBase();
            if (base) {
                const std::uint64_t sched = g_Memory.Read<std::uint64_t>(
                    base + Offsets::TaskScheduler::Pointer);
                if (g_Memory.IsValid(sched) &&
                    g_Memory.IsWritable(sched + Offsets::TaskScheduler::MaxFPS, sizeof(double))) {

                    const double cur = g_Memory.Read<double>(
                        sched + Offsets::TaskScheduler::MaxFPS);
                    if (cur > 0.0 && cur <= 1.0) {
                        s_scheduler = sched; s_is_delay = true;
                    } else if (cur >= 10.0 && cur <= 100000.0) {
                        s_scheduler = sched; s_is_delay = false;
                    }

                }
            }
        }

        if (s_scheduler) {
            const std::uint64_t maxfps_addr = s_scheduler + Offsets::TaskScheduler::MaxFPS;
            if (m.fps_unlock) {
                if (!s_has_orig) {
                    s_orig_value = g_Memory.Read<double>(maxfps_addr);
                    s_has_orig = true;
                }

                const int cap = m.fps_cap < 30 ? 30 : m.fps_cap;
                const double target = s_is_delay
                    ? 1.0 / static_cast<double>(cap)
                    : static_cast<double>(cap);
                if (target != s_last_written &&
                    g_Memory.IsWritable(maxfps_addr, sizeof(double))) {
                    g_Memory.Write<double>(maxfps_addr, target);
                    s_last_written = target;
                }
            } else if (s_has_orig) {
                if (g_Memory.IsWritable(maxfps_addr, sizeof(double)))
                    g_Memory.Write<double>(maxfps_addr, s_orig_value);
                s_has_orig     = false;
                s_last_written = 0.0;
            }
        }
    }

    {
        static bool  s_fov_has_orig = false;
        static float s_fov_orig     = 70.0f;

        if (m.fov && Cheat::Globals::Workspace) {
            auto cam = Cheat::Globals::Workspace->GetCurrentCamera();
            if (cam) {
                const std::uint64_t fov_addr = cam->address + Offsets::Camera::FieldOfView;
                const float cur = g_Memory.Read<float>(fov_addr);

                const bool  is_radians = cur > 0.0f && cur < 3.2f;
                const float target = is_radians ? m.fov_value * (kPi / 180.0f) : m.fov_value;

                if (!s_fov_has_orig) {
                    s_fov_orig     = is_radians ? cur * (180.0f / kPi) : cur;
                    s_fov_has_orig = true;
                }

                if (std::fabs(cur - target) > 0.0005f)
                    g_Memory.Write<float>(fov_addr, target);
            }
        } else if (s_fov_has_orig && Cheat::Globals::Workspace) {
            if (auto cam = Cheat::Globals::Workspace->GetCurrentCamera()) {
                const std::uint64_t fov_addr = cam->address + Offsets::Camera::FieldOfView;
                const float cur = g_Memory.Read<float>(fov_addr);
                const bool  is_radians = cur > 0.0f && cur < 3.2f;
                const float restore = is_radians ? s_fov_orig * (kPi / 180.0f) : s_fov_orig;
                g_Memory.Write<float>(fov_addr, restore);
            }
            s_fov_has_orig = false;
        }
    }

    if (m.jump && g_Memory.IsValid(g_cache.humanoid)) {
        g_Memory.Write<bool>(g_cache.humanoid + Offsets::Humanoid::UseJumpPower, true);
        g_Memory.Write<float>(g_cache.humanoid + Offsets::Humanoid::JumpPower, m.jump_power);
    }

    if (m.walkspeed && g_Memory.IsValid(g_cache.root) && g_Memory.IsValid(g_cache.humanoid)) {
        if (g_Memory.IsValid(g_cache.root)) {
            Vector3 move_dir = g_Memory.Read<Vector3>(
                g_cache.humanoid + Offsets::Humanoid::MoveDirection);
            if (move_dir.LengthSquared() > 0.01f) {
                move_dir = move_dir.Normalized();
                BasePart root(g_cache.root);
                if (m.walkspeed_mode == 0) {
                    Vector3 pos = root.GetPosition();
                    pos += move_dir * (m.walkspeed_value * dt);
                    root.SetPosition(pos);
                } else {
                    Vector3 vel = root.GetAssemblyLinearVelocity();
                    Vector3 target(move_dir.x * m.walkspeed_value, vel.y,
                                   move_dir.z * m.walkspeed_value);
                    root.SetAssemblyLinearVelocity(target);
                }
            }
        }
    }

    if (m.fly && g_Memory.IsValid(g_cache.root)) {
        Vector3 fwd, right, up;
        if (CameraBasis(fwd, right, up)) {
            Vector3 dir(0.f, 0.f, 0.f);
            if (GetAsyncKeyState('W') & 0x8000) dir += fwd;
            if (GetAsyncKeyState('S') & 0x8000) dir -= fwd;
            if (GetAsyncKeyState('D') & 0x8000) dir += right;
            if (GetAsyncKeyState('A') & 0x8000) dir -= right;
            if (GetAsyncKeyState(VK_SPACE) & 0x8000)   dir += Vector3(0.f, 1.f, 0.f);
            if (GetAsyncKeyState(VK_CONTROL) & 0x8000) dir += Vector3(0.f, -1.f, 0.f);

            BasePart root(g_cache.root);
            const bool moving = dir.LengthSquared() > 0.01f;
            if (moving) dir = dir.Normalized();

            if (m.fly_mode == 0) {

                if (moving) {
                    Vector3 pos = root.GetPosition();
                    pos += dir * (m.fly_speed * dt);
                    root.SetPosition(pos);
                }
                root.SetAssemblyLinearVelocity(Vector3(0.f, 0.f, 0.f));
            } else {

                root.SetAssemblyLinearVelocity(dir * m.fly_speed);
            }
        }
    }

    Freecam(dt);

    WorldVisuals::Apply(g_cache.lighting);
    LocalMods::Noclip(g_cache.character, m.noclip);
    LocalMods::InfiniteJump(g_cache.humanoid, m.inf_jump);
}

namespace {
    std::thread       g_miscThread;
    std::atomic<bool> g_miscRun{ false };

    void MiscLoop()
    {
        LARGE_INTEGER freq{}, prev{};
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&prev);

        timeBeginPeriod(1);

        while (g_miscRun.load()) {
            LARGE_INTEGER now{};
            QueryPerformanceCounter(&now);
            float dt = static_cast<float>(double(now.QuadPart - prev.QuadPart) /
                                          double(freq.QuadPart));
            prev = now;
            if (dt <= 0.0f || dt > 0.1f) dt = 0.002f;

            Cheat::Features::Misc::Tick(dt);

            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }

        timeEndPeriod(1);
    }
}

void Cheat::Features::Misc::Start()
{
    if (g_miscRun.load()) return;
    g_miscRun.store(true);
    g_miscThread = std::thread(MiscLoop);
}

void Cheat::Features::Misc::Stop()
{
    g_miscRun.store(false);
    if (g_miscThread.joinable())
        g_miscThread.join();
}
