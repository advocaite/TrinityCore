// BossWarnings — on-screen warnings for the human players of a raid group while a boss fights.
//
// "Magmadar casts Panic - fear! (about 5 sec)", "Baron Geddon is casting Inferno!". Server side: the warning is a
// CHAT_MSG_RAID_BOSS_EMOTE packet sent straight to the human members (bots ignore chat), so the client shows it in the
// centre of the screen and in the chat frame, the way the retail encounters do. It is not a /raid-warning from a player,
// so nobody needs to be leader or assistant.
//
// This header is the pure part (rule vocabulary, timing state machine, text) so it can be unit tested;
// BossWarningData_*.h hold the data tables and BossWarnings.cpp reads the live boss state.
//
// Two ways a rule warns:
//  * warnOnCast: the boss's cast bar shows this spell (a spell with a cast time or a channel): announced when it begins.
//  * predicted:  the ability is instant, but the script casts it on a timer. firstMs after the fight starts and repeatMs
//    after the previous cast are the EARLIEST times the script can cast it (scripts randomise between a minimum and a
//    maximum), and the warning goes out leadMs before that. A cast the raid can observe (resyncAura appearing on the boss
//    or a raid member, or the cast bar) re-anchors the clock, so the prediction does not drift.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Playerbot::Raid {

struct BossWarning
{
    uint32_t    boss;          // creature entry
    uint32_t    spell;         // the boss's spell (cast bar match for warnOnCast)
    char const* label;         // what to tell the players: "Panic - fear!"
    bool        warnOnCast;    // announce when its cast bar starts
    uint32_t    firstMs;       // earliest first use after the fight starts (0 = unknown: no early warning for the first)
    uint32_t    repeatMs;      // earliest re-use after the previous one (0 = does not repeat / not predictable)
    uint32_t    leadMs;        // warn this long before the earliest time
    uint32_t    resyncAura;    // aura that appears when the spell has just landed (0 = none)
    char const* carrierLabel = nullptr;   // when set, a raid member gaining resyncAura is named at once: "<name> <carrierLabel>"
};

struct BossWarnEvent
{
    size_t   rule;             // index into the rule list
    bool     casting;          // true: the cast has started; false: it is expected soon
    uint32_t secs;             // casting: seconds left on the cast bar (0 = instant); predicted: seconds until the earliest time
};

// One fight of one boss. The rule list must outlive the tracker.
class BossWarnTracker
{
public:
    // After this long past the earliest time with no sighting, the script has randomised later (or the spell was skipped):
    // assume the next cycle instead of warning again.
    static constexpr uint32_t kLateGraceMs = 12000;

    explicit BossWarnTracker(std::vector<BossWarning> const* rules) : rules_(rules), st_(rules->size()) {}

    void Start(uint32_t nowMs)
    {
        for (size_t i = 0; i < st_.size(); ++i)
        {
            st_[i] = State{};
            if ((*rules_)[i].firstMs)
                st_[i].earliest = nowMs + (*rules_)[i].firstMs;
        }
    }

    // castingSpell: the spell on the boss's cast bar or channel (0 = none), castLeftMs its remaining time.
    // auraPresent: per rule, whether rule.resyncAura is on the boss or any raid member right now (may be empty).
    std::vector<BossWarnEvent> Tick(uint32_t nowMs, uint32_t castingSpell, uint32_t castLeftMs, std::vector<uint8_t> const& auraPresent)
    {
        std::vector<BossWarnEvent> out;
        for (size_t i = 0; i < st_.size(); ++i)
        {
            BossWarning const& r = (*rules_)[i];
            State& s = st_[i];
            bool landed = false;

            if (castingSpell && castingSpell == r.spell)
            {
                if (!s.castActive)
                {
                    s.castActive = true;
                    landed = true;
                    if (r.warnOnCast)
                        out.push_back({ i, true, (castLeftMs + 999) / 1000 });
                }
            }
            else
                s.castActive = false;

            if (r.resyncAura)
            {
                bool const now = i < auraPresent.size() && auraPresent[i];
                if (now && !s.auraWas)
                    landed = true;
                s.auraWas = now;
            }

            if (landed)
            {
                s.earliest = r.repeatMs ? nowMs + r.repeatMs : 0;
                s.warned = false;
                continue;
            }
            if (!s.earliest)
                continue;
            if (!s.warned && nowMs + r.leadMs >= s.earliest)
            {
                s.warned = true;
                out.push_back({ i, false, nowMs >= s.earliest ? 0u : (s.earliest - nowMs + 999) / 1000 });
            }
            else if (s.warned && nowMs >= s.earliest + kLateGraceMs)
            {
                s.earliest = r.repeatMs ? s.earliest + r.repeatMs : 0;
                s.warned = false;
            }
        }
        return out;
    }

private:
    struct State
    {
        uint32_t earliest = 0;   // 0 = nothing predicted
        bool     warned = false;
        bool     castActive = false;
        bool     auraWas = false;
    };
    std::vector<BossWarning> const* rules_;
    std::vector<State> st_;
};

// "Magmadar is casting Panic - fear! (2 sec)" / "Magmadar: Panic - fear! in about 5 sec"
inline std::string FormatBossWarning(std::string const& boss, BossWarning const& r, BossWarnEvent const& e)
{
    std::string s = boss;
    if (e.casting)
    {
        s += " is casting ";
        s += r.label;
        if (e.secs)
            s += " (" + std::to_string(e.secs) + " sec)";
    }
    else
    {
        s += ": ";
        s += r.label;
        s += e.secs ? " in about " + std::to_string(e.secs) + " sec" : " any moment now";
    }
    return s;
}

// Rules for one boss entry (the tables are per boss so the live code only handles the bosses it fights).
std::vector<BossWarning> const& BossWarningsFor(uint32_t bossEntry);

// World-thread driver (BossWarnings.cpp): throttled to 4 Hz, does nothing when Playerbot.Raid.BossWarnings = 0.
void UpdateBossWarnings(uint32_t nowMs);

} // namespace Playerbot::Raid
