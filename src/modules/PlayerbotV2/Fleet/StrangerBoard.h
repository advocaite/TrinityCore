// StrangerBoard - the real players a world bot may do a small kindness for (Playerbot.World.Kindness.*): a passing priest's
// Fortitude, a mage's Intellect or a druid's Mark on a player without it, a heal from a passing priest, druid, paladin or shaman for
// a hurt player, a hand for a player losing a fight to a mob, a resurrection for a player lying dead next to the bot. Each kindness is rare: one per player per kind per cooldown, whoever does it.
//
// Pure part: the table, the picks and the cooldown board. The world thread publishes the table once a second
// (WorldBotSpawner::PublishStrangers); the idle rules (Bot/States/Rules/AmbientRules.cpp, idle:kind_*) read it from any thread.
// Tested in tests/game/ClassicStrangerBoard.cpp.
#pragma once

#include <atomic>
#include <cmath>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace Playerbot::V2::Strangers
{

enum BuffBit : std::uint8_t { kFortitude = 1, kIntellect = 2, kMark = 4 };

struct Stranger
{
    std::uint64_t guid = 0;
    std::uint64_t group = 0;           // his group's guid counter, 0 = none
    std::uint32_t map = 0;
    float x = 0.f, y = 0.f, z = 0.f;
    bool alliance = true;
    std::uint8_t level = 1;
    bool alive = true;
    bool ghost = false;                // released: running back, nothing to resurrect
    std::uint8_t hp_pct = 100;
    std::uint32_t hp = 0, max_hp = 0; // for sizing a heal to what he is missing
    bool has_mana = false;
    std::uint8_t buffs = 0;            // BuffBit he has (from anyone)
    std::uint64_t attacker = 0;        // guid counter of a creature fighting him, 0 = none (the bot finds it among its own enemies)
    std::uint8_t attacker_level = 0;
    std::uint8_t attacker_hp_pct = 0;
    bool pvp_flagged = false;          // flagged for PvP (or free-for-all): a heal would flag an unflagged healer
    bool pvp_fight = false;            // fighting a player or a player's pet, or in a duel: no stranger steps into that
};

enum class Kind : std::uint8_t { Buff, Help, Rez, Heal };   // two bits of the cooldown key

// A stranger this bot may help: same map and faction, not in the bot's group, within `range` yards.
inline bool Reachable(Stranger const& p, std::uint32_t map, bool alliance, std::uint64_t bot_group, float bx, float by, float range)
{
    return p.map == map && p.alliance == alliance && !(bot_group && p.group == bot_group)
        && std::hypot(p.x - bx, p.y - by) <= range;
}

// Who to buff: alive, out of a fight, missing the bot's buff `bit` (Intellect only for a mana user), nearest first.
inline Stranger const* PickBuffTarget(std::vector<Stranger> const& list, std::uint32_t map, bool alliance, std::uint64_t bot_group,
                                      float bx, float by, float range, std::uint8_t bit)
{
    Stranger const* best = nullptr;
    float best_d = range + 1.f;
    for (Stranger const& p : list)
    {
        if (!p.alive || p.ghost || p.attacker || (p.buffs & bit)) continue;
        if (bit == kIntellect && !p.has_mana) continue;
        if (!Reachable(p, map, alliance, bot_group, bx, by, range)) continue;
        float const d = std::hypot(p.x - bx, p.y - by);
        if (d < best_d) { best_d = d; best = &p; }
    }
    return best;
}

// Who to help: alive, at or under `hp_pct` and fighting a creature the bot can take on (no more than `level_over` above it).
inline Stranger const* PickHelpTarget(std::vector<Stranger> const& list, std::uint32_t map, bool alliance, std::uint64_t bot_group,
                                      float bx, float by, float range, std::uint8_t hp_pct, std::uint8_t bot_level, std::uint8_t level_over)
{
    Stranger const* best = nullptr;
    for (Stranger const& p : list)
    {
        if (!p.alive || !p.attacker || p.hp_pct > hp_pct) continue;
        if (p.attacker_level > bot_level + level_over) continue;
        if (!Reachable(p, map, alliance, bot_group, bx, by, range)) continue;
        if (!best || p.hp_pct < best->hp_pct) best = &p;
    }
    return best;
}

// Who to heal: alive, at or under `hp_pct` health, not in a fight with a player or a duel, and not flagged for PvP unless the bot
// already is (healing a flagged player flags the healer). A fight with a mob is fine: that is when a heal helps most. Lowest first.
inline Stranger const* PickHealTarget(std::vector<Stranger> const& list, std::uint32_t map, bool alliance, std::uint64_t bot_group,
                                      float bx, float by, float range, std::uint8_t hp_pct, bool bot_pvp)
{
    Stranger const* best = nullptr;
    for (Stranger const& p : list)
    {
        if (!p.alive || p.ghost || p.hp_pct > hp_pct || p.pvp_fight || (p.pvp_flagged && !bot_pvp)) continue;
        if (!Reachable(p, map, alliance, bot_group, bx, by, range)) continue;
        if (!best || p.hp_pct < best->hp_pct) best = &p;
    }
    return best;
}

// Who to resurrect: dead and not released.
inline Stranger const* PickRezTarget(std::vector<Stranger> const& list, std::uint32_t map, bool alliance, std::uint64_t bot_group,
                                     float bx, float by, float range)
{
    for (Stranger const& p : list)
        if (!p.alive && !p.ghost && Reachable(p, map, alliance, bot_group, bx, by, range))
            return &p;
    return nullptr;
}

// One kindness of a kind per player per cooldown, whoever does it.
class Cooldowns
{
public:
    bool Take(std::uint64_t player, Kind k, std::uint32_t now_ms, std::uint32_t cooldown_ms)
    {
        std::lock_guard<std::mutex> g(mtx_);
        std::uint64_t const key = (player << 2) | std::uint64_t(k);
        auto it = last_.find(key);
        if (it != last_.end() && now_ms - it->second < cooldown_ms)
            return false;
        if (last_.size() > 4096)
            last_.clear();
        last_[key] = now_ms ? now_ms : 1u;
        return true;
    }

private:
    std::mutex mtx_;
    std::unordered_map<std::uint64_t, std::uint32_t> last_;
};

class Board
{
public:
    static Board& Get()
    {
        static Board b;
        return b;
    }
    void Publish(std::vector<Stranger> list)
    {
        auto next = std::make_shared<std::vector<Stranger> const>(std::move(list));
        std::lock_guard<std::mutex> g(mtx_);
        cur_ = std::move(next);
    }
    std::shared_ptr<std::vector<Stranger> const> Current() const
    {
        std::lock_guard<std::mutex> g(mtx_);
        return cur_;
    }
    Cooldowns cooldowns;
    std::atomic<std::uint32_t> buffs{0}, helps{0}, rezzes{0}, heals{0};
    std::atomic<std::uint32_t> cooldown_skips{0};   // a buff found a player another bot buffed within BuffCooldownSec
    std::atomic<std::uint32_t> cast_fails{0};       // the buff cast was refused (range, sight, facing, GCD)

private:
    mutable std::mutex mtx_;
    std::shared_ptr<std::vector<Stranger> const> cur_ = std::make_shared<std::vector<Stranger> const>();
};

} // namespace Playerbot::V2::Strangers
