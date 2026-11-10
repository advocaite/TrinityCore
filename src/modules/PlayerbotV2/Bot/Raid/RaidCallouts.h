// RaidCallouts - bots say what they are doing in raid chat, the way players do ("Living Bomb on me, running out", "taunting
// Flamegor", "Polymorph on Flamewaker Healer, don't break it", "spread out").
//
// No model: the line comes from the raid brain's own decision (RaidOrder), at the moment that decision changes. The text is
// English and is translated per reader where it leaves the server (BotLanguage::GroupChat and the Russian catalog).
//
// This header is the pure part (which line for which decision, and how often a bot or a raid may speak) so it can be unit
// tested; RaidBrain.cpp resolves the names and sends.
//
//   Playerbot.Raid.Callouts  1   0 = the bots stay quiet in raid chat

#pragma once

#include <cstdint>
#include <deque>
#include <iterator>
#include <string>
#include <string_view>
#include <unordered_map>

namespace Playerbot::Raid {

enum class CalloutKind : uint8_t
{
    None = 0,
    WipeRetreat,    // the raid is lost: one bot calls the run back
    CarrierRun,     // the bot carries a bomb aura and runs out
    IceBlock,       // the bot hides behind an ice block for a breath
    Spread,         // the raid has to spread: one bot calls it
    HeiganDance,    // Heigan's dance starts: one bot calls it
    TakeOrb,        // the bot takes Razorgore's Orb of Domination
    HoldHeals,      // a healing priest stops for Nefarian's priest call
    Taunt,          // the bot taunts a boss or an add
    CrowdControl,   // the bot polymorphs, banishes or shackles an add for a long time
    Count
};

// What the bot was just told to do (a changed raid order), with the names already resolved by the caller.
struct CalloutFacts
{
    std::string_view move_reason;   // RaidOrder::move_reason, empty when the bot has no move
    bool taunt = false;             // its elected cast is a taunt
    bool long_cc = false;           // its elected cast is crowd control that lasts 15 s or more
    bool hold_rotation = false;
    bool use_object = false;
    std::string spell;              // the elected spell's name (taunt, crowd control)
    std::string target;             // the elected cast's target name
    std::string carried;            // carrier run: the name of the aura the bot carries
};

struct Callout
{
    CalloutKind kind = CalloutKind::None;
    std::string text;
    bool group_wide = false;        // a call for the whole raid: one bot says it, not every bot that got the order
};

// The one line for this decision, the most urgent first. kind None = nothing worth saying.
inline Callout PickCallout(CalloutFacts const& f)
{
    std::string_view const m = f.move_reason;
    if (m == "wipe retreat")
        return { CalloutKind::WipeRetreat, "wipe, run back to the entrance", true };
    if (m == "carrier run")
        return { CalloutKind::CarrierRun, (f.carried.empty() ? std::string("bomb") : f.carried) + " on me, running out", false };
    if (m == "hide behind ice block")
        return { CalloutKind::IceBlock, "hiding behind an ice block", false };
    if (m == "spread")
        return { CalloutKind::Spread, "spread out", true };
    if (m == "heigan dance" || m == "heigan eruption")
        return { CalloutKind::HeiganDance, "dance time, follow the safe side", true };
    if (f.use_object || m.substr(0, 4) == "orb:")
        return { CalloutKind::TakeOrb, "taking the orb", false };
    if (f.hold_rotation)
        return { CalloutKind::HoldHeals, "priest call, not healing", false };
    if (f.taunt && !f.target.empty())
        return { CalloutKind::Taunt, "taunting " + f.target, false };
    if (f.long_cc && !f.spell.empty() && !f.target.empty())
        return { CalloutKind::CrowdControl, f.spell + " on " + f.target + ", don't break it", false };
    return {};
}

// How often the bots may speak, so raid chat reads like players and not like a log:
//  * the same bot, the same kind of line: once per kSameKindMs (a re-sheep a few seconds later stays quiet);
//  * the same bot, any line: once per kBotGapMs;
//  * a group-wide call (spread, wipe, dance): once per kGroupWideMs for the whole raid, whichever bot says it;
//  * the whole raid: kGroupGapMs between two lines, at most kWindowMax lines in kWindowMs.
class CalloutGate
{
public:
    static constexpr uint32_t kSameKindMs  = 20000;
    static constexpr uint32_t kBotGapMs    = 6000;
    static constexpr uint32_t kGroupWideMs = 45000;
    static constexpr uint32_t kGroupGapMs  = 2000;
    static constexpr uint32_t kWindowMs    = 20000;
    static constexpr uint32_t kWindowMax   = 5;

    // True when the line may be said now; it is then counted as said.
    bool Allow(uint64_t group, uint64_t bot, CalloutKind kind, bool group_wide, uint32_t now)
    {
        if (kind == CalloutKind::None)
            return false;
        GroupState& g = groups_[group];
        while (!g.said.empty() && now - g.said.front() >= kWindowMs)
            g.said.pop_front();
        if (!g.said.empty() && now - g.said.back() < kGroupGapMs)
            return false;
        if (g.said.size() >= kWindowMax)
            return false;
        size_t const k = static_cast<size_t>(kind);
        if (group_wide && g.wide[k] && now - g.wide[k] < kGroupWideMs)
            return false;
        BotState& b = bots_[bot];
        if (b.last && now - b.last < kBotGapMs)
            return false;
        if (b.kind[k] && now - b.kind[k] < kSameKindMs)
            return false;

        uint32_t const stamp = now ? now : 1u;
        g.said.push_back(stamp);
        g.last = stamp;
        if (group_wide)
            g.wide[k] = stamp;
        b.last = stamp;
        b.kind[k] = stamp;
        return true;
    }

    // Drops what no longer limits anything (call now and then).
    void Prune(uint32_t now)
    {
        uint32_t const keep = kGroupWideMs > kSameKindMs ? kGroupWideMs : kSameKindMs;
        for (auto it = bots_.begin(); it != bots_.end();)
            it = now - it->second.last >= keep ? bots_.erase(it) : std::next(it);
        for (auto it = groups_.begin(); it != groups_.end();)
            it = now - it->second.last >= keep ? groups_.erase(it) : std::next(it);
    }

    size_t Tracked() const { return bots_.size() + groups_.size(); }

private:
    static constexpr size_t kKinds = static_cast<size_t>(CalloutKind::Count);
    struct BotState { uint32_t last = 0; uint32_t kind[kKinds] = {}; };
    struct GroupState { std::deque<uint32_t> said; uint32_t last = 0; uint32_t wide[kKinds] = {}; };
    std::unordered_map<uint64_t, BotState> bots_;
    std::unordered_map<uint64_t, GroupState> groups_;
};

} // namespace Playerbot::Raid
