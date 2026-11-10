// RaidBanter - a raid leader who reacts to mistakes, and bots that argue after a wipe, the way a real raid's chat reads.
//
// One bot of the raid is its leader (the group's leader when that is a bot, else an assistant bot, else the first bot): it calls out
// a raider standing in the bad stuff, a cast nobody kicked, a crowd control somebody broke, an extra pull, the wipe and the kill. After
// a wipe one or two other bots answer: blame, excuses, "my bad". Game mistakes only, in good fun: never about the person behind the
// character, never a slur (the lines here are fixed; a model's line goes through the program's filter, and the program never sees a
// player's own persona text for these).
//
//   Playerbot.Raid.Banter  0 off, 1 mild (teasing, the default), 2 harsh (a leader who freaks out at every mishap)
//
// No model needed: every line has a fixed form here, translated per reader where it leaves the server (the Russian catalog). With the bot
// bridge answering chat, the program may write the line in the bot's own words; when its line is not back in time (kModelWaitMs), the
// fixed line is said instead, so a raid never waits for a slow model.
//
// Pure: the vocabulary, the lines, the rate limits (unit tested in tests/game/ClassicBossWarnings.cpp). RaidBrain.cpp spots the
// mishaps and sends.

#pragma once

#include <cstdint>
#include <iterator>
#include <string>
#include <unordered_map>
#include <vector>

