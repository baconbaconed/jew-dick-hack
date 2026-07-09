#include "PlayerHandler.h"
#include <thread>
#include <chrono>
#include "../Roblox/Engine/Offsets/Offsets.h"
#include "../Memory/Memory.h"
#include "../Globals/Globals.h"
#include <iostream>
#include <mutex>

#undef GetClassName

namespace Cheat {
    std::unordered_map<std::uint64_t, PlayerCache> PlayerHandler::playerCache;
    std::thread PlayerHandler::cacheThread;
    std::atomic<bool> PlayerHandler::shouldRun = false;
    std::mutex PlayerHandler::cacheMutex;
}

namespace {

bool PopulatePartsFromChildren(const std::vector<Cheat::Instance>& parts, Cheat::PlayerCache& cache)
{
    std::uint64_t humanoidAddr = 0;

    for (const auto& part : parts)
    {
        std::string partClass = part.GetClassName();
        if (partClass == "Accessory") continue;
        if (partClass == "Tool") { cache.toolName = part.GetName(); continue; }

        if (partClass == "Humanoid") {
            cache.humanoid = std::make_shared<Cheat::Instance>(part);
            humanoidAddr = part.address;
        }

        std::string partName = part.GetName();

        if (partName == "Head") cache.head = std::make_shared<Cheat::Instance>(part);
        else if (partName == "HumanoidRootPart") cache.humanoidRootPart = std::make_shared<Cheat::Instance>(part);

        else if (partName == "UpperTorso") cache.upperTorso = std::make_shared<Cheat::Instance>(part);
        else if (partName == "LowerTorso") cache.lowerTorso = std::make_shared<Cheat::Instance>(part);
        else if (partName == "LeftUpperArm") cache.leftUpperArm = std::make_shared<Cheat::Instance>(part);
        else if (partName == "LeftLowerArm") cache.leftLowerArm = std::make_shared<Cheat::Instance>(part);
        else if (partName == "LeftHand") cache.leftHand = std::make_shared<Cheat::Instance>(part);
        else if (partName == "RightUpperArm") cache.rightUpperArm = std::make_shared<Cheat::Instance>(part);
        else if (partName == "RightLowerArm") cache.rightLowerArm = std::make_shared<Cheat::Instance>(part);
        else if (partName == "RightHand") cache.rightHand = std::make_shared<Cheat::Instance>(part);
        else if (partName == "LeftUpperLeg") cache.leftUpperLeg = std::make_shared<Cheat::Instance>(part);
        else if (partName == "LeftLowerLeg") cache.leftLowerLeg = std::make_shared<Cheat::Instance>(part);
        else if (partName == "LeftFoot") cache.leftFoot = std::make_shared<Cheat::Instance>(part);
        else if (partName == "RightUpperLeg") cache.rightUpperLeg = std::make_shared<Cheat::Instance>(part);
        else if (partName == "RightLowerLeg") cache.rightLowerLeg = std::make_shared<Cheat::Instance>(part);
        else if (partName == "RightFoot") cache.rightFoot = std::make_shared<Cheat::Instance>(part);

        else if (partName == "Torso") { cache.upperTorso = std::make_shared<Cheat::Instance>(part); cache.isR6 = true; }
        else if (partName == "Left Arm") cache.leftUpperArm = std::make_shared<Cheat::Instance>(part);
        else if (partName == "Right Arm") cache.rightUpperArm = std::make_shared<Cheat::Instance>(part);
        else if (partName == "Left Leg") cache.leftUpperLeg = std::make_shared<Cheat::Instance>(part);
        else if (partName == "Right Leg") cache.rightUpperLeg = std::make_shared<Cheat::Instance>(part);
    }

    if (!cache.humanoidRootPart && humanoidAddr) {
        std::uint64_t root = Cheat::Humanoid(humanoidAddr).GetRootPartAddress();
        if (g_Memory.IsValid(root))
            cache.humanoidRootPart = std::make_shared<Cheat::Instance>(root);
    }

    return static_cast<bool>(cache.humanoidRootPart) || humanoidAddr != 0;
}

bool PopulateParts(std::uint64_t characterAddress, Cheat::PlayerCache& cache)
{
    if (!g_Memory.IsValid(characterAddress)) return false;
    return PopulatePartsFromChildren(Cheat::Instance(characterAddress).GetChildren(), cache);
}

std::uint64_t LocalCharacterAddress()
{
    if (!Cheat::Globals::Players || !g_Memory.IsValid(Cheat::Globals::Players->address))
        return 0;

    std::uint64_t local = g_Memory.Read<std::uint64_t>(
        Cheat::Globals::Players->address + Offsets::Player::LocalPlayer);
    if (!g_Memory.IsValid(local))
        return 0;

    return g_Memory.Read<std::uint64_t>(local + Offsets::Player::ModelInstance);
}

constexpr int kMaxDepth   = 12;
constexpr int kNodeBudget = 40000;

bool IsIgnoredService(const std::string& cls)
{
    return cls == "ReplicatedStorage" || cls == "ServerStorage" ||
           cls == "ReplicatedFirst"   || cls == "StarterPlayer" ||
           cls == "StarterPack"       || cls == "StarterGui" ||
           cls == "CoreGui"           || cls == "Chat" ||
           cls == "MaterialService"   || cls == "Lighting" ||
           cls == "SoundService"      || cls == "TextChatService";
}

bool IsLeafPartClass(const std::string& cls)
{
    return cls == "Part" || cls == "MeshPart" || cls == "UnionOperation" ||
           cls == "NegateOperation" || cls == "IntersectOperation" ||
           cls == "TrussPart" || cls == "WedgePart" || cls == "CornerWedgePart" ||
           cls == "Seat" || cls == "VehicleSeat" || cls == "SpawnLocation" ||
           cls == "Terrain";
}

void AddCharacter(const Cheat::Instance& model, Cheat::PlayerCache&& cache,
                  std::uint64_t localChar,
                  std::unordered_map<std::uint64_t, Cheat::PlayerCache>& target)
{
    if (model.address == localChar) return;

    cache.name = model.GetName();
    if (cache.name.empty()) cache.name = "unknown";

    if (cache.humanoid) {
        std::string dn = Cheat::Humanoid(cache.humanoid->address).GetDisplayName();
        cache.displayName = (dn.empty() || dn == "Unknown") ? cache.name : dn;
    } else {
        cache.displayName = cache.name;
    }

    target[model.address] = std::move(cache);
}

void ScanNode(const Cheat::Instance& node, int depth, int& budget,
              std::uint64_t localChar,
              std::unordered_map<std::uint64_t, Cheat::PlayerCache>& target)
{
    if (depth > kMaxDepth || budget <= 0) return;

    auto children = node.GetChildren();

    {
        Cheat::PlayerCache probe(node.address);
        if (PopulatePartsFromChildren(children, probe)) {
            AddCharacter(node, std::move(probe), localChar, target);
            return;
        }
    }

    for (const auto& child : children) {
        if (--budget <= 0) return;
        if (IsLeafPartClass(child.GetClassName())) continue;
        ScanNode(child, depth + 1, budget, localChar, target);
    }
}

void ScanTree(const Cheat::Instance& root, bool skipStorageServices,
              std::uint64_t localChar,
              std::unordered_map<std::uint64_t, Cheat::PlayerCache>& target)
{
    int budget = kNodeBudget;
    if (!skipStorageServices) {
        ScanNode(root, 0, budget, localChar, target);
        return;
    }

    for (const auto& service : root.GetChildren()) {
        if (budget <= 0) break;
        if (IsIgnoredService(service.GetClassName())) continue;
        ScanNode(service, 1, budget, localChar, target);
    }
}

void CacheFromWorkspace(std::unordered_map<std::uint64_t, Cheat::PlayerCache>& target)
{
    const std::uint64_t localChar = LocalCharacterAddress();

    if (Cheat::Globals::Workspace && g_Memory.IsValid(Cheat::Globals::Workspace->address))
        ScanTree(*Cheat::Globals::Workspace, false, localChar, target);

    if (target.empty() && g_Memory.IsValid(Cheat::Globals::InstanceDataModel.address))
        ScanTree(Cheat::Globals::InstanceDataModel, true, localChar, target);
}

std::uint64_t FindServiceByClass(const Cheat::Instance& dm, const char* cls)
{
    for (const auto& child : dm.GetChildren())
        if (child.GetClassName() == cls)
            return child.address;
    return 0;
}

bool LooksLikeDataModel(std::uint64_t dm)
{
    if (!dm) return false;
    Cheat::Instance inst(dm);
    if (inst.GetClassName() == "DataModel") return true;
    const std::string name = inst.GetName();
    return name == "Game" || name == "Ugc" || name == "LuaApp";
}

std::uint64_t ResolveDataModel()
{
    const uintptr_t base = g_Memory.GetModuleBase();
    if (!base) return 0;

    const std::uint64_t front = g_Memory.Read<std::uint64_t>(base + Offsets::FakeDataModel::Pointer);
    if (front) {
        Cheat::Globals::FrontDataModel = front;
        std::uint64_t dm = g_Memory.Read<std::uint64_t>(front + Offsets::FakeDataModel::RealDataModel);
        if (LooksLikeDataModel(dm))    return dm;
        if (LooksLikeDataModel(front)) return front;
    }

    const std::uint64_t ve = g_Memory.Read<std::uint64_t>(base + Offsets::VisualEngine::Pointer);
    if (ve) {
        const std::uint64_t fake = g_Memory.Read<std::uint64_t>(ve + Offsets::VisualEngine::FakeDataModel);
        if (fake) {
            std::uint64_t dm = g_Memory.Read<std::uint64_t>(fake + Offsets::FakeDataModel::RealDataModel);
            if (LooksLikeDataModel(dm))   return dm;
            dm = g_Memory.Read<std::uint64_t>(fake + Offsets::RenderJob::RealDataModel);
            if (LooksLikeDataModel(dm))   return dm;
            if (LooksLikeDataModel(fake)) return fake;
        }
    }

    static ULONGLONG s_last_diag = 0;
    const ULONGLONG now = GetTickCount64();
    if (now - s_last_diag > 3000) {
        s_last_diag = now;
        const std::uint64_t dm1 = front
            ? g_Memory.Read<std::uint64_t>(front + Offsets::FakeDataModel::RealDataModel) : 0;
        std::cout << "[cache] waiting for datamodel... front=0x" << std::hex << front
                  << " dm=0x" << dm1 << " ve=0x" << ve << std::dec
                  << " cls='" << Cheat::Instance(dm1).GetClassName()
                  << "' (stale offsets?)\n";
    }
    return 0;
}

void RefreshGlobals()
{
    const std::uint64_t dm = ResolveDataModel();
    if (!dm) return;

    if (dm != Cheat::Globals::InstanceDataModel.address) {
        Cheat::Globals::InstanceDataModel.address = dm;
        Cheat::Globals::Workspace = nullptr;
        Cheat::Globals::Players   = nullptr;
        std::cout << "[cache] datamodel: 0x" << std::hex << dm << std::dec << "\n";
    }

    if (!Cheat::Globals::Workspace) {
        if (std::uint64_t ws = FindServiceByClass(Cheat::Globals::InstanceDataModel, "Workspace")) {
            Cheat::Globals::Workspace = std::make_shared<Cheat::Workspace>(ws);
            std::cout << "[cache] workspace found\n";
        }
    }
    if (!Cheat::Globals::Players) {
        if (std::uint64_t pl = FindServiceByClass(Cheat::Globals::InstanceDataModel, "Players")) {
            Cheat::Globals::Players = std::make_shared<Cheat::Players>(pl);
            std::cout << "[cache] players service found\n";
        }
    }
}

}

