// Who still has to be told where to get the ForeverBots add-on: a player is noted at login and dropped as soon as the add-on
// says hello (a self-whisper under the order prefix) or the player leaves. Whoever is still noted after the grace period is due
// once, then forgotten, so each login produces at most one hint. Pure bookkeeping: no Player, no clock, no config.
#pragma once

#include <algorithm>
#include <cstdint>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Playerbot::V2 {

// Text of the add-on's greeting to the server.
inline constexpr std::string_view kAddonHelloText = "hello";

class AddonHintTracker
{
public:
    void Login(uint64_t player, uint32_t now_ms, uint32_t delay_ms) { due_[player] = now_ms + delay_ms; }
    void Hello(uint64_t player) { due_.erase(player); }
    void Logout(uint64_t player) { due_.erase(player); }
    bool Pending(uint64_t player) const { return due_.count(player) != 0; }
    size_t Size() const { return due_.size(); }

    // Players whose grace period has run out; they are removed (one hint per login). Wrap-safe for the 32-bit millisecond clock.
    std::vector<uint64_t> TakeDue(uint32_t now_ms)
    {
        std::vector<uint64_t> out;
        for (auto it = due_.begin(); it != due_.end();)
        {
            if (int32_t(now_ms - it->second) >= 0)
            {
                out.push_back(it->first);
                it = due_.erase(it);
            }
            else
                ++it;
        }
        std::sort(out.begin(), out.end());
        return out;
    }

private:
    std::unordered_map<uint64_t, uint32_t> due_;
};

}  // namespace Playerbot::V2