namespace Playerbot::Raid {

enum class BanterLevel : uint8_t { Off = 0, Mild = 1, Harsh = 2 };

enum class Mishap : uint8_t
{
    StoodInBad = 0,   // a raider stays inside a hazard circle (fire, a void zone, a bomb's blast) and gets hurt
    MissedKick,       // a cast the raid kicks first (RaidRules::interruptFirst) went off
    BrokeCc,          // a long crowd control broke early under someone's damage
    ExtraPull,        // a new pack joined a running fight (not the boss's own summons)
    Wipe,             // the raid is lost
    BossDown,         // the boss died
    Count
};

// How long the program has to write a line before the fixed one is said.
constexpr uint32_t kModelWaitMs = 2500;

namespace BanterDetail {

struct Lines { char const* mild[3]; char const* harsh[3]; };

// `{}` is the name the line is about (a raider, a spell; for the kill, the raider who died first, if anyone did). Lower case, short,
// no pronoun that needs a gender.
inline Lines const& LeaderLines(Mishap m, bool named)
{
    static Lines const kBad      = { { "{}, out of the bad stuff pls", "{} you're standing in it", "{} move, that hurts" },
                                     { "{} GET OUT OF IT", "{} why are you standing in that again", "{} MOVE. it's not a buff" } };
    static Lines const kKick     = { { "who's on kicks? {} went off", "kick {} next time pls", "{} got through, kickers wake up" },
                                     { "NOBODY KICKED {}??", "{} went off AGAIN. kick it", "are the kickers asleep? {} went off" } };
    static Lines const kCc       = { { "{} broke the cc, watch your target", "careful {}, that was cc'd", "{} broke it, check the marks" },
                                     { "{} STOP BREAKING MY CC", "{} broke cc again. target. the. skull", "who broke it? oh it's {} again" } };
    static Lines const kCcAnon   = { { "cc broke, watch your targets", "someone broke the cc, check the marks", "cc's gone, tanks pick it up" },
                                     { "WHO BROKE THE CC", "cc broke AGAIN. target the skull", "who's hitting the cc'd one. stop" } };
    static Lines const kPull     = { { "extra pull, adds on {}, tanks grab them", "adds on {}, pick them up", "we've got company, adds on {}" },
                                     { "WHO PULLED THAT. adds on {}", "{} has adds, tanks GRAB THEM", "extra pull again?? adds on {}" } };
    static Lines const kPullAnon = { { "extra pull, tanks grab the adds", "we've got company, tanks pick them up", "adds incoming, tanks" },
                                     { "WHO PULLED THAT", "extra pull AGAIN, tanks grab them", "adds. why are there adds" } };
    static Lines const kWipe     = { { "wipe, it happens. run back and rebuff", "ok that's a wipe, run back, we got this",
                                       "wipe. {} went first, let's go again" },
                                     { "WIPE. {} died first. again", "that was a mess. run back and stay out of the bad stuff",
                                       "{} died first, everyone run back. AGAIN" } };
    static Lines const kDown     = { { "gg, nice one everyone", "gg, clean kill", "gg, rez {} and let's move on" },
                                     { "FINALLY. gg", "gg. see, not that hard", "gg. {}, we'll talk about dying later" } };
    switch (m)
    {
        case Mishap::StoodInBad: return kBad;
        case Mishap::MissedKick: return kKick;
        case Mishap::BrokeCc:    return named ? kCc : kCcAnon;
        case Mishap::ExtraPull:  return named ? kPull : kPullAnon;
        case Mishap::Wipe:       return kWipe;
        default:                 return kDown;
    }
}

inline std::string Fill(char const* tmpl, std::string const& name)
{
    std::string out(tmpl);
    if (size_t const at = out.find("{}"); at != std::string::npos)
        out.replace(at, 2, name);
    return out;
}

inline bool HasSlot(char const* tmpl) { return std::string(tmpl).find("{}") != std::string::npos; }

} // namespace BanterDetail

// The raid leader's line for a mishap. `name` is the raider (or the spell, for MissedKick) it is about, may be empty (the lines without
// a name are taken then). `pick` chooses among the lines (a random number). Off: empty.
inline std::string LeaderLine(Mishap m, BanterLevel level, std::string const& name, uint32_t pick)
{
    if (level == BanterLevel::Off || m >= Mishap::Count)
        return {};
    BanterDetail::Lines const& lines = BanterDetail::LeaderLines(m, !name.empty());
    char const* const* set = level == BanterLevel::Harsh ? lines.harsh : lines.mild;
    // A line with a name slot needs a name; without one, the first line with no slot (or nothing).
    for (uint32_t i = 0; i < 3; ++i)
    {
        char const* tmpl = set[(pick + i) % 3];
        if (!name.empty() || !BanterDetail::HasSlot(tmpl))
            return BanterDetail::Fill(tmpl, name);
    }
    return {};
}

// After a wipe: what one other bot says (an excuse, a blame, an apology). `blame` is the raider who died first (may be empty), `self`
// whether the speaking bot is that raider. Harsh adds a bit of bite. Never about the person, only the fight.
inline std::string WipeReply(BanterLevel level, std::string const& blame, bool self, uint32_t pick)
{
    if (level == BanterLevel::Off)
        return {};
    if (self)
    {
        static char const* const kSelf[] = { "my bad", "sorry, my bad", "lag, i swear", "i was rooted ok" };
        return kSelf[pick % std::size(kSelf)];
    }
    static char const* const kMild[] = { "healers?", "lag", "who had the bomb", "rip", "one more go", "{} pulled aggro i think",
                                          "that was {} lol" };
    static char const* const kHarsh[] = { "that was {}'s fault", "{} pulled aggro AGAIN", "healers??", "can we get {} out of the fire next time",
                                          "lag. definitely lag", "who had the bomb. be honest" };
    char const* const* set = level == BanterLevel::Harsh ? kHarsh : kMild;
    size_t const n = level == BanterLevel::Harsh ? std::size(kHarsh) : std::size(kMild);
    for (size_t i = 0; i < n; ++i)
    {
        char const* tmpl = set[(pick + i) % n];
        if (!blame.empty() || !BanterDetail::HasSlot(tmpl))
            return BanterDetail::Fill(tmpl, blame);
    }
    return {};
}

// What the program is told happened, in plain words (it writes its own line from this when the bridge is on).
inline std::string MishapDetail(Mishap m, std::string const& name, std::string const& boss)
{
    std::string const at = boss.empty() ? std::string() : " (fighting " + boss + ")";
    switch (m)
    {
        case Mishap::StoodInBad: return name + " keeps standing in a damaging area" + at;
        case Mishap::MissedKick: return "nobody interrupted the enemy cast " + name + at;
        case Mishap::BrokeCc:    return (name.empty() ? std::string("someone") : name) + " broke a crowd-controlled enemy early" + at;
        case Mishap::ExtraPull:  return "an extra pack of enemies was pulled into the fight" + (name.empty() ? std::string() : ", they attack " + name) + at;
        case Mishap::Wipe:       return "the raid just wiped" + at + (name.empty() ? std::string() : "; " + name + " died first");
        default:                 return "the raid just killed the boss" + (boss.empty() ? std::string() : " " + boss) +
                                        (name.empty() ? std::string() : "; " + name + " died during the fight");
    }
}

inline char const* MishapWord(Mishap m)
{
    static char const* const kWords[] = { "stood_in_bad", "missed_kick", "broke_cc", "extra_pull", "wipe", "boss_down" };
    return m < Mishap::Count ? kWords[size_t(m)] : "mishap";
}

// How often the leader may react, so it reads like a person and not a log:
//  * one line per raid every kLeaderGapMs (mild) or kLeaderGapHarshMs (harsh);
//  * the same kind of mishap once per kSameKindMs; the same raider named for the same kind once per kSameNameMs;
//  * a wipe and a kill always get their line (they end the fight), at most once per kFightEndMs.
class BanterGate
{
public:
    static constexpr uint32_t kLeaderGapMs      = 20000;
    static constexpr uint32_t kLeaderGapHarshMs = 10000;
    static constexpr uint32_t kSameKindMs       = 45000;
    static constexpr uint32_t kSameNameMs       = 90000;
    static constexpr uint32_t kFightEndMs       = 30000;

