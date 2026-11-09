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

// A ".bot ..." command the add-on sends to itself under the order prefix instead of saying it: the client blocks chat sent
// from an add-on timer (Quick party adds its second and later bots a moment apart), addon messages are not blocked. Only
// ".bot " lines count; the server runs them like the same line typed in /say, with the player's own rights.
inline constexpr std::string_view kAddonCommandPrefix = ".bot ";
inline bool IsAddonBotCommand(std::string_view text)
{
    if (text.size() <= kAddonCommandPrefix.size() || text.substr(0, kAddonCommandPrefix.size()) != kAddonCommandPrefix)
        return false;
    return text.find_first_not_of(' ', kAddonCommandPrefix.size()) != std::string_view::npos;
}

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
