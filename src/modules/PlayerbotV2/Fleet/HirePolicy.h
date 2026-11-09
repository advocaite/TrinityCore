// HirePolicy - the rules of hired bots (Playerbot.Hire.*), kept free of game types so tests/game/ClassicHirePolicy.cpp can pin them.
//
// In hired mode a player's companions are not made out of nothing: they are hired from a mercenary NPC for gold. Three grades:
//   Sellsword  every listed inn and the capitals, at the hirer's level, the normal companion gear (match).
//   Veteran    the capitals only, level cap only, raid gear up to Playerbot.Gear.MaxTier whatever the owner wears (best).
//   Legend     the black-market brokers of Gadgetzan, Booty Bay and Ratchet, level cap only, best in slot (bis).
// Prices are package prices at the level cap, as the server owner sets them: a 5-man (the 4 places beside the player) and a 40-man
// (39 places). One bot costs its package price divided by those places; a Sellsword below the cap costs (level / cap)^2 of that,
// never under kMinPriceCopper. Upkeep: a share of what was paid for the bot, once for each day the bot is called.

#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>

namespace Playerbot::V2::Fleet::Hire {

enum class Grade : uint8_t { None = 0, Sellsword = 1, Veteran = 2, Legend = 3 };
constexpr uint8_t kGradeCount = 4;

// Playerbot.Hire.Mode: 0 off (bots are free, as before), 1 hired bots for everyone, 2 each player chooses (`.bot hire on|off`).
enum class Mode : uint8_t { Off = 0, Everyone = 1, PlayerChoice = 2 };

// What a broker NPC sells, from its creature entry (kBrokerEntryFirst.. in blocks of 100).
enum class Broker : uint8_t { None = 0, Inn = 1, Capital = 2, BlackMarket = 3 };

// (an entry travels in 23 bits to the client: nothing above 8388607 can be a creature the client lets you talk to)
constexpr uint32_t kBrokerEntryFirst = 6110000;   // 6110000-6110099 inn, 6110100-6110199 capital, 6110200-6110299 black market
constexpr uint32_t kBrokerEntryLast  = 6110299;
constexpr uint64_t kMinPriceCopper   = 100;       // 1 silver
constexpr uint32_t kPartyPlaces      = 4;         // the places beside the player in a 5-man
constexpr uint32_t kRaidPlaces       = 39;        // ... in a 40-man
constexpr uint32_t kDaySeconds       = 86400;
// A Group Finder / dungeon finder bot made for a player who must hire: it plays with them for free while it stays in their group,
// cannot be called on its own and is deleted once it leaves the group (a loan, not a hire). Stored as this hire_grade value.
constexpr uint8_t  kLoanGradeDb      = 9;
constexpr uint32_t kLoanGraceMs      = 120000;    // a loan bot that is still logging in or joining is not taken back yet

constexpr Broker BrokerOfEntry(uint32_t entry)
{
    if (entry < kBrokerEntryFirst || entry > kBrokerEntryLast)
        return Broker::None;
    return Broker(1 + (entry - kBrokerEntryFirst) / 100);
}

// The grades each kind of broker sells by default: an inn Sellswords, a capital Sellswords and Veterans, the black market Legends.
// A server owner changes this with Playerbot.Hire.<Inn|Capital|BlackMarket>.Sells (Rules below).
constexpr bool BrokerSells(Broker b, Grade g)
{
    switch (b)
    {
        case Broker::Inn:         return g == Grade::Sellsword;
        case Broker::Capital:     return g == Grade::Sellsword || g == Grade::Veteran;
        case Broker::BlackMarket: return g == Grade::Legend;
        default:                  return false;
    }
}

// Veterans and Legends are level-cap only: raid gear means nothing below it, and a Legend hired at 15 for pocket money would grow
// into best-in-slot gear for free.
constexpr bool GradeForLevel(Grade g, uint32_t level, uint32_t cap)
{
    if (g == Grade::Sellsword)
        return level >= 1;
    return (g == Grade::Veteran || g == Grade::Legend) && level >= cap;
}

// A grade's package prices at the level cap, in copper.
struct GradePrices
{
    uint64_t party = 0;   // the 4 bots of a 5-man
    uint64_t raid  = 0;   // the 39 bots of a 40-man
};

struct Prices
{
    GradePrices grade[kGradeCount] = {
        {},
        { 10000, 200000 },     // Sellsword  1g / 20g
        { 30000, 600000 },     // Veteran    3g / 60g
        { 50000, 1000000 },    // Legend     5g / 100g
    };
    uint32_t upkeep_pct = 25;  // of the price paid, per day the bot is called; 0 = no upkeep
};

// The server owner's rules for each grade and broker (Playerbot.Hire.<Grade>.Gear / .HealthPct, Playerbot.Hire.<Broker>.Sells). The
// defaults are the built-in rules above, so a server without these lines plays as before.
struct Rules
{
    // Gear mode (Gear::Owned::GearMode value: 0 match, 1 best, 2 t1, 3 t2, 4 t3, 5 bis) each grade wears.
    int gear_mode[kGradeCount] = { -1, 0, 1, 5 };
    // A hired bot's health in percent of what its class, level and gear give it (100 = normal).
    uint32_t health_pct[kGradeCount] = { 100, 100, 100, 100 };
    // Bit (1 << grade) for each grade a kind of broker sells, by Broker value.
    uint8_t sells[4] = { 0, 1u << 1, (1u << 1) | (1u << 2), 1u << 3 };
};

constexpr uint32_t kMinHealthPct = 25;
constexpr uint32_t kMaxHealthPct = 1000;

constexpr bool BrokerSells(Rules const& r, Broker b, Grade g)
{
    if (b == Broker::None || uint8_t(b) > 3 || g == Grade::None || uint8_t(g) >= kGradeCount)
        return false;
    return (r.sells[uint8_t(b)] >> uint8_t(g)) & 1u;
}

constexpr uint32_t HealthPctFor(Rules const& r, Grade g)
{
    if (g == Grade::None || uint8_t(g) >= kGradeCount)
        return 100;
    return std::clamp(r.health_pct[uint8_t(g)], kMinHealthPct, kMaxHealthPct);
}

// The extra (or, under 100%, missing) health of a bot whose class, level and gear give it `normal` health, at `pct` percent.
constexpr int64_t HealthBonusFor(uint64_t normal, uint32_t pct)
{
    pct = std::clamp(pct, kMinHealthPct, kMaxHealthPct);
    return int64_t(normal) * (int64_t(pct) - 100) / 100;
}

// A list of grade names ("sellsword, veteran"; commas or spaces, any case) as Rules::sells bits. "none" or an empty list = nothing.
// Unknown words are skipped.
inline uint8_t ParseGradeList(std::string_view list)
{
    uint8_t mask = 0;
    size_t i = 0;
    while (i <= list.size())
    {
        size_t j = list.find_first_of(", ", i);
        if (j == std::string_view::npos)
            j = list.size();
        std::string w(list.substr(i, j - i));
        for (char& c : w)
            if (c >= 'A' && c <= 'Z')
                c = char(c - 'A' + 'a');
        if (w == "sellsword" || w == "sellswords")
            mask |= 1u << uint8_t(Grade::Sellsword);
        else if (w == "veteran" || w == "veterans")
            mask |= 1u << uint8_t(Grade::Veteran);
        else if (w == "legend" || w == "legends")
            mask |= 1u << uint8_t(Grade::Legend);
        i = j + 1;
    }
    return mask;
}

// The price of one bot of grade `g` for a hirer of `level`; `raid` = the bot fills a raid place (a raid group, or an instance for
// more than 5). 0 = not for sale.
constexpr uint64_t PriceFor(Prices const& p, Grade g, uint32_t level, uint32_t cap, bool raid)
{
    if (g == Grade::None || uint8_t(g) >= kGradeCount || !GradeForLevel(g, level, cap) || cap == 0)
        return 0;
    GradePrices const& gp = p.grade[uint8_t(g)];
    uint64_t const package = raid ? gp.raid : gp.party;
    uint64_t const places = raid ? kRaidPlaces : kPartyPlaces;
    uint64_t one = package / places;
    if (g == Grade::Sellsword && level < cap)
        one = one * uint64_t(level) * uint64_t(level) / (uint64_t(cap) * uint64_t(cap));
    return std::max(one, kMinPriceCopper);
}

// The day's upkeep for a bot that was hired for `paid` copper (0 = no upkeep: a bot owned before hired mode came on, or upkeep off).
constexpr uint64_t UpkeepFor(Prices const& p, uint64_t paid)
{
    if (!paid || !p.upkeep_pct)
        return 0;
    return std::max(paid * p.upkeep_pct / 100, kMinPriceCopper);
}

// Whether a bot's upkeep is due when it is called at `now` (unix seconds): its paid day has run out. Days the bot is not called cost
// nothing; the next paid day runs a full day from the payment.
constexpr bool UpkeepDue(uint64_t paid_until, uint64_t now) { return paid_until != 0 && now >= paid_until; }

// Whether the player must hire: hired mode is on for everyone, or the player chose it; a GM skips it when GmBypass is on.
constexpr bool Required(Mode mode, bool player_opted_in, bool is_gm, bool gm_bypass)
{
    if (is_gm && gm_bypass)
        return false;
    return mode == Mode::Everyone || (mode == Mode::PlayerChoice && player_opted_in);
}

// The gear mode (Gear::Owned::GearMode value) a hired bot wears: a Veteran the raid gear (best = 1), a Legend best in slot
// (bis = 5). A Sellsword, or an owned bot of a player who must hire, wears match (0) whatever the owner chose; -1 = no rule (the
// owner's own choice stands).
constexpr int GearModeFor(Grade g, bool owner_must_hire)
{
    if (g == Grade::Legend)  return 5;
    if (g == Grade::Veteran) return 1;
    if (g == Grade::Sellsword || owner_must_hire) return 0;
    return -1;
}

// The same with the server owner's gear for each grade (Playerbot.Hire.<Grade>.Gear).
constexpr int GearModeFor(Rules const& r, Grade g, bool owner_must_hire)
{
    if (g != Grade::None && uint8_t(g) < kGradeCount && r.gear_mode[uint8_t(g)] >= 0)
        return r.gear_mode[uint8_t(g)];
    return owner_must_hire ? 0 : -1;
}

constexpr char const* GradeName(Grade g)
{
    switch (g)
    {
        case Grade::Sellsword: return "Sellsword";
        case Grade::Veteran:   return "Veteran";
        case Grade::Legend:    return "Legend";
        default:               return "Companion";
    }
}

// "1g 25s", "75s", "12s 50c": gold, silver and copper, the parts that are not zero.
inline std::string MoneyText(uint64_t copper)
{
    uint64_t const g = copper / 10000, s = (copper / 100) % 100, c = copper % 100;
    std::string out;
    auto part = [&](uint64_t v, char const* unit)
    {
        if (!v) return;
        if (!out.empty()) out += ' ';
        out += std::to_string(v) + unit;
    };
    part(g, "g");
    part(s, "s");
    part(c, "c");
    return out.empty() ? std::string("0c") : out;
}

}  // namespace Playerbot::V2::Fleet::Hire