void Cheat::PlayerHandler::UpdateCache(const Instance& player,
                                       std::unordered_map<std::uint64_t, PlayerCache>& target)
{
    if (!g_Memory.IsValid(player.address)) return;

    std::uint64_t characterAddress = g_Memory.Read<std::uint64_t>(player.address + Offsets::Player::ModelInstance);
    if (!g_Memory.IsValid(characterAddress)) return;

    PlayerCache cache(player.address);
    cache.name = player.GetName();
    if (cache.name.empty()) return;

    cache.displayName = Player(player.address).GetDisplayName();
    if (cache.displayName.empty() || cache.displayName == "Unknown")
        cache.displayName = cache.name;

    PopulateParts(characterAddress, cache);

    target[player.address] = std::move(cache);
}

Cheat::PlayerCache Cheat::PlayerHandler::GetCachedPlayer(std::uint64_t address)
{
    std::lock_guard<std::mutex> lock(cacheMutex);
    auto it = playerCache.find(address);
    if (it != playerCache.end())
        return it->second;
    return {};
}

std::unordered_map<std::uint64_t, Cheat::PlayerCache> Cheat::PlayerHandler::GetPlayerCache()
{
    std::lock_guard<std::mutex> lock(cacheMutex);
    return playerCache;
}

void Cheat::PlayerHandler::CacheAllPlayers()
{

    if (!Globals::InstanceDataModel.address)
        return;

    std::unordered_map<std::uint64_t, PlayerCache> fresh;

    if (Globals::Players && g_Memory.IsValid(Globals::Players->address))
    {
        auto players = Globals::Players->GetChildren();
        fresh.reserve(players.size());
        for (const auto& player : players)
        {
            UpdateCache(player, fresh);
        }
    }

    bool any_parts = false;
    for (const auto& [addr, cache] : fresh) {
        if (cache.humanoidRootPart) { any_parts = true; break; }
    }

    const std::size_t players_found = fresh.size();
    bool used_fallback = false;
    if (!any_parts) {
        fresh.clear();
        CacheFromWorkspace(fresh);
        used_fallback = true;
    }

    {
        static std::size_t s_last_count = SIZE_MAX;
        static bool        s_last_fallback = false;
        if (fresh.size() != s_last_count || used_fallback != s_last_fallback) {
            if (used_fallback)
                std::cout << "[cache] players-service empty (" << players_found
                          << ") -> workspace scan found " << fresh.size() << " characters\n";
            else
                std::cout << "[cache] players service: " << fresh.size() << " characters\n";
            s_last_count = fresh.size();
            s_last_fallback = used_fallback;
        }
    }

    std::lock_guard<std::mutex> lock(cacheMutex);
    playerCache.swap(fresh);
}

void Cheat::PlayerHandler::CacheThreadLoop()
{
    while (shouldRun.load())
    {
        RefreshGlobals();
        CacheAllPlayers();
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
}

std::size_t Cheat::PlayerHandler::GetPlayerCount()
{
    std::lock_guard<std::mutex> lock(cacheMutex);
    return playerCache.size();
}

void Cheat::PlayerHandler::StartCacheThread()
{
    if (shouldRun.load()) return;
    shouldRun.store(true);
    cacheThread = std::thread(CacheThreadLoop);
}

void Cheat::PlayerHandler::StopCacheThread()
{
    if (!shouldRun.load()) return;
    shouldRun.store(false);
    if (cacheThread.joinable())
        cacheThread.join();
}
