#pragma once
#include <string>
#include <memory>
#include <unordered_map>
#include <cstdint>
#include <thread>
#include <atomic>
#include <chrono>
#include <mutex>
#include "../Roblox/Engine/Classes/Classes.h"

namespace Cheat {

    struct PlayerCache
    {
        std::string name;
        std::string displayName;
        std::string toolName;
        std::uint64_t address;
        bool isR6 = false;

        std::shared_ptr<Instance> head;
        std::shared_ptr<Instance> humanoidRootPart;
        std::shared_ptr<Instance> upperTorso;
        std::shared_ptr<Instance> lowerTorso;
        std::shared_ptr<Instance> leftUpperArm;
        std::shared_ptr<Instance> leftLowerArm;
        std::shared_ptr<Instance> leftHand;
        std::shared_ptr<Instance> rightUpperArm;
        std::shared_ptr<Instance> rightLowerArm;
        std::shared_ptr<Instance> rightHand;
        std::shared_ptr<Instance> leftUpperLeg;
        std::shared_ptr<Instance> leftLowerLeg;
        std::shared_ptr<Instance> leftFoot;
        std::shared_ptr<Instance> rightUpperLeg;
        std::shared_ptr<Instance> rightLowerLeg;
        std::shared_ptr<Instance> rightFoot;
        std::shared_ptr<Instance> humanoid;

        PlayerCache() : address(0) {}
        explicit PlayerCache(std::uint64_t addr) : address(addr) {}
    };

    class PlayerHandler
    {
    public:
        PlayerHandler() = default;
        ~PlayerHandler() = default;

        static void UpdateCache(const Instance& player,
                                std::unordered_map<std::uint64_t, PlayerCache>& target);

        static void CacheAllPlayers();
        static PlayerCache GetCachedPlayer(std::uint64_t address);
        static bool IsPlayerCached(std::uint64_t address);
        static void ClearCache();
        static void RemoveFromCache(std::uint64_t address);

        static void StartCacheThread();
        static void StopCacheThread();
        static std::unordered_map<std::uint64_t, PlayerCache> GetPlayerCache();

        template<typename F>
        static void ForEachPlayer(F&& fn)
        {
            std::lock_guard<std::mutex> lock(cacheMutex);
            for (auto const& [address, cache] : playerCache)
                fn(cache);
        }

        static std::size_t GetPlayerCount();

    private:
        static std::unordered_map<std::uint64_t, PlayerCache> playerCache;
        static std::thread cacheThread;
        static std::atomic<bool> shouldRun;
        static std::mutex cacheMutex;
        static void CacheThreadLoop();
    };

}
