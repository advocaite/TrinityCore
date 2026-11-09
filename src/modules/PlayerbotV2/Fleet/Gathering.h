// Gathering - small groups of world bots hanging out just outside a capital's gate while a real player is near: one ring sits around
// a campfire, one stands together, and they talk to each other in /say now and then, the way the crowd outside Orgrimmar, Ironforge and
// Stormwind looked on a vanilla realm. Pure part: which cities, the layout, the talk scripts and their pacing, the board the idle rule
// reads. The driver is WorldBotSpawner::DriveGatherings (it uses the gate GateDuels found); the rule is idle:gathering
// (Bot/States/Rules/AmbientRules.cpp). With Playerbot.ChatModel on, the lines come from the chat model instead of the scripts
// (BotChatModel::AskSay). Tested in tests/game/ClassicGathering.cpp.
//
// Street chats (Playerbot.World.Gathering.CityChats): inside a capital, where a real player walks, two to four world bots stand
// together by the bank, the auction house, the inn, a shop or a mailbox, turned to each other, and talk face to face in /say with
// a gesture on each line. Same driver, same talk; their ring numbers start at kStreetRing.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Playerbot::V2::Gathering
{

constexpr std::size_t   kMaxRing        = 6;         // bots in one ring
constexpr std::size_t   kMinRing        = 3;         // fewer than this is no gathering
constexpr float         kSitRadius      = 3.0f;      // yards from the fire to a seat
constexpr float         kStandRadius    = 2.4f;      // a standing group is closer
constexpr std::uint32_t kRestAfterMs    = 600000u;   // a bot that sat at a gathering does something else for a while
constexpr std::uint32_t kLineMinMs      = 6000u;     // time between two lines of one talk: never faster than this
constexpr std::uint32_t kLineMaxMs      = 14000u;

// The gate cities that get gatherings: the big three by default (GateDuels::kCities order), all six with AllCities.
inline bool BigCity(std::size_t gate_city)
{
    return gate_city == 0 || gate_city == 1 || gate_city == 3;   // Stormwind, Ironforge, Orgrimmar
}

struct Pt { float x, y; };

// Where ring `ring` (0 = the campfire, 1 = standing) of a gate lies: between the duel slots of GateDuels::LayoutFor (angles 1 + 2.1k),
// far enough out that a duel does not run through it.
inline Pt RingCenter(float gx, float gy, std::uint32_t ring)
{
    float const th = 2.05f + 2.1f * float(ring), r = 24.0f + 8.0f * float(ring);
    return { gx + std::cos(th) * r, gy + std::sin(th) * r };
}

// Seat `i` of `n` around (cx, cy): evenly round the circle, each facing the middle (the angle to face is returned in `face`).
inline Pt Seat(Pt c, std::size_t i, std::size_t n, float radius, float& face)
{
    float const a = 0.4f + 6.2831853f * float(i) / float(std::max<std::size_t>(n, 1));
    face = std::fmod(a + 3.14159265f, 6.2831853f);
    return { c.x + std::cos(a) * radius, c.y + std::sin(a) * radius };
}

// How many bots each ring gets out of `want` (the per-gate cap): the campfire first, the standing group only when it can hold kMinRing.
inline void SplitRings(std::size_t want, std::size_t& fire, std::size_t& stand)
{
    want = std::min(want, 2 * kMaxRing);
    fire = std::min(kMaxRing, (want + 1) / 2);
    stand = want - fire;
    if (stand < kMinRing) { fire = std::min(kMaxRing, fire + stand); stand = 0; }
    stand = std::min(stand, kMaxRing);
    if (fire < kMinRing) fire = 0;
}

// ---- Street chats -------------------------------------------------------------------------------------------------------------------

constexpr std::uint32_t kStreetRing      = 100u;      // ring numbers from here on are street chats inside a city
constexpr std::size_t   kStreetMin       = 2;         // two people make a chat
constexpr std::size_t   kStreetMax       = 4;
constexpr float         kStreetRadius    = 1.6f;      // closer than the gate's standing group: a few people talking
constexpr float         kStreetFromNpc   = 7.0f;      // the chat stands this far from the bank / auctioneer / mailbox, toward the player
constexpr float         kStreetApart     = 18.0f;     // two chats are at least this far apart
constexpr std::uint32_t kStreetStayMinMs = 180000u;   // a chat lasts 3 to 6 minutes, then they go about their business
constexpr std::uint32_t kStreetStayMaxMs = 360000u;

inline bool IsStreet(std::uint32_t ring) { return ring >= kStreetRing; }

// Where a chat by an NPC at (nx, ny) stands: kStreetFromNpc yards from it toward the player at (px, py) (so it is in the walkway he
// uses, not behind the counter), or along +x when he stands on the NPC.
inline Pt StreetCenter(float nx, float ny, float px, float py)
{
    float dx = px - nx, dy = py - ny;
    float const d = std::sqrt(dx * dx + dy * dy);
    if (d < 1.0f) { dx = 1.0f; dy = 0.0f; }
    else { dx /= d; dy /= d; }
    float const r = std::min(kStreetFromNpc, std::max(d - 2.0f, 3.0f));
    return { nx + dx * r, ny + dy * r };
}

// The gesture that goes with a line said face to face: a question, a laugh, a yes or a no, an exclamation, or plain talking.
constexpr std::uint16_t kGestureTalk = 1, kGestureQuestion = 6, kGestureLaugh = 11, kGestureExclaim = 5, kGestureYes = 273,
                        kGestureNo = 274, kGesturePoint = 25;
inline std::uint16_t GestureFor(std::string const& line)
{
    std::string l;
    for (char c : line) l += char(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
    std::string const padded = " " + l + " ";
    auto has = [&](char const* w)
    {
        std::string const word = std::string(" ") + w;
        for (std::size_t at = padded.find(word); at != std::string::npos; at = padded.find(word, at + 1))
        {
            char const after = padded[at + word.size()];
            if (!(after >= 'a' && after <= 'z'))
                return true;
        }
        return false;
    };
    if (has("lol") || has("haha") || has("hah") || has("lmao") || has("rofl") || has("hehe") || l.find("haha") != std::string::npos)
        return kGestureLaugh;
    if (!l.empty() && l.find('?') != std::string::npos)
        return kGestureQuestion;
    if (has("no") || has("nah") || has("nope") || has("never"))
        return kGestureNo;
    if (has("yes") || has("yeah") || has("yep") || has("sure") || has("aye") || has("agreed") || has("true") || has("exactly"))
        return kGestureYes;
    if (has("look") || has("over there") || has("that one") || has("see that"))
        return kGesturePoint;
    if (l.find('!') != std::string::npos)
        return kGestureExclaim;
    return kGestureTalk;
}

// Topics for a chat in the city.
inline char const* const kStreetTopics[] = {
    "auction house prices", "what you are saving gold for", "a mount someone just bought", "where to level next",
    "a dungeon group for tonight", "the bank being too small", "which profession pays", "the city guards",
    "a rare drop someone almost got", "joining a guild", "the enemy faction raiding a town", "an annoying quest with a bad drop rate",
    "the inn's food and ale", "which class is the most fun", "a duel someone just lost", "what you did today",
};
constexpr std::size_t kStreetTopicCount = sizeof(kStreetTopics) / sizeof(kStreetTopics[0]);

// ---- Talk -------------------------------------------------------------------------------------------------------------------------
// A script is a short exchange between two or three people. A line names its speaker (0, 1, 2: the first, second, third talker of the
// ring) and may carry {city}, {zone} (the zone outside the gate), {0} {1} {2} (the talkers' names). `faction`: 0 both, 1 Alliance,
// 2 Horde. `emote`: an emote the speaker plays with the line when standing (0 none).

struct Line { std::uint8_t who; char const* text; std::uint16_t emote; };
struct Script { std::uint8_t faction; std::uint8_t talkers; std::vector<Line> lines; };

constexpr std::uint16_t kTalk = 1, kBow = 2, kCheer = 4, kExclaim = 5, kQuestion = 6, kLaugh = 11, kPoint = 25, kYes = 273, kNo = 274;

inline std::vector<Script> const& Scripts()
{
    static std::vector<Script> const s = {
        { 0, 2, { { 0, "anyone know a good spot to grind around my level?", kQuestion },
                  { 1, "depends, what are you, mid 20s?", kTalk },
                  { 0, "26", 0 },
                  { 1, "try the camps in Stranglethorn once you hit 30, till then just quest", kTalk } } },
        { 0, 2, { { 0, "this fire is the only warm thing I've seen all week", kLaugh },
                  { 1, "you've clearly never been to Searing Gorge", kLaugh } } },
        { 0, 3, { { 0, "who's up for a duel after this?", kQuestion },
                  { 1, "not me, I just repaired", kNo },
                  { 2, "I'll fight you {0}, loser buys the next round", kPoint },
                  { 0, "deal", kYes } } },
        { 0, 2, { { 0, "how much did you get your mount for?", kQuestion },
                  { 1, "still saving, 90 gold is no joke", kTalk },
                  { 0, "the auction house is where I lose all mine", kLaugh } } },
        { 0, 3, { { 0, "heard someone saw Onyxia's lair get cleared again", kTalk },
                  { 1, "one day that'll be us", kCheer },
                  { 2, "one day I'll have a full set of blues first", kLaugh } } },
        { 0, 2, { { 0, "long day out there", kTalk },
                  { 1, "aye. sit, rest a bit, the road will still be there", kYes } } },
        { 0, 2, { { 0, "anyone need anything crafted? I've got tailoring", kQuestion },
                  { 1, "could use a few bags if you've got the cloth for it", kTalk },
                  { 0, "bring me the cloth and it's yours", kYes } } },
        { 0, 3, { { 0, "lost my corpse twice in the same cave today", kNo },
                  { 1, "which cave", kQuestion },
                  { 0, "does it matter, they're all full of kobolds", kLaugh },
                  { 2, "you will not take my candle", kLaugh } } },
        { 0, 2, { { 0, "whats your hearthstone set to?", kQuestion },
                  { 1, "here, never bothered to change it", kTalk } } },
        { 0, 2, { { 0, "fishing is so relaxing after a dungeon", kTalk },
                  { 1, "relaxing? I fished for an hour and got one boot", kLaugh } } },
        { 0, 3, { { 0, "Deadmines or Wailing Caverns tonight?", kQuestion },
                  { 1, "whichever has a healer", kLaugh },
                  { 2, "I can heal, sort of", kTalk },
                  { 0, "sort of is good enough", kYes } } },
        { 0, 2, { { 0, "the stars out here are something else", kPoint },
                  { 1, "beats staring at a wall in the inn", kYes } } },
        { 0, 2, { { 0, "what's everyone doing for the weekend", kQuestion },
                  { 1, "farming herbs, as always", kTalk },
                  { 0, "riveting", kLaugh } } },
        { 0, 3, { { 0, "anyone seen a guild recruiting around here", kQuestion },
                  { 1, "they all want you to be 60 already", kNo },
                  { 2, "start your own, we'll join", kCheer } } },
        { 0, 2, { { 0, "you smell like a gnoll", kLaugh },
                  { 1, "spent the morning in a gnoll camp, what's your excuse", kLaugh } } },
        { 0, 2, { { 0, "this stew is terrible", kNo },
                  { 1, "you made it", kPoint },
                  { 0, "and I stand by my words", kLaugh } } },
        { 0, 3, { { 0, "fun fact, I have never once found a green I could use", kTalk },
                  { 1, "of the Monkey, every time", kLaugh },
                  { 2, "I got a Bear one yesterday, on a mage", kLaugh } } },
        { 0, 2, { { 0, "is there anything good in {zone} at all", kQuestion },
                  { 1, "the view, and the road out", kLaugh } } },
        { 0, 2, { { 0, "any tips for a new player?", kQuestion },
                  { 1, "train your weapon skills and never pull more than two", kTalk },
                  { 0, "noted", kYes } } },
        { 0, 3, { { 0, "was that a world boss earlier on the road?", kExclaim },
                  { 1, "just a level 62 elite wandering about", kTalk },
                  { 2, "just, he says", kLaugh } } },
        { 0, 2, { { 0, "{1} did you finish that quest chain yet", kQuestion },
                  { 1, "almost, need ten more of those feathers", kNo },
                  { 0, "the drop rate is criminal", kLaugh } } },
        { 0, 2, { { 0, "good night for a fire", kTalk },
                  { 1, "every night is a good night for a fire", kYes } } },
        // Alliance
        { 1, 2, { { 0, "for the Alliance", kCheer },
                  { 1, "and for a hot meal, ideally", kLaugh } } },
        { 1, 3, { { 0, "the guards in {city} never let me sit on the bench", kNo },
                  { 1, "you sat on it in plate", kLaugh },
                  { 2, "it was a nice bench", kTalk } } },
        { 1, 2, { { 0, "a dwarf told me the best ale is in Ironforge", kTalk },
                  { 1, "a dwarf would say that", kLaugh } } },
        { 1, 2, { { 0, "Defias spotted on the Westfall road again", kExclaim },
                  { 1, "when are they not", kNo } } },
        { 1, 3, { { 0, "Light keep you all", kBow },
                  { 1, "and you, friend", kBow },
                  { 2, "and keep the murlocs away from me", kLaugh } } },
        { 1, 2, { { 0, "anyone heading to Southshore?", kQuestion },
                  { 1, "only if there's a group, I don't trust the Hillsbrad road", kNo } } },
        // Horde
        { 2, 2, { { 0, "Lok'tar", kCheer },
                  { 1, "Lok'tar ogar", kCheer } } },
        { 2, 3, { { 0, "the Crossroads got raided again", kExclaim },
                  { 1, "every time I hearth there", kNo },
                  { 2, "we should make a raid of our own", kPoint } } },
        { 2, 2, { { 0, "good fire. reminds me of the old camps", kTalk },
                  { 1, "strength and honor", kBow } } },
        { 2, 2, { { 0, "Thrall walked by this morning, didn't even look at me", kTalk },
                  { 1, "he's busy, you're level 20", kLaugh } } },
        { 2, 3, { { 0, "anyone going to Tarren Mill?", kQuestion },
                  { 1, "only if I can bring a friend or five", kLaugh },
                  { 2, "count me in", kCheer } } },
        { 2, 2, { { 0, "the Barrens chat is at it again", kNo },
                  { 1, "where is Mankrik's wife", kLaugh } } },
    };
    return s;
}

// A script with lines that only make sense outside the gate, around a fire or under the sky.
inline bool OutdoorScript(Script const& s)
{
    static constexpr char const* kOutdoor[] = { "fire", "stew", "stars", "out here", "road", "the view", "sit, rest" };
    for (Line const& ln : s.lines)
        for (char const* w : kOutdoor)
            if (std::string_view(ln.text).find(w) != std::string_view::npos)
                return true;
    return false;
}

// The scripts a ring of `talkers` people on this side can say; `street`: none of the ones about the fire or the open road.
inline std::vector<std::size_t> ScriptsFor(bool alliance, std::size_t talkers, bool street = false)
{
    std::vector<std::size_t> out;
    auto const& all = Scripts();
    for (std::size_t i = 0; i < all.size(); ++i)
        if (all[i].talkers <= talkers && (all[i].faction == 0 || all[i].faction == (alliance ? 1u : 2u)) && !(street && OutdoorScript(all[i])))
            out.push_back(i);
    return out;
}

// A script line with its placeholders filled.
inline std::string Fill(char const* text, std::string const& city, std::string const& zone, std::vector<std::string> const& names)
{
    std::string out;
    for (char const* p = text; *p; ++p)
    {
        if (*p == '{')
        {
            char const* e = p + 1;
            while (*e && *e != '}') ++e;
            if (*e == '}')
            {
                std::string const key(p + 1, e);
                if (key == "city") out += city;
                else if (key == "zone") out += zone;
                else if (key.size() == 1 && key[0] >= '0' && key[0] <= '2')
                {
                    std::size_t const i = std::size_t(key[0] - '0');
                    out += i < names.size() ? names[i] : std::string("friend");
                }
                p = e;
                continue;
            }
        }
        out += *p;
    }
    return out;
}

// The pause before the next line, from the length of the one just said: time to read it, then type a reply. In [kLineMinMs, kLineMaxMs].
inline std::uint32_t LinePause(std::size_t said_bytes, std::uint32_t jitter)
{
    std::uint32_t const ms = 4500u + std::uint32_t(said_bytes) * 80u + (jitter % 3000u);
    return std::clamp(ms, kLineMinMs, kLineMaxMs);
}

// Topics for the chat model, one per conversation.
inline char const* const kTopics[] = {
    "a dungeon run that went badly", "the price of a mount", "the best place to level around your level", "a rare drop someone almost got",
    "professions and what they are good for", "a dragon or world boss rumoured nearby", "the long road you just travelled",
    "which class is the most fun", "a duel someone just lost", "the food cooking on the fire", "the weather and the stars tonight",
    "joining a guild", "the enemy faction raiding a town", "fishing", "an annoying quest with a bad drop rate", "old war stories",
};
constexpr std::size_t kTopicCount = sizeof(kTopics) / sizeof(kTopics[0]);

// ---- The board the idle rule reads ------------------------------------------------------------------------------------------------

struct Spot { std::uint32_t city = 0, ring = 0; float x = 0, y = 0, z = 0, face = 0; bool sit = false; };

class Board
{
public:
    static Board& Get() { static Board b; return b; }
    void Set(std::uint64_t bot, Spot const& s) { std::lock_guard<std::mutex> g(m_); by_bot_[bot] = s; }
    bool Find(std::uint64_t bot, Spot& out) const
    {
        std::lock_guard<std::mutex> g(m_);
        auto const it = by_bot_.find(bot);
        if (it == by_bot_.end()) return false;
        out = it->second;
        return true;
    }
    bool Has(std::uint64_t bot) const { std::lock_guard<std::mutex> g(m_); return by_bot_.contains(bot); }
    void Clear(std::uint64_t bot) { std::lock_guard<std::mutex> g(m_); by_bot_.erase(bot); }
    void ClearAll() { std::lock_guard<std::mutex> g(m_); by_bot_.clear(); }
private:
    mutable std::mutex m_;
    std::unordered_map<std::uint64_t, Spot> by_bot_;
};

} // namespace Playerbot::V2::Gathering
