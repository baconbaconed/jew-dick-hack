#define NOMINMAX
#include "Console.h"
#include "../Globals/Globals.h"
#include "../Memory/Memory.h"
#include "../Roblox/Engine/Offsets/Offsets.h"
#include "../PlayerHandler/PlayerHandler.h"
#include <Windows.h>
#include <cstdio>
#include <cstdarg>
#include <mutex>

namespace Cheat::Console {
    namespace {
        std::mutex g_mu;
        bool g_vt = false;

        void ensure_console() {
            static bool once = false;
            if (once) return;
            once = true;
            HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
            if (!h || h == INVALID_HANDLE_VALUE) return;
            DWORD mode = 0;
            if (GetConsoleMode(h, &mode)) {
                mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
                g_vt = SetConsoleMode(h, mode) != 0;
            }
        }

        void stamp(char* out, size_t n) {
            SYSTEMTIME st{};
            GetLocalTime(&st);
            std::snprintf(out, n, "[%02u:%02u:%02u]",
                (unsigned)st.wHour, (unsigned)st.wMinute, (unsigned)st.wSecond);
        }

        WORD attr(Color c) {
            return static_cast<WORD>(c);
        }

        void write_colored(Color color, const char* text) {
            ensure_console();
            HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
            if (h && h != INVALID_HANDLE_VALUE) {
                CONSOLE_SCREEN_BUFFER_INFO csbi{};
                GetConsoleScreenBufferInfo(h, &csbi);
                SetConsoleTextAttribute(h, attr(color));
                std::fputs(text, stdout);
                SetConsoleTextAttribute(h, csbi.wAttributes);
            } else {
                std::fputs(text, stdout);
            }
        }

        void log_locked(Color color, const char* body) {
            char t[16]{};
            stamp(t, sizeof(t));
            write_colored(Color::Dim, t);
            write_colored(Color::Dim, ": ");
            write_colored(color, body);
            std::fputc('\n', stdout);
            std::fflush(stdout);
        }
    }

    void Clear() {
        std::lock_guard<std::mutex> lock(g_mu);
        ensure_console();
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
        if (!h || h == INVALID_HANDLE_VALUE) return;

        CONSOLE_SCREEN_BUFFER_INFO csbi{};
        if (!GetConsoleScreenBufferInfo(h, &csbi)) return;

        const DWORD cells = (DWORD)csbi.dwSize.X * (DWORD)csbi.dwSize.Y;
        DWORD written = 0;
        COORD home{ 0, 0 };
        FillConsoleOutputCharacterA(h, ' ', cells, home, &written);
        FillConsoleOutputAttribute(h, csbi.wAttributes, cells, home, &written);
        SetConsoleCursorPosition(h, home);
    }

    void Log(Color color, const char* fmt, ...) {
        if (!fmt) return;
        char body[640]{};
        va_list ap;
        va_start(ap, fmt);
        std::vsnprintf(body, sizeof(body), fmt, ap);
        va_end(ap);
        std::lock_guard<std::mutex> lock(g_mu);
        log_locked(color, body);
    }

    void Log(const char* fmt, ...) {
        if (!fmt) return;
        char body[640]{};
        va_list ap;
        va_start(ap, fmt);
        std::vsnprintf(body, sizeof(body), fmt, ap);
        va_end(ap);
        std::lock_guard<std::mutex> lock(g_mu);
        log_locked(Color::White, body);
    }

    void Ptr(Color color, const char* name, std::uint64_t addr) {
        Log(color, "%-14s 0x%llX", name ? name : "?", (unsigned long long)addr);
    }

    void Ptr(const char* name, std::uint64_t addr) {
        Ptr(Color::Cyan, name, addr);
    }

    void DumpWorld() {
        const uintptr_t base = g_Memory.GetModuleBase();
        const DWORD pid = g_Memory.GetPID();
        const std::uint64_t ve = base
            ? g_Memory.Read<std::uint64_t>(base + Offsets::VisualEngine::Pointer) : 0;
        const std::uint64_t front = Globals::FrontDataModel;
        const std::uint64_t dm = Globals::InstanceDataModel.address;
        const std::uint64_t ws = Globals::Workspace ? Globals::Workspace->address : 0;
        const std::uint64_t pl = Globals::Players ? Globals::Players->address : 0;

        std::uint64_t local_player = 0;
        std::uint64_t local_char = 0;
        std::uint64_t camera = 0;
        std::int64_t place_id = 0;
        std::int64_t game_id = 0;
        std::int64_t user_id = 0;
        std::uint64_t world = 0;

        if (dm) {
            place_id = g_Memory.Read<std::int64_t>(dm + Offsets::DataModel::PlaceId);
            game_id  = g_Memory.Read<std::int64_t>(dm + Offsets::DataModel::GameId);
        }
        if (pl) {
            local_player = g_Memory.Read<std::uint64_t>(pl + Offsets::Player::LocalPlayer);
            if (g_Memory.IsValid(local_player)) {
                local_char = g_Memory.Read<std::uint64_t>(
                    local_player + Offsets::Player::ModelInstance);
                user_id = g_Memory.Read<std::int64_t>(
                    local_player + Offsets::Player::UserId);
            }
        }
        if (ws) {
            camera = g_Memory.Read<std::uint64_t>(ws + Offsets::Workspace::CurrentCamera);
            world  = g_Memory.Read<std::uint64_t>(ws + Offsets::Workspace::World);
        }

        const std::size_t players = PlayerHandler::GetPlayerCount();

        Log(Color::White,   "jewsploit status");
        Log(Color::Gray,    "PID            %lu", (unsigned long)pid);
        Ptr(Color::Cyan,    "Module", base);
        Ptr(Color::Yellow,  "Front DM", front);
        Ptr(Color::Green,   "Data Model", dm);
        Ptr(Color::Magenta, "Render View", ve);
        Ptr(Color::Blue,    "Workspace", ws);
        Ptr(Color::Teal,    "World", world);
        Ptr(Color::Sky,     "Players", pl);
        Ptr(Color::Lime,    "LocalPlayer", local_player);
        Ptr(Color::Purple,  "Character", local_char);
        Ptr(Color::Pink,    "Camera", camera);
        Log(Color::Orange,  "PlaceId        %lld", (long long)place_id);
        Log(Color::Yellow,  "GameId         %lld", (long long)game_id);
        Log(Color::Lime,    "UserId         %lld", (long long)user_id);
        Log(Color::White,   "Cached         %zu players", players);
    }

    void DumpSilent(bool ok, std::uint64_t handler, std::uint64_t stub,
                    std::uint64_t state, std::uint64_t slot, const char* detail) {
        if (ok) {
            Log(Color::Green, "Silent inject  ok");
            Ptr(Color::Magenta, "Handler", handler);
            Ptr(Color::Yellow,  "Stub", stub);
            Ptr(Color::Cyan,    "State", state);
            Ptr(Color::Blue,    "Slot", slot);
            Ptr(Color::Teal,    "Desc RVA", Offsets::WorldRoot::RaycastBoundDesc);
            Log(Color::Gray,    "Fn off         0x%llX",
                (unsigned long long)Offsets::WorldRoot::RaycastBoundFn);
        } else {
            Log(Color::Red, "Silent inject  fail%s%s",
                detail && detail[0] ? " " : "",
                detail ? detail : "");
            if (handler) Ptr(Color::Magenta, "Handler", handler);
            if (stub)    Ptr(Color::Yellow,  "Stub", stub);
            if (slot)    Ptr(Color::Blue,    "Slot", slot);
        }
    }
}
