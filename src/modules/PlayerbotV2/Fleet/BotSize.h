/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PLAYERBOT_V2_FLEET_BOT_SIZE_H
#define PLAYERBOT_V2_FLEET_BOT_SIZE_H

// Plain bot numbers (Karrz 2026-11-01). The player's file has at most these lines:
//   Bots.Size      = Low | Normal | High     fills the four below; most people need only this
//                  = Off                     no world bots at all (own bots, dungeon finder and battleground bots still come);
//                                            the four below are then not used (the launcher's Bots window, 0.1.29)
//   Bots.Reserve   = 3000                    bot characters kept ready, logged out, balanced by faction and level
//   Bots.ActiveCap = 500                     most bots logged in at once, for everyone together
//   Bots.PerZone   = 50-70                   bots in a levelling zone with a player in it; each zone picks a number in the range
//   Bots.InCity    = 120                     bots in a capital with a player in it
// Any Bots.* line switches the plain rules on, and the old keys (Playerbot.World.MaxBots, BotsPerPlayer, CityBots, ZoneBots,
// PoolMax, RssBudgetMb) are then not used (the server log names any that are still set). With no Bots.* line at all nothing
// changes: the old keys and their built-in defaults apply as before.
// The active cap is shared evenly: every occupied zone (or capital) wants its number; when all of them together want more than
// the cap, each one is cut by the same share. Players in the same zone share that zone's bots.
// Pure (no server types), so the tests can check every rule.

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Playerbot::V2::Fleet
{
    enum class BotSize : int { Low = 0, Normal = 1, High = 2 };

    struct PlainBots
    {
        uint32_t reserve = 0;      // -> PoolMax
        uint32_t active_cap = 0;   // -> MaxBots
        uint32_t zone_lo = 0;      // -> ZoneBots, rolled per zone in [lo, hi]
        uint32_t zone_hi = 0;
        uint32_t in_city = 0;      // -> CityBots
    };

    // Low, Normal, High. The launcher (repack\launcher\BotCount.cs) and the install (repack\make-tester-configs.ps1) show the same
    // numbers; keep the three in step.
    inline PlainBots PresetBots(BotSize s)
    {
        switch (s)
        {
            case BotSize::Low:  return {  600, 120, 15, 25,  50 };
            case BotSize::High: return { 3000, 500, 50, 70, 120 };
            default:            return { 1500, 250, 30, 45,  80 };
        }
    }

    inline char const* BotSizeName(BotSize s)
    {
        return s == BotSize::Low ? "Low" : s == BotSize::High ? "High" : "Normal";
    }

    inline std::string Squeeze(std::string_view text)
    {
        std::string t;
        for (char ch : text)
            if (!std::isspace(static_cast<unsigned char>(ch)) && ch != '"')
                t += char(std::tolower(static_cast<unsigned char>(ch)));
        return t;
    }

    // "low", " High ", "NORMAL" -> the size; anything else (empty, a typo) -> none
    inline std::optional<BotSize> ParseBotSize(std::string_view text)
    {
        std::string const t = Squeeze(text);
        if (t == "low" || t == "small") return BotSize::Low;
        if (t == "normal" || t == "medium") return BotSize::Normal;
        if (t == "high" || t == "large") return BotSize::High;
        return std::nullopt;
    }

    // Bots.Size = Off (also "none", "0"): the player's file asks for no world bots. Not a size: Low / Normal / High stay the presets.
    inline bool IsBotSizeOff(std::string_view text)
    {
        std::string const t = Squeeze(text);
        return t == "off" || t == "none" || t == "0" || t == "zero";
    }

    // "50-70" -> 50..70, "60" -> 60..60, "70-50" -> 50..70; anything else -> none
    inline bool ParseRange(std::string_view text, uint32_t& lo, uint32_t& hi)
    {
        std::string const t = Squeeze(text);
        if (t.empty()) return false;
        auto num = [](std::string const& s, uint32_t& out)
        {
            if (s.empty() || s.size() > 6) return false;
            for (char ch : s) if (!std::isdigit(static_cast<unsigned char>(ch))) return false;
            out = uint32_t(std::stoul(s));
            return true;
        };
        std::size_t const dash = t.find('-');
        uint32_t a = 0, b = 0;
        if (dash == std::string::npos) { if (!num(t, a)) return false; b = a; }
        else if (!num(t.substr(0, dash), a) || !num(t.substr(dash + 1), b)) return false;
        lo = std::min(a, b);
        hi = std::max(a, b);
        return true;
    }

    // What the config files say; an unset or unreadable line is nullopt.
    struct PlainInput
    {
        std::optional<BotSize> size;
        std::optional<uint32_t> reserve, active_cap, in_city;
        std::optional<std::string> per_zone;
    };

    // nullopt: no Bots.* line, the old keys apply.
    inline std::optional<PlainBots> ResolvePlainBots(PlainInput const& in)
    {
        uint32_t lo = 0, hi = 0;
        bool const zone_ok = in.per_zone && ParseRange(*in.per_zone, lo, hi);
        if (!in.size && !in.reserve && !in.active_cap && !in.in_city && !zone_ok)
            return std::nullopt;
        PlainBots b = PresetBots(in.size.value_or(BotSize::Normal));
        if (in.active_cap) b.active_cap = std::min<uint32_t>(*in.active_cap, 5000);
        if (in.reserve) b.reserve = std::min<uint32_t>(*in.reserve, 50000);
        if (in.in_city) b.in_city = std::min<uint32_t>(*in.in_city, 2000);
        if (zone_ok) { b.zone_lo = std::min<uint32_t>(lo, 1000); b.zone_hi = std::min<uint32_t>(hi, 1000); }
        b.reserve = std::max(b.reserve, b.active_cap);   // never fewer characters than may be logged in
        return b;
    }

    // The zone's number in [lo, hi] from a random draw (taken once when the zone gets its first player).
    inline uint32_t ZoneRoll(uint32_t lo, uint32_t hi, uint32_t draw)
    {
        if (hi <= lo) return lo;
        return lo + draw % (hi - lo + 1);
    }

    // One occupied place (a zone or a capital on one map): what it wants and how many players are in it.
    struct PlaceWant
    {
        uint64_t key = 0;
        uint32_t want = 0;
        uint32_t players = 1;
    };

    // The even share: when all places together want more than the cap, every place gets the same share of what it wants
    // (at least 1). Returns each place's number, in the same order.
    inline std::vector<uint32_t> ShareCap(std::vector<PlaceWant> const& places, uint32_t cap)
    {
        std::vector<uint32_t> out;
        out.reserve(places.size());
        uint64_t sum = 0;
        for (PlaceWant const& p : places) sum += p.want;
        for (PlaceWant const& p : places)
        {
            if (!p.want) { out.push_back(0); continue; }
            if (sum <= cap) { out.push_back(p.want); continue; }
            out.push_back(std::max<uint32_t>(1, uint32_t(uint64_t(p.want) * cap / sum)));
        }
        return out;
    }

    // One player's part of the place's number: players in the same place share its bots (rounded up, so nobody gets 0).
    inline uint32_t PerPlayer(uint32_t place_target, uint32_t players)
    {
        if (!place_target) return 0;
        if (players <= 1) return place_target;
        return (place_target + players - 1) / players;
    }

    inline uint64_t PlaceKey(uint32_t map, uint32_t zone) { return (uint64_t(map) << 32) | zone; }

    // The world bot choice of the ForeverBots panel (Karrz 2026-11-03: "bots off, low, medium or high ... rather than having it in
    // the config"). A GM picks it in game; the server keeps it (characters table playerbot_v2_realm_setting) and it wins over
    // Bots.Size, the other Bots.* numbers and Playerbot.World.Enable until "file" (.bot population file) hands back to the files.
    // Off: no world bots (the ones online log out). Low / Medium / High: that preset, world bots on. Medium is Normal.
    enum class PopLevel : int { Off = 0, Low = 1, Medium = 2, High = 3 };

    inline char const* PopLevelWord(PopLevel p)
    {
        switch (p)
        {
            case PopLevel::Off:  return "off";
            case PopLevel::Low:  return "low";
            case PopLevel::High: return "high";
            default:             return "medium";
        }
    }

    // "off" / "none" / "0", "low", "medium" / "normal", "high"; anything else -> none.
    inline std::optional<PopLevel> ParsePopLevel(std::string_view text)
    {
        std::string const t = Squeeze(text);
        if (t == "off" || t == "none" || t == "0" || t == "zero") return PopLevel::Off;
        if (std::optional<BotSize> const s = ParseBotSize(t))
            return *s == BotSize::Low ? PopLevel::Low : *s == BotSize::High ? PopLevel::High : PopLevel::Medium;
        return std::nullopt;
    }

    // Low / Medium / High -> its preset; Off -> all zero (nothing comes, the reserve is left as it is).
    inline PlainBots PopLevelBots(PopLevel p, uint32_t keep_reserve)
    {
        switch (p)
        {
            case PopLevel::Off:  return { keep_reserve, 0, 0, 0, 0 };
            case PopLevel::Low:  return PresetBots(BotSize::Low);
            case PopLevel::High: return PresetBots(BotSize::High);
            default:             return PresetBots(BotSize::Normal);
        }
    }

    // The bots online have to go first when the new choice wants fewer of them than are allowed now (or none): a step down logs
    // the world bots out and the new numbers fill again; a step up only lets more come.
    inline bool PopLevelNeedsDrain(uint32_t old_active_cap, uint32_t old_zone_hi, uint32_t old_city, PlainBots const& now)
    {
        return now.active_cap < old_active_cap || now.zone_hi < old_zone_hi || now.in_city < old_city;
    }
}

#endif