    bool Allow(uint64_t group, Mishap m, std::string const& name, BanterLevel level, uint32_t now)
    {
        if (level == BanterLevel::Off || m >= Mishap::Count)
            return false;
        State& s = groups_[group];
        size_t const k = size_t(m);
        bool const end = m == Mishap::Wipe || m == Mishap::BossDown;
        if (end)
        {
            if (s.end && now - s.end < kFightEndMs)
                return false;
        }
        else
        {
            uint32_t const gap = level == BanterLevel::Harsh ? kLeaderGapHarshMs : kLeaderGapMs;
            if (s.last && now - s.last < gap)
                return false;
            if (s.kind[k] && now - s.kind[k] < kSameKindMs)
                return false;
            if (!name.empty())
                if (auto it = s.named.find(std::to_string(k) + ":" + name); it != s.named.end() && now - it->second < kSameNameMs)
                    return false;
        }
        uint32_t const stamp = now ? now : 1u;
        s.last = stamp;
        s.kind[k] = stamp;
        if (end)
            s.end = stamp;
        if (!name.empty())
        {
            if (s.named.size() > 64)
                s.named.clear();
            s.named[std::to_string(k) + ":" + name] = stamp;
        }
        return true;
    }

    void Prune(uint32_t now)
    {
        for (auto it = groups_.begin(); it != groups_.end();)
            it = now - it->second.last >= kSameNameMs ? groups_.erase(it) : std::next(it);
    }

    size_t Tracked() const { return groups_.size(); }

private:
    struct State
    {
        uint32_t last = 0, end = 0;
        uint32_t kind[size_t(Mishap::Count)] = {};
        std::unordered_map<std::string, uint32_t> named;
    };
    std::unordered_map<uint64_t, State> groups_;
};

// Reads Playerbot.Raid.Banter (0 / 1 / 2, anything higher is harsh).
inline BanterLevel BanterLevelFromConfig(int value)
{
    return value <= 0 ? BanterLevel::Off : value == 1 ? BanterLevel::Mild : BanterLevel::Harsh;
}

inline char const* BanterLevelWord(BanterLevel l)
{
    return l == BanterLevel::Harsh ? "harsh" : l == BanterLevel::Mild ? "mild" : "off";
}

} // namespace Playerbot::Raid
