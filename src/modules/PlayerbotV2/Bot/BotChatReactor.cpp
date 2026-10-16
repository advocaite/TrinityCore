// BotChatReactor — see header for scope. Implementation here resolves
// the keyword pattern, picks one bot to speak, and pushes a
// PartyChatIntent through the per-bot intent queue.

#include "BotChatReactor.h"
#include "BotConversation.h"

#include "BotAI.h"
#include "BotIntent.h"
#include "BotPersonality.h"
#include "BotRegistry.h"
#include "BotTextMgr.h"
#include "TradeTalk.h"
#include "BotTradeShop.h"
#include "BotTrade.h"
#include "BotTypes.h"
#include "../Services.h"
#include "../Threading/IntentQueue.h"
#include "../Util/ConfigReader.h"
#include "Random.h"
#include "Log.h"

#include "DB2Stores.h"     // sAreaTableStore for LocationQuery whisper reply
#include "Cell.h"          // SC-P1a grid-bounded say/yell range query
#include "CellImpl.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Group.h"
#include "GroupReference.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "QuestDef.h"
#include "World.h"        // sWorld locale + CONFIG_LISTEN_RANGE_* for say/yell
#include "WorldSession.h"
#include "GameTime.h"
#include "ObjectAccessor.h"
#include "ObjectGuid.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <list>
#include <mutex>
#include <string>
#include <utility>
#include <unordered_map>
#include <vector>

namespace Playerbot {

namespace {

// SC-P3b: the reaction-timestamp maps below are keyed by bot/area/guild ids
// and only ever grow — a key is inserted the first time that bot/area/guild
// reacts and is never removed. Over a long uptime with churn (bots cycling
// in and out, players roaming many areas, guilds forming/disbanding) these
// maps leak unboundedly. PruneStale sweeps entries whose timestamp is older
// than `keep_ms` (the relevant cooldown, doubled — anything that stale can
// never gate a future reaction, so dropping it is behavior-preserving). It
// is called under the map's own mutex at insert time, but ONLY when the map
// has grown past `threshold` so the common path stays O(1). `now_ms` is the
// monotonic GameTime tick; uint32 millis wrap every ~49.7 days but the
// `now - ts` subtraction is wrap-safe (modular arithmetic) and a wrap at
// worst spares one sweep, never corrupts state.
template <typename Map>
void PruneStale(Map& m, uint32_t now_ms, uint32_t keep_ms, size_t threshold)
{
    if (m.size() < threshold) return;
    for (auto it = m.begin(); it != m.end(); )
    {
        if (now_ms - it->second > keep_ms)
            it = m.erase(it);
        else
            ++it;
    }
}

// Sweep threshold for the reaction maps. A real server tops out at a few
// thousand distinct bots/areas/guilds reacting; 4096 keeps the maps small
// while making the sweep a rare event (it runs only once the map crosses
// the threshold, then again each insert until it drops back under).
constexpr size_t kReactMapPruneThreshold = 4096;

// Per-bot last-reaction timestamp (ms). Read+write from the chat handler
// thread; the world thread doesn't touch this map. Guarded by a small mutex.
// Chat fires at human typing rates so the contention here is negligible.
// SC-P3b: the single global mutex is intentionally NOT sharded. Reactions
// are driven by human chat cadence (a handful per second server-wide even at
// 2000 bots), so the critical section — a hash probe + a rare bounded sweep
// — is never hot. Sharding would add complexity (per-shard prune, key→shard
// hashing) for no measured win; revisit only if a profile shows g_react_mtx
// contention, which chat traffic cannot realistically produce.
std::mutex                                g_react_mtx;
std::unordered_map<BotId, uint32_t>       g_last_react_ms;

constexpr uint32_t kPerBotReactCooldownMs = 90u * 1000u;   // one reply per 90s per bot

// SC-P1b: addressed reactions (someone said the bot's name, or whispered it
// directly) get a SHORTER cooldown than ambient chatter — staying silent
// when called by name for 90s reads as broken — but they are NO LONGER
// exempt from the cooldown entirely. The old code bypassed the throttle for
// addressed replies AND never wrote the reaction timestamp, so a duo could
// both answer and a name spammer could trigger a reply on every line. With a
// 20s addressed window + an always-written timestamp, a bot answers a direct
// address promptly but not on back-to-back lines.
constexpr uint32_t kAddressedReactCooldownMs = 20u * 1000u;

// SC-P1a: per-area cooldown for /say and /yell reactions. A crowded hub
// (Stormwind trade district, Valdrakken) can see dozens of /say lines per
// second; without an area-scoped gate the bots there would chatter
// constantly. Keyed by (map_id<<32 | area_id). 8s lets a hub feel alive
// without turning into a wall of bot replies.
std::mutex                                  g_area_react_mtx;
std::unordered_map<uint64_t, uint32_t>      g_last_area_react_ms;
constexpr uint32_t kPerAreaReactCooldownMs  = 8u * 1000u;

// Lowercase + collapse whitespace to a single space. Strips leading/trailing
// whitespace and ASCII punctuation that would otherwise wreck keyword
// matching ("ty!" → "ty", "thx," → "thx"). Returns at most 256 chars; longer
// messages are truncated for matching purposes only (the bot's reply doesn't
// depend on input length).
std::string Normalize(std::string const& in)
{
    std::string out;
    out.reserve(std::min<size_t>(in.size(), 256));
    bool last_was_space = true;
    for (char c : in)
    {
        if (out.size() >= 256) break;
        unsigned char uc = static_cast<unsigned char>(c);
        // Treat punctuation-like ASCII as whitespace so "ty!" / "thx," normalize cleanly.
        if (uc == '!' || uc == '?' || uc == '.' || uc == ',' ||
            uc == ';' || uc == ':' || uc == '(' || uc == ')' ||
            uc == '\t' || uc == '\r' || uc == '\n' || uc == ' ')
        {
            if (!last_was_space) { out.push_back(' '); last_was_space = true; }
            continue;
        }
        out.push_back(char(std::tolower(uc)));
        last_was_space = false;
    }
    while (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}

// Returns true if normalized `hay` contains `needle` as a whole word.
// "ty" matches "ty", "ty everyone", " ty all" — not "pretty" or "country".
bool ContainsWord(std::string const& hay, char const* needle)
{
    const std::string n = needle;
    size_t pos = 0;
    while ((pos = hay.find(n, pos)) != std::string::npos)
    {
        const bool left_ok  = (pos == 0) || hay[pos - 1] == ' ';
        const bool right_ok = (pos + n.size() == hay.size()) || hay[pos + n.size()] == ' ';
        if (left_ok && right_ok) return true;
        pos += n.size();
    }
    return false;
}

enum class ReactKind : uint8_t
{
    None,
    Thanks,        // ty / thx / thanks
    Grats,         // gz / grats / congratulations
    Laugh,         // lol / haha / lmao / xd
    Acknowledge,   // pull / go / wait / hold / inc / back / brb — behavioral cues
    Ready,         // ready / rdy — separate so reply can echo "rdy!" not just "k"
    Greeting,      // hi / hello / hey — small social opener
    DirectAddress, // bot's name appears as a whole word
    LocationQuery, // "where" / "wo bist du" — bot whispers back its current zone
};

ReactKind ClassifyMessage(std::string const& norm, std::string const& bot_first_name_lower)
{
    const bool addressed =
        !bot_first_name_lower.empty() && bot_first_name_lower.size() >= 3 &&
        ContainsWord(norm, bot_first_name_lower.c_str());

    // Includes a handful of non-English thanks tokens — the user's server is
    // German-leaning ("danke") and EU players mix DE/FR/ES freely in /p chat.
    const bool has_thanks =
        ContainsWord(norm, "ty")     || ContainsWord(norm, "thx") ||
        ContainsWord(norm, "thanks") || ContainsWord(norm, "thank") ||
        ContainsWord(norm, "tyvm")   || ContainsWord(norm, "danke") ||
        ContainsWord(norm, "merci")  || ContainsWord(norm, "gracias");
    const bool has_grats =
        ContainsWord(norm, "gz")     || ContainsWord(norm, "grats") ||
        ContainsWord(norm, "gratz")  || ContainsWord(norm, "congrats") ||
        ContainsWord(norm, "congratulations") ||
        // "Ding!" / "Ding 47" — bots and players announce a level-up
        // this way. Treat it as a Grats trigger so the rest of the group
        // congratulates on the ding. "Hark!" prefix from Roleplay-verbose
        // bot dings is also recognised. Without this, dings dropped into
        // dead air — humans congratulate every party-ding.
        ContainsWord(norm, "ding") || ContainsWord(norm, "hark");
    const bool has_laugh =
        ContainsWord(norm, "lol")  || ContainsWord(norm, "haha") ||
        ContainsWord(norm, "lmao") || ContainsWord(norm, "xd")   ||
        ContainsWord(norm, "rofl");
    // "gg" / "wp" / "good game" — universal after-kill or end-of-BG line.
    // Folded into Grats reply pool ("gz!"/"nice"/"well done") since the
    // sentiment is the same (acknowledging a finish).
    const bool has_gg =
        ContainsWord(norm, "gg")        || ContainsWord(norm, "wp") ||
        ContainsWord(norm, "wellplayed");
    // Behavioral cues: words real players type to coordinate the pull/pace
    // of the group. Bots don't execute these (the ;-prefix command path
    // does), but a one-line acknowledgement reads like a focused human
    // teammate instead of a silent NPC. "inc"/"incoming" is acknowledged
    // even though no defensive action follows — silence on "inc" feels
    // worse than a "k" while the bot keeps doing its job.
    const bool has_ack =
        ContainsWord(norm, "pull")    || ContainsWord(norm, "pulling") ||
        ContainsWord(norm, "go")      || ContainsWord(norm, "wait")    ||
        ContainsWord(norm, "hold")    || ContainsWord(norm, "stop")    ||
        ContainsWord(norm, "back")    || ContainsWord(norm, "regroup") ||
        ContainsWord(norm, "rg")      || ContainsWord(norm, "inc")     ||
        ContainsWord(norm, "incoming")|| ContainsWord(norm, "afk")     ||
        ContainsWord(norm, "brb");
    const bool has_ready =
        ContainsWord(norm, "ready")   || ContainsWord(norm, "rdy");
    // Greeting tokens — the kind of thing said when a player joins
    // /p or just opens the conversation. EU mix again (German/French/
    // Spanish hello variants). "yo" / "hey" / "hi" / "sup" are also
    // common in younger-skewed groups.
    const bool has_greeting =
        ContainsWord(norm, "hi")    || ContainsWord(norm, "hello") ||
        ContainsWord(norm, "hey")   || ContainsWord(norm, "yo")    ||
        ContainsWord(norm, "sup")   || ContainsWord(norm, "hallo") ||
        ContainsWord(norm, "bonjour") || ContainsWord(norm, "hola") ||
        ContainsWord(norm, "moin");
    // Location query — "where are you" / "wo bist du" / "where r u".
    // Triggers a zone-aware reply ("im in {zone}") instead of the generic
    // "?" direct-address response. SC-P1c: this is now answered on BOTH the
    // whisper AND the party path (when the bot is addressed by name) via the
    // shared BuildLocationReply() helper — previously the party-chat
    // PickReply had no LocationQuery case, so an addressed "where are you
    // Aron?" in /p returned silence. It is still only classified as a
    // LocationQuery when the bot is addressed (a bare party "where do we go
    // next?" must not trip a zone dump from every bot).
    const bool has_location_query =
        ContainsWord(norm, "where") || ContainsWord(norm, "wherer") ||
        ContainsWord(norm, "wo");

    // When a social keyword AND the bot's name are both present, the social
    // keyword wins ("gz Aron" → Aron replies "ty!", not "yes?"). When only
    // the name appears, the address reply ("?", "yes?") fires.
    if (addressed)
    {
        if (has_thanks)         return ReactKind::Thanks;
        if (has_grats || has_gg) return ReactKind::Grats;
        if (has_ready)          return ReactKind::Ready;
        if (has_laugh)          return ReactKind::Laugh;
        if (has_ack)            return ReactKind::Acknowledge;
        if (has_location_query) return ReactKind::LocationQuery;
        if (has_greeting)       return ReactKind::Greeting;
        return ReactKind::DirectAddress;
    }

    if (has_thanks)         return ReactKind::Thanks;
    if (has_grats || has_gg) return ReactKind::Grats;
    if (has_ready)          return ReactKind::Ready;
    if (has_laugh)          return ReactKind::Laugh;
    if (has_ack)            return ReactKind::Acknowledge;
    if (has_greeting)       return ReactKind::Greeting;

    return ReactKind::None;
}

// Sub-kind for the Acknowledge reply pool — lets the reply track the
// kind of cue (pause vs go vs incoming vs general) so a "wait" gets
// "ok holding" instead of a generic "k". Resolved at React-time from
// the normalized message and threaded into PickReply.
enum class AckFlavor : uint8_t { Generic, Pause, Go, Inc, Drink };

// SC-P2a: politeness-aware pool selector. Picks one of three pools by the
// bot's Politeness personality field. Rude bots are terse/curt, polite bots
// add please/thanks/smileys, neutral keeps the existing middle-ground tone.
// A pool may be empty (nullptr, 0) to fall through to neutral — used where a
// kind has no meaningful rude/polite variant.
inline char const* PickFromTier(Politeness pol, uint32 sel,
                                 char const* const* rude,  size_t rude_n,
                                 char const* const* neut,  size_t neut_n,
                                 char const* const* polite, size_t polite_n)
{
    char const* const* pool = neut;
    size_t n = neut_n;
    if (pol == Politeness::Rude   && rude   && rude_n)   { pool = rude;   n = rude_n; }
    if (pol == Politeness::Polite && polite && polite_n) { pool = polite; n = polite_n; }
    if (!pool || n == 0) { pool = neut; n = neut_n; }
    return pool[sel % n];
}
#define POOL(arr) (arr), (sizeof(arr) / sizeof((arr)[0]))

char const* PickReply(ReactKind k, bool addressed, uint32 sel,
                      AckFlavor ack = AckFlavor::Generic,
                      Politeness pol = Politeness::Neutral)
{
    switch (k)
    {
        case ReactKind::Thanks:
        {
            // Whether addressed by name or not, the bot was thanked → "you're welcome".
            static constexpr char const* kRude[]   = { "np", "k", "sure", "yw" };
            static constexpr char const* kNeut[]   = {
                "np", "yw!", "no problem", "anytime",
                "np :)", "happy to help", "no worries",
            };
            static constexpr char const* kPolite[] = {
                "you're welcome!", "happy to help :)", "anytime, friend",
                "my pleasure", "no problem at all :)", "glad to help!",
            };
            return PickFromTier(pol, sel, POOL(kRude), POOL(kNeut), POOL(kPolite));
        }
        case ReactKind::Grats:
        {
            // Addressee role: someone congratulated THIS bot → reply with thanks.
            // Otherwise: bot is congratulating a third party → reply with "gz!".
            if (addressed)
            {
                static constexpr char const* kRude[]   = { "ty", "thanks", "cheers" };
                static constexpr char const* kNeut[]   = {
                    "ty!", "thanks!", "thank you :)", "ty :)",
                    "thanks everyone!", "ty all!",
                };
                static constexpr char const* kPolite[] = {
                    "thank you so much! :)", "aw thanks everyone!",
                    "thank you, really appreciate it", "ty all, you're the best :)",
                };
                return PickFromTier(pol, sel, POOL(kRude), POOL(kNeut), POOL(kPolite));
            }
            static constexpr char const* kRude[]   = { "gz", "gg", "nice" };
            static constexpr char const* kNeut[]   = {
                "gz!", "grats!", "congrats!", "nice :)",
                "gz :)", "well done", "gg!", "wp!",
            };
            static constexpr char const* kPolite[] = {
                "congratulations! :)", "well deserved, gz!", "grats, that's awesome!",
                "nicely done :)", "huge gz!",
            };
            return PickFromTier(pol, sel, POOL(kRude), POOL(kNeut), POOL(kPolite));
        }
        case ReactKind::Laugh:
        {
            // Not politeness-sensitive — one neutral pool.
            static constexpr char const* kPool[] = {
                "lol", "haha", "hehe", ":)", "xD",
            };
            return kPool[sel % (sizeof(kPool) / sizeof(kPool[0]))];
        }
        case ReactKind::Acknowledge:
        {
            switch (ack)
            {
                case AckFlavor::Pause:
                {
                    constexpr char const* kPool[] = {
                        "ok holding", "k waiting", "holding", "ok",
                        "k", "wait, got it", "sure",
                    };
                    return kPool[sel % (sizeof(kPool) / sizeof(kPool[0]))];
                }
                case AckFlavor::Go:
                {
                    constexpr char const* kPool[] = {
                        "going", "let's go", "ok pulling", "k", "on it",
                        "incoming dmg", "engage",
                    };
                    return kPool[sel % (sizeof(kPool) / sizeof(kPool[0]))];
                }
                case AckFlavor::Inc:
                {
                    constexpr char const* kPool[] = {
                        "k watching", "got it", "on it", "ready",
                        "k", "watching",
                    };
                    return kPool[sel % (sizeof(kPool) / sizeof(kPool[0]))];
                }
                case AckFlavor::Drink:
                {
                    constexpr char const* kPool[] = {
                        "k drinking", "drinking too", "k",
                        "ok drink", "rest break", "k :)",
                    };
                    return kPool[sel % (sizeof(kPool) / sizeof(kPool[0]))];
                }
                case AckFlavor::Generic:
                default: break;
            }
            static constexpr char const* kRude[]   = { "k", "kk", "fine", "sure" };
            static constexpr char const* kNeut[]   = {
                "k", "ok", "on it", "got it", "k :)", "sure", "kk",
            };
            static constexpr char const* kPolite[] = {
                "sure thing :)", "on it!", "got it, thanks", "okay :)", "will do!",
            };
            return PickFromTier(pol, sel, POOL(kRude), POOL(kNeut), POOL(kPolite));
        }
        case ReactKind::Ready:
        {
            // Not politeness-sensitive.
            static constexpr char const* kPool[] = {
                "rdy", "ready", "rdy!", "yes", "yep", "ready :)", "rdy here",
            };
            return kPool[sel % (sizeof(kPool) / sizeof(kPool[0]))];
        }
        case ReactKind::DirectAddress:
        {
            static constexpr char const* kRude[]   = { "?", "what", "hm?", "yeah?" };
            static constexpr char const* kNeut[]   = {
                "?", "yes?", "hm?", "what's up?", "yeah?",
            };
            static constexpr char const* kPolite[] = {
                "yes? :)", "hey, what's up?", "yes, how can i help?", "hi! you called?",
            };
            return PickFromTier(pol, sel, POOL(kRude), POOL(kNeut), POOL(kPolite));
        }
        case ReactKind::Greeting:
        {
            static constexpr char const* kRude[]   = { "hi", "hey", "yo", "sup" };
            static constexpr char const* kNeut[]   = {
                "hi", "hi!", "hey", "hello", "yo",
                "hi :)", "heya", "hey there", "hi all",
            };
            static constexpr char const* kPolite[] = {
                "hi there! :)", "hello! :)", "hey, good to see you", "heya :)",
                "hi everyone!", "well met :)",
            };
            return PickFromTier(pol, sel, POOL(kRude), POOL(kNeut), POOL(kPolite));
        }
        case ReactKind::None:
        default:
            return nullptr;
    }
}
#undef POOL

// SC-P1c: shared zone-aware reply builder. Resolves the bot's current area
// from AreaTable.db2 and renders one of a few natural "im in {zone}"
// templates. Factored out of ReactWhisper so the party-chat path can answer
// an addressed LocationQuery too (previously PickReply had no LocationQuery
// case, so "where are you Aron?" in /p returned silence). `sel` chooses the
// template + (for politeness) tone. Falls back to a numeric area id when the
// locale string is empty (rare).
std::string BuildLocationReply(Player* bot, uint32 sel, Politeness pol)
{
    uint32 const area_id = bot->GetAreaId();
    std::string area_name;
    if (auto const* ae = sAreaTableStore.LookupEntry(area_id))
        area_name = ae->AreaName[sWorld->GetDefaultDbcLocale()];
    if (area_name.empty())
        area_name = "area " + std::to_string(area_id);

    static constexpr char const* kRude[]   = { "{}", "im in {}", "{} atm" };
    static constexpr char const* kNeut[]   = {
        "im in {}", "{} atm", "over in {}", "currently in {}",
    };
    static constexpr char const* kPolite[] = {
        "i'm over in {} :)", "currently in {}, come find me!",
        "im in {} right now :)", "you'll find me in {}",
    };
    char const* const* pool = kNeut;
    size_t n = sizeof(kNeut) / sizeof(kNeut[0]);
    if (pol == Politeness::Rude)   { pool = kRude;   n = sizeof(kRude) / sizeof(kRude[0]); }
    if (pol == Politeness::Polite) { pool = kPolite; n = sizeof(kPolite) / sizeof(kPolite[0]); }

    std::string t = pool[sel % n];
    auto const pos = t.find("{}");
    if (pos != std::string::npos)
        t.replace(pos, 2, area_name);
    else
        t = area_name;
    return t;
}

// Small talk answered from the bot's real data through BotConversation (how are you, where from, level, class, what doing,
// group, quest help, directions, professions, guild, jokes, goodbye, ...). Returns false when the line is none of those (the
// usual reply pools then apply) or when Playerbot.Chatter.Conversation is off.
V2::ConversationContext MakeConvContext(Player* bot, BotAI* ai, Player* sender)
{
    static constexpr char const* kClass[] = { "", "warrior", "paladin", "hunter", "rogue", "priest", "death knight", "shaman",
                                               "mage", "warlock", "monk", "druid", "demon hunter", "evoker" };
    V2::ConversationContext ctx;
    ctx.self_name   = bot->GetName();
    ctx.sender_name = sender->GetName();
    ctx.knows_sender = bot->GetGroup() && bot->GetGroup()->IsMember(sender->GetGUID());
    uint8 const cls = bot->GetClass();
    ctx.class_name  = cls < sizeof(kClass) / sizeof(kClass[0]) && kClass[cls][0] ? kClass[cls] : "adventurer";
    ctx.level       = bot->GetLevel();
    if (auto const* ae = sAreaTableStore.LookupEntry(bot->GetAreaId()))
        ctx.zone = ae->AreaName[sWorld->GetDefaultDbcLocale()];
    ctx.in_group = bot->GetGroup() != nullptr;
    ctx.activity = "just hanging around";
    if (ctx.in_group)
        ctx.activity = "running with a group";
    else if (ai)
    {
        switch (ai->activity_mode())
        {
            case BotAI::ActivityMode::Questing:      ctx.activity = "out questing"; break;
            case BotAI::ActivityMode::Professioning: ctx.activity = "running errands in town"; break;
            case BotAI::ActivityMode::Wandering:     ctx.activity = "just wandering around"; break;
        }
    }
    for (uint16 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
    {
        if (uint32 const qid = bot->GetQuestSlotQuestId(slot))
            if (Quest const* q = sObjectMgr->GetQuestTemplate(qid))
            {
                ctx.quest_title = q->GetLogTitle();
                break;
            }
    }
    ctx.guild_name = bot->GetGuildName();
    static constexpr std::pair<uint32, char const*> kProfessions[] = {
        { SKILL_ALCHEMY, "alchemy" }, { SKILL_BLACKSMITHING, "blacksmithing" }, { SKILL_ENCHANTING, "enchanting" },
        { SKILL_ENGINEERING, "engineering" }, { SKILL_HERBALISM, "herbalism" }, { SKILL_LEATHERWORKING, "leatherworking" },
        { SKILL_MINING, "mining" }, { SKILL_SKINNING, "skinning" }, { SKILL_TAILORING, "tailoring" },
    };
    for (auto const& pr : kProfessions)
        if (bot->HasSkill(pr.first))
            ctx.professions.emplace_back(pr.second);
    if (ai)
    {
        BotPersonality const& pers = ai->personality();
        ctx.tone = pers.politeness == Politeness::Rude ? V2::ConvTone::Rude
                 : pers.politeness == Politeness::Polite ? V2::ConvTone::Polite : V2::ConvTone::Neutral;
        ctx.length = pers.verbosity <= Verbosity::Terse ? V2::ConvLength::Terse
                   : pers.verbosity >= Verbosity::Chatty ? V2::ConvLength::Chatty : V2::ConvLength::Normal;
    }
    ctx.bot_seed = uint32(bot->GetGUID().GetCounter());
    return ctx;
}

bool BuildSmallTalkReply(Player* bot, BotAI* ai, std::string const& msg, Player* sender, uint32 sel, bool addressed,
                         std::string& out)
{
    if (!Services::Config().chatter_conversation())
        return false;
    return V2::ConvReply(MakeConvContext(bot, ai, sender), V2::ConvNormalize(msg), msg, sel, addressed, out);
}

// True when the message carries a pause cue ("wait"/"hold"/"stop"/"back"/
// "regroup") — real-player shorthand that means "everyone freeze, don't
// pull yet". back/regroup pause a bit longer because the group is
// typically reforming after a wipe or repositioning.
// Distinct from the broader Acknowledge classification because we only
// want to inject the BotAI pause for these specific tokens, not for
// "pull"/"go" (which mean the opposite) or "afk"/"brb" (which mean
// "the SPEAKER is gone", not "the group should hold").
bool IsPauseCue(std::string const& norm)
{
    return ContainsWord(norm, "wait")    ||
           ContainsWord(norm, "hold")    ||
           ContainsWord(norm, "stop")    ||
           ContainsWord(norm, "back")    ||
           ContainsWord(norm, "regroup") ||
           ContainsWord(norm, "rg");
}
// Drink cue — healer or caster announces a mana break. Triggers a
// LONGER pause (30s) than the standard wait cue because a real drink
// from full empty takes ~25s and you want the tank to wait through
// the whole regen, not just the first sip. Distinct cue lets the
// reply pool acknowledge specifically ("k drink") and not get mixed
// with the generic wait reply.
bool IsDrinkCue(std::string const& norm)
{
    return ContainsWord(norm, "drink") ||
           ContainsWord(norm, "drinking") ||
           ContainsWord(norm, "mb")  ||   // "mana break"
           ContainsWord(norm, "oom");     // "out of mana"
}
// "go" / "pull" / "pulling" — the inverse cue: clear any pause so a
// previous "wait" doesn't keep the group frozen indefinitely.
bool IsGoCue(std::string const& norm)
{
    return ContainsWord(norm, "go")      ||
           ContainsWord(norm, "pull")    ||
           ContainsWord(norm, "pulling") ||
           ContainsWord(norm, "engage");
}

// Pause window — 12s. Long enough to cover a typical "wait, healer is
// drinking" beat; short enough that a forgotten "wait" doesn't strand
// the group. Real players keep this kind of gate mental; the bot just
// uses a fixed timeout instead.
constexpr uint32_t kChatPauseMs = 12u * 1000u;

// First-token-only lowercased bot name. WoW character names are single-word
// in practice; this also lets matching work without case sensitivity.
std::string FirstNameLower(std::string const& name)
{
    std::string out;
    out.reserve(name.size());
    for (char c : name)
    {
        if (c == ' ' || c == '\t') break;
        out.push_back(char(std::tolower(static_cast<unsigned char>(c))));
    }
    return out;
}

// Verbosity gate. Returns false when the bot is too quiet to react.
bool VerbosityAllows(Verbosity v, ReactKind k)
{
    if (v == Verbosity::Silent) return false;
    if (v == Verbosity::Terse)
    {
        // Terse bots only answer direct address; they don't volunteer chatter.
        return k == ReactKind::DirectAddress;
    }
    return true;   // Normal / Chatty / Roleplay all react
}

// Stable selection: pick one bot from the candidates list by hash of
// (sender_guid_low ^ message_first_char ^ candidate_count). Same group
// + same speaker tends to get the same talker reply, giving the channel
// a recognizable "this bot speaks for the group" rhythm.
size_t PickReplier(uint32 sender_low, char first_char, size_t candidate_count)
{
    if (candidate_count == 0) return 0;
    const uint32 seed = sender_low ^ uint32(uint8(first_char)) * 2654435761u;
    return size_t(seed) % candidate_count;
}

// SC-P2a: human-pacing reply latency derived from the bot's personality
// instead of hardcoded read/type constants. response_delay_ms is the bot's
// baseline reaction time; response_jitter_ms is the random spread on top of
// it (a fast, decisive player has a small delay+jitter; a distracted one a
// larger one). A read component scales with the incoming message length
// (longer lines take longer to notice/read) and a type component scales with
// the reply length (~55ms/char) so a longer reply visibly takes longer to
// "type". Returns an absolute defer_until timestamp (now + total). `salt`
// (typically bot id) decorrelates the per-bot jitter so two bots don't fire
// in lockstep.
uint32 ComputeReplyDeferMs(uint32 now_ms, BotPersonality const& p,
                           size_t in_len, size_t reply_len, uint32 salt)
{
    const uint32 base   = p.response_delay_ms;
    const uint32 jitter = (p.response_jitter_ms == 0)
        ? 0u
        : (((salt * 2654435761u) ^ now_ms) % (uint32(p.response_jitter_ms) + 1u));
    const uint32 read_ms = uint32(in_len) * 30u;          // scan/read the line
    const uint32 type_ms = uint32(reply_len) * 55u + 120u; // type the reply
    return now_ms + base + jitter + read_ms + type_ms;
}

} // namespace

void BotChatReactor::React(Player* sender, Group* group, std::string const& msg)
{
    if (!sender || !group || msg.empty()) return;
    // Bot speakers never trigger reactions — prevents echo storms when
    // multiple V2 bots are in the same group.
    if (sender->GetSession() && sender->GetSession()->IsBot()) return;
    // Don't react to our own command messages (`;`-prefix handled by
    // BotCommandParser; the parser is upstream of us already in OnPartyChat).
    if (msg.front() == ';' || msg.front() == '.' || msg.front() == '/') return;

    const std::string norm = Normalize(msg);
    if (norm.size() < 2) return;

    // Build candidate list: bots in the group that are registered with V2
    // and whose verbosity allows reaction. Classify per-bot because the
    // direct-address check depends on each bot's name.
    struct Candidate
    {
        Player*    bot;
        BotId      id;
        BotAI*     ai;          // for personality (politeness / pacing) — SC-P2a
        ReactKind  kind;
        bool       addressed;   // bot's own name appeared in the message
    };
    std::vector<Candidate> candidates;
    candidates.reserve(8);

    BotRegistry& reg = Services::Registry();

    for (GroupReference const& itr : group->GetMembers())
    {
        Player* member = itr.GetSource();
        if (!member || member == sender) continue;
        if (!member->GetSession() || !member->GetSession()->IsBot()) continue;
        // Dead bots stay silent — a ghost saying "ty!" reads as broken. The
        // ghost will rejoin chatter once rezzed and snapshot ticks Idle.
        if (!member->IsAlive()) continue;
        const BotId mid = member->GetGUID().GetCounter();
        if (!reg.has(mid)) continue;

        BotAI* ai = reg.ai(mid);
        if (!ai) continue;
        const Verbosity v = ai->personality().verbosity;
        if (v == Verbosity::Silent) continue;

        const std::string first = FirstNameLower(member->GetName());
        ReactKind kind = ClassifyMessage(norm, first);
        if (kind == ReactKind::None) continue;
        if (!VerbosityAllows(v, kind)) continue;

        const bool addr = !first.empty() && first.size() >= 3 &&
                          ContainsWord(norm, first.c_str());
        candidates.push_back({member, mid, ai, kind, addr});
    }

    // Behavioral cue: inject a per-bot pause when the SPEAKER (a real
    // player) said wait/hold/stop/back/regroup, OR a shorter pause for
    // inc/incoming, OR a LONGER pause for drink/oom (mana break = full
    // regen takes ~25s). "go"/"pull" clears any pause so a stuck "wait"
    // can be lifted by saying "go".
    {
        const bool pause = IsPauseCue(norm);
        const bool go    = IsGoCue(norm);
        const bool inc   = ContainsWord(norm, "inc") ||
                           ContainsWord(norm, "incoming");
        const bool drink = IsDrinkCue(norm);
        if (pause || go || inc || drink)
        {
            BotRegistry& reg2 = Services::Registry();
            const uint32 now2 = GameTime::GetGameTimeMS();
            // "inc" earns a shorter window than wait/hold — it's an
            // alert, not an indefinite hold. 8s is enough to span the
            // typical incoming-enemy arrival without committing to a
            // long freeze. "drink" stretches the pause to 30s so a
            // healer drinking from empty actually finishes regen.
            constexpr uint32 kIncAlertMs   = 8u * 1000u;
            constexpr uint32 kDrinkPauseMs = 30u * 1000u;
            for (GroupReference const& itr : group->GetMembers())
            {
                Player* member = itr.GetSource();
                if (!member || member == sender) continue;
                if (!member->GetSession() || !member->GetSession()->IsBot()) continue;
                const BotId mid = member->GetGUID().GetCounter();
                if (!reg2.has(mid)) continue;
                if (BotAI* mai = reg2.ai(mid))
                {
                    // Priority: drink > pause > inc > go (longer windows
                    // override shorter; explicit "go" still clears).
                    if (drink)       mai->set_chat_pause_until_ms(now2 + kDrinkPauseMs);
                    else if (pause)  mai->set_chat_pause_until_ms(now2 + kChatPauseMs);
                    else if (inc)    mai->set_chat_pause_until_ms(now2 + kIncAlertMs);
                    else /* go */
                    {
                        mai->set_chat_pause_until_ms(0);
                        // DungeonFlow: only "go" opens the human-lead release window. "pull" / "pulling" / "engage" are the owner's own
                        // orders (their handlers decide what the autopilot may do); opening the window for every bot on them let the
                        // tank autopilot pull on and run back and the casters wander (Deadmines 2026-10-14).
                        if (ContainsWord(norm, "go"))
                            mai->release_owner_lead();
                    }
                }
            }
        }
    }

    if (candidates.empty()) return;

    // When ANY candidate is addressed (their name appeared), only addressed
    // bots reply — picking a random non-addressed bot would create the
    // "wrong bot answered" effect. Otherwise pick one bot stably by hash.
    bool any_addressed = false;
    for (auto const& c : candidates) if (c.addressed) { any_addressed = true; break; }

    const uint32 now_ms      = GameTime::GetGameTimeMS();
    const uint32 sender_low  = sender->GetGUID().GetCounter();
    const char first_ch      = norm.empty() ? '?' : norm[0];

    // Resolve Acknowledge flavor once for the whole reply batch — every
    // reply for this message uses the same cue context.
    AckFlavor ack_flavor = AckFlavor::Generic;
    if (IsDrinkCue(norm))                       ack_flavor = AckFlavor::Drink;
    else if (IsPauseCue(norm))                  ack_flavor = AckFlavor::Pause;
    else if (IsGoCue(norm))                     ack_flavor = AckFlavor::Go;
    else if (ContainsWord(norm, "inc") ||
             ContainsWord(norm, "incoming"))    ack_flavor = AckFlavor::Inc;

    // `extra_delay_ms` staggers a 2nd addressed bot so a duo doesn't reply on
    // the same tick (SC-P1b multi-name cap fallback).
    auto emit_reply = [&](Candidate const& c, uint32 extra_delay_ms)
    {
        // SC-P1b: tiered cooldown. Addressed reactions use a SHORTER window
        // (20s) than ambient chatter (90s) — silence when called by name
        // reads as broken — but they are NO LONGER cooldown-exempt, and the
        // timestamp is ALWAYS written. The old code skipped the throttle AND
        // the stamp for addressed replies, so a name-spammer triggered a
        // reply on every line and both bots in a duo answered repeatedly.
        const uint32 cd = c.addressed ? kAddressedReactCooldownMs
                                      : kPerBotReactCooldownMs;
        {
            std::lock_guard<std::mutex> lk(g_react_mtx);
            auto it = g_last_react_ms.find(c.id);
            if (it != g_last_react_ms.end() && now_ms - it->second < cd)
                return;
            // SC-P3b: bound map growth — sweep keys older than 2x the longest
            // per-bot cooldown before inserting (only fires past threshold).
            PruneStale(g_last_react_ms, now_ms, 2u * kPerBotReactCooldownMs,
                       kReactMapPruneThreshold);
            g_last_react_ms[c.id] = now_ms;   // always stamp on a real reply
        }

        const uint32 sel = (sender_low ^ uint32(c.id) ^ now_ms);
        const Politeness pol = c.ai ? c.ai->personality().politeness
                                    : Politeness::Neutral;
        // SC-P1c: an addressed LocationQuery in party chat now answers with
        // the bot's zone (shared BuildLocationReply), instead of returning
        // silent because PickReply has no LocationQuery case.
        std::string reply_str;
        if (BuildSmallTalkReply(c.bot, c.ai, msg, sender, sel, c.addressed, reply_str))
        {
        }
        else if (c.kind == ReactKind::LocationQuery)
        {
            reply_str = BuildLocationReply(c.bot, sel, pol);
        }
        else
        {
            // Named with nothing the keyword table knows: answer as Karrz's ike3 bots did
            // (question answers / "yes???" / "what?"), from the bot text table.
            if (c.kind == ReactKind::DirectAddress)
                reply_str = BotChatter::GenerateReply(c.bot->GetName(), msg, sender->GetName());
            if (reply_str.empty())
            {
                char const* reply = PickReply(c.kind, c.addressed, sel, ack_flavor, pol);
                if (!reply || !*reply) return;
                reply_str = reply;
            }
        }

        const size_t reply_len = reply_str.size();
        Intent it{};
        it.bot_id = c.id;
        it.body   = ChatIntent{PartyChatIntent{std::move(reply_str)}};
        // SC-P2a: personality-paced latency (see ComputeReplyDeferMs), plus a
        // per-replier stagger for multi-name lines.
        const BotPersonality& p = c.ai ? c.ai->personality() : DefaultPersonality();
        it.defer_until_ms = ComputeReplyDeferMs(now_ms, p, norm.size(),
                                                reply_len, uint32(c.id))
                            + extra_delay_ms;
        Services::Intents(c.id).push(std::move(it));
    };

    if (any_addressed)
    {
        // SC-P1b: cap a multi-name line to ONE replying bot, staggering a
        // 2nd by +1.5s. A line like "gz Aron and Bren" should not make both
        // answer simultaneously (reads as scripted); at most two reply, the
        // second a beat later. Beyond two, the rest stay silent.
        uint32 replied = 0;
        for (auto const& c : candidates)
        {
            if (!c.addressed) continue;
            if (replied >= 2) break;
            emit_reply(c, replied == 1 ? 1500u : 0u);
            ++replied;
        }
        return;
    }

    // Pick exactly one bot to speak for the group. PickReplier is stable
    // across messages so the same bot tends to be "the talker" of the group.
    const size_t idx = PickReplier(sender_low, first_ch, candidates.size());
    emit_reply(candidates[idx], 0u);
}

// Whisper social reactor. Real players reply when strangers whisper "hi"
// or "ty". The reactor is reused here: classify the message against the
// same keyword table and emit a whisper-back via the bot's intent queue.
// Distinct from ReactBot (party-chat) in that:
//   * Single target (no group iteration / candidate picking)
//   * Reply goes via WhisperIntent, not PartyChatIntent
//   * Same 90s per-bot throttle, but tracked separately so a whisper
//     from a player who never sees /p chat doesn't get blocked by the
//     bot's last /p reply
// Sender must be a real player. Bot self-talk and ;-prefix commands
// are filtered upstream in PlayerbotV2.cpp::OnWhisperReceived.
namespace {

// A companion is a bot grouped with a real player. Sets `sender_in_group` when `sender` is in that same group.
bool IsCompanionBot(Player* bot, Player* sender, bool& sender_in_group)
{
    sender_in_group = false;
    Group* group = bot->GetGroup();
    if (!group) return false;
    bool human = false;
    for (GroupReference const& itr : group->GetMembers())
    {
        Player* m = itr.GetSource();
        if (!m || m == bot) continue;
        if (m == sender) sender_in_group = true;
        if (m->GetSession() && !m->GetSession()->IsBot()) human = true;
    }
    return human;
}

} // namespace

// Trading by whisper: "what do you have" lists what the bot sells, "buy 2" / "buy <item>" orders from that list, and the order is
// filled in the trade window (API::trade_with). Returns true when the line was trade talk and has been answered.
static bool ReplyTradeTalk(Player* sender, Player* bot, BotAI* ai, std::string const& norm, uint32 now_ms)
{
    namespace TT = V2::TradeTalk;
    bool const ask = TT::IsInventoryQuestion(norm);
    TT::Order const order = ask ? TT::Order{} : TT::ParseOrder(norm);
    if (!ask && !order.is_order) return false;

    std::vector<std::string> lines;
    if (ask)
    {
        std::vector<TradeShop::Offer> offers = TradeShop::BuildOffers(bot);
        TradeShop::StoreList(bot, sender, offers);
        if (offers.empty())
            lines.push_back("sorry, i've got nothing to sell right now");
        else
        {
            lines.push_back("here's what i can sell:");
            for (std::string& l : TradeShop::Describe(offers)) lines.push_back(std::move(l));
            lines.push_back("whisper me \"buy 1\" (the number), then open a trade and put the gold in");
        }
    }
    else
    {
        std::vector<TradeShop::Offer> list;
        if (!TradeShop::GetList(bot, sender, list)) return false;   // no list shown yet: not about trading
        std::vector<TT::ListedItem> names;
        for (TradeShop::Offer const& o : list)
        {
            std::string n = o.name;
            std::transform(n.begin(), n.end(), n.begin(), [](unsigned char c) { return char(std::tolower(c)); });
            names.push_back({ std::move(n) });
        }
        int const idx = TT::ResolveOrder(order, names);
        if (idx < 0)
        {
            if (!order.buy_verb) return false;
            lines.push_back("i don't have that, ask me \"what do you have\" for my list");
        }
        else
        {
            TradeShop::Offer o;
            if (!TradeShop::PlaceOrder(bot, sender, size_t(idx), o) || !bot->GetItemByGuid(o.item))
                lines.push_back("sorry, i no longer have that one");
            else
                lines.push_back(o.name + (o.count > 1 ? " x" + std::to_string(o.count) : std::string()) + " is " + BotTrade::FormatMoney(o.price) +
                                ", open a trade and put that in");
        }
    }
    uint32 delay = ComputeReplyDeferMs(now_ms, ai->personality(), norm.size(), lines.front().size(), uint32(bot->GetGUID().GetCounter()));
    for (std::string& l : lines)
    {
        Intent it{};
        it.bot_id = bot->GetGUID().GetCounter();
        it.body   = ChatIntent{WhisperIntent{sender->GetName(), std::move(l)}};
        it.defer_until_ms = delay;
        Services::Intents(it.bot_id).push(std::move(it));
        delay += 1100u;
    }
    return true;
}

void BotChatReactor::ReactWhisper(Player* sender, Player* bot, std::string const& msg)
{
    if (!sender || !bot || msg.empty()) return;
    if (sender->GetSession() && sender->GetSession()->IsBot()) return;
    if (msg.front() == ';' || msg.front() == '.' || msg.front() == '/') return;
    const BotId mid = bot->GetGUID().GetCounter();
    BotRegistry& reg = Services::Registry();
    if (!reg.has(mid)) return;
    BotAI* ai = reg.ai(mid);
    if (!ai) return;
    const Verbosity v = ai->personality().verbosity;
    if (v == Verbosity::Silent || !bot->IsAlive())
    {
        TC_LOG_INFO("playerbot.v2", "[conv] {} <- {} whisper \"{}\": silent ({})", bot->GetName(), sender->GetName(), msg,
                    v == Verbosity::Silent ? "personality is Silent" : "bot is dead");
        return;
    }

    // Playerbot.Behavior.BotWhisperable: a companion is somebody's party member - a stranger whispering it gets a polite
    // refusal (once per 30 s per bot) instead of the chatty reply a free-roaming world bot gives.
    if (Services::Config().behavior_bot_whisperable())
    {
        bool in_group = false;
        if (IsCompanionBot(bot, sender, in_group) && !in_group)
        {
            static std::mutex s_mtx;
            static std::unordered_map<uint64, uint32> s_last;
            uint32 const now = GameTime::GetGameTimeMS();
            {
                std::lock_guard<std::mutex> lk(s_mtx);
                uint32& last = s_last[bot->GetGUID().GetCounter()];
                if (last != 0 && now - last < 30000u) return;
                last = now ? now : 1u;
            }
            static constexpr char const* kRefuse[] = {
                "sorry, i'm in a group right now :)", "hey, i'm busy with my party at the moment",
                "can't chat right now, i'm with my group", "sorry, i'm running with a group, maybe later!",
            };
            Intent it{};
            it.bot_id = mid;
            it.body   = ChatIntent{WhisperIntent{sender->GetName(), kRefuse[(uint32(mid) ^ now) % 4]}};
            it.defer_until_ms = now + 1000u + urand(0, 2500);
            Services::Intents(mid).push(std::move(it));
            return;
        }
    }
    const std::string norm = Normalize(msg);
    if (norm.size() < 2) return;
    if (ReplyTradeTalk(sender, bot, ai, norm, GameTime::GetGameTimeMS())) return;
    const std::string first_name = FirstNameLower(bot->GetName());
    ReactKind kind = ClassifyMessage(norm, first_name);
    // A whisper is always addressed to the bot, so a line the keyword table does not recognise
    // (and the command parser did not take) still gets an answer - ike3's ChatReplyAction fallback.
    if (kind == ReactKind::None)
    {
        if (!BotTextMgr::instance().Enabled() &&
            !(Services::Config().chatter_conversation() && V2::ConvUnderstands(V2::ConvNormalize(msg))))
            return;
        kind = ReactKind::DirectAddress;
    }
    if (!VerbosityAllows(v, kind))
    {
        TC_LOG_INFO("playerbot.v2", "[conv] {} <- {} whisper \"{}\": silent (personality verbosity {})", bot->GetName(),
                    sender->GetName(), msg, int(v));
        return;
    }

    // A whisper is a direct question from a person, so it is never ignored for a cooldown: only a flood (a line every
    // 1.5 s from the same player) is dropped, and a line answered before gets a "you already said that" instead of
    // silence. The stamp still goes into the shared react map so a party reaction right after does not double up.
    const uint32 now_ms = GameTime::GetGameTimeMS();
    uint32 repeat = 0;
    {
        static std::mutex s_w_mtx;
        struct WState { uint32 last_ms = 0; std::string last_norm; uint32 repeat = 0; };
        static std::unordered_map<uint64, WState> s_w;
        std::lock_guard<std::mutex> lk(s_w_mtx);
        if (s_w.size() > 4096)
            s_w.clear();
        WState& st = s_w[(uint64(mid) << 32) | uint64(sender->GetGUID().GetCounter())];
        if (st.last_ms != 0 && now_ms - st.last_ms < 1500u)
        {
            TC_LOG_INFO("playerbot.v2", "[conv] {} <- {} whisper \"{}\": dropped (flood guard, under 1.5 s since the last line)",
                        bot->GetName(), sender->GetName(), msg);
            return;
        }
        // the same wording again within 5 minutes is a repeat; anything else starts over
        if (st.last_norm == norm && now_ms - st.last_ms < 300000u)
            ++st.repeat;
        else
            st.repeat = 0;
        st.last_norm = norm;
        st.last_ms = now_ms ? now_ms : 1u;
        repeat = st.repeat;
    }
    {
        std::lock_guard<std::mutex> lk(g_react_mtx);
        PruneStale(g_last_react_ms, now_ms, 2u * kPerBotReactCooldownMs,
                   kReactMapPruneThreshold);
        g_last_react_ms[mid] = now_ms;
    }

    // Whisper-flavored ack — same pool selectors but DirectAddress
    // applies because every whisper is implicitly addressed to the bot.
    const uint32 sender_low = sender->GetGUID().GetCounter();
    const uint32 sel = sender_low ^ uint32(mid) ^ now_ms;
    // SC-P2a: personality politeness drives the reply tone.
    const Politeness pol = ai->personality().politeness;
    std::string reply_str;
    char const* source = "conversation";
    if (repeat > 0 && Services::Config().chatter_conversation())
    {
        reply_str = V2::ConvRepeatReply(MakeConvContext(bot, ai, sender), sel, repeat);
        source = "repeat";
    }
    else if (BuildSmallTalkReply(bot, ai, msg, sender, sel, /*addressed=*/true, reply_str))
    {
    }
    else if (kind == ReactKind::LocationQuery)
    {
        // SC-P1c: shared zone-aware reply builder (also used by the party
        // path now). Resolves the bot's current AreaTable.db2 zone.
        reply_str = BuildLocationReply(bot, sel, pol);
    }
    else
    {
        // ike3 lines: the generated reply for anything unrecognised or bare-named, and half the
        // time ike3's "hi %s" for a greeting.
        if (kind == ReactKind::DirectAddress)
            reply_str = BotChatter::GenerateReply(bot->GetName(), msg, sender->GetName());
        else if (kind == ReactKind::Greeting && BotTextMgr::instance().Enabled() && (sel & 1u))
            reply_str = BotTextMgr::instance().GetReply(ReplyType::Hello, sender->GetName());
        if (reply_str.empty())
        {
            char const* reply = PickReply(kind, /*addressed=*/true, sel,
                                          AckFlavor::Generic, pol);
            // A whisper always gets an answer: a kind with no pool falls back to the "yes?" pool.
            if (!reply || !*reply)
                reply = PickReply(ReactKind::DirectAddress, /*addressed=*/true, sel, AckFlavor::Generic, pol);
            if (!reply || !*reply)
            {
                TC_LOG_INFO("playerbot.v2", "[conv] {} <- {} whisper \"{}\": no reply (kind {} has no pool)", bot->GetName(),
                            sender->GetName(), msg, int(kind));
                return;
            }
            reply_str = reply;
            source = "pool";
        }
        else
            source = "ike3";
    }
    TC_LOG_INFO("playerbot.v2", "[conv] {} <- {} whisper \"{}\": {} intent={} repeat={} -> \"{}\"", bot->GetName(), sender->GetName(), msg,
                source, V2::ConvIntentName(V2::ConvClassify(V2::ConvNormalize(msg))), repeat, reply_str);
    Intent it{};
    it.bot_id = mid;
    it.body   = ChatIntent{WhisperIntent{sender->GetName(), reply_str}};
    // SC-P2a: human-pacing now derives from the bot's personality
    // (response_delay_ms + rng(response_jitter_ms)) plus length-scaled
    // read/type components, instead of the old hardcoded 500ms/60ms-per-char
    // constants. The Intent's defer_until_ms is honored by the DrainIntents
    // dispatcher (BotIntentExecutor.cpp) — the queued intent waits until now
    // passes the deadline before executing.
    it.defer_until_ms = ComputeReplyDeferMs(now_ms, ai->personality(),
                                            norm.size(), reply_str.size(), uint32(mid));
    Services::Intents(mid).push(std::move(it));
}

// Phase C.3 guild-chat reactor. Picks one online officer-or-better of
// the sender's guild and emits a contextual guild_chat reply through
// the bot's intent queue. Per-guild throttle prevents reaction storms
// when multiple players chat back-to-back.
void BotChatReactor::ReactGuild(Player* sender, uint64 guild_id, std::string const& msg)
{
    if (!sender || guild_id == 0 || msg.empty()) return;
    // Filter bot speakers — same echo-storm guard as React().
    if (sender->GetSession() && sender->GetSession()->IsBot()) return;
    // Skip command prefixes / addon traffic that isn't social chat.
    if (msg.front() == ';' || msg.front() == '.' || msg.front() == '/') return;

    Guild* g = sGuildMgr->GetGuildById(guild_id);
    if (!g) return;

    // Per-guild throttle (separate from per-bot to coexist with party
    // chat). 60s window — slightly looser than party's 90s because
    // guild chat has fewer total speakers per minute on average and
    // we want guild conversation to feel responsive.
    static std::mutex                            s_guild_react_mtx;
    static std::unordered_map<uint64, uint32_t>  s_last_guild_react_ms;
    constexpr uint32_t kPerGuildReactCooldownMs = 60u * 1000u;
    const uint32 now_ms = GameTime::GetGameTimeMS();
    {
        std::lock_guard<std::mutex> lk(s_guild_react_mtx);
        auto it = s_last_guild_react_ms.find(guild_id);
        if (it != s_last_guild_react_ms.end() &&
            now_ms - it->second < kPerGuildReactCooldownMs)
            return;
        // Defer the stamp until after we successfully pick a speaker —
        // if no eligible officer exists, the next message gets a real
        // chance. (Re-grab the mutex below.)
    }

    const std::string norm = Normalize(msg);
    if (norm.size() < 2) return;

    // Walk guild members; pick the highest-rank online bot whose
    // classifier produces a non-None reply for the message. Officer+
    // preferred (the "talker" role); falls back to any rank when no
    // officer is online.
    BotRegistry& reg = Services::Registry();
    struct GCand
    {
        Player*    bot;
        BotId      id;
        uint8      rank;
        ReactKind  kind;
        bool       addressed;
    };
    std::vector<GCand> candidates;
    candidates.reserve(8);

    for (auto const& [mguid, member] : g->GetMembers())
    {
        if (!member.IsOnline()) continue;
        if (mguid == sender->GetGUID()) continue;
        Player* mp = ObjectAccessor::FindConnectedPlayer(mguid);
        if (!mp) continue;
        if (!mp->GetSession() || !mp->GetSession()->IsBot()) continue;
        if (!mp->IsAlive()) continue;
        const BotId mid = mguid.GetCounter();
        if (!reg.has(mid)) continue;
        BotAI* ai = reg.ai(mid);
        if (!ai) continue;
        const Verbosity v = ai->personality().verbosity;
        if (v == Verbosity::Silent) continue;

        const std::string first = FirstNameLower(mp->GetName());
        ReactKind kind = ClassifyMessage(norm, first);
        if (kind == ReactKind::None) continue;
        if (!VerbosityAllows(v, kind)) continue;

        const bool addr = !first.empty() && first.size() >= 3 &&
                          ContainsWord(norm, first.c_str());
        candidates.push_back({mp, mid, static_cast<uint8>(member.GetRankId()),
                              kind, addr});
    }
    if (candidates.empty()) return;

    // Sort: addressed bots first, then by rank ascending (lower rank id
    // = higher real rank; GM=0 outranks Officer=1 etc.).
    std::sort(candidates.begin(), candidates.end(),
        [](GCand const& a, GCand const& b) {
            if (a.addressed != b.addressed) return a.addressed;
            return a.rank < b.rank;
        });

    const uint32 sender_low = sender->GetGUID().GetCounter();
    auto const& chosen = candidates.front();
    const uint32 sel = sender_low ^ uint32(chosen.id) ^ now_ms;
    char const* reply = PickReply(chosen.kind, chosen.addressed, sel);
    if (!reply || !*reply) return;

    // Stamp the throttle only AFTER confirming a reply will emit. The
    // earlier-commented intent ("defer the stamp until after we
    // successfully pick a speaker — if no eligible officer exists, the
    // next message gets a real chance") was contradicted by stamping
    // before the PickReply nullptr guard. With the guard added in
    // anticipation of future ReactKinds whose reply pool may be
    // conditional, an empty-reply path would otherwise silence the
    // entire guild for the full 60s cooldown without anyone speaking.
    {
        std::lock_guard<std::mutex> lk(s_guild_react_mtx);
        // SC-P3b: bound per-guild map growth — guilds form and disband over
        // a long uptime; sweep keys older than 2x the per-guild cooldown.
        PruneStale(s_last_guild_react_ms, now_ms, 2u * kPerGuildReactCooldownMs,
                   kReactMapPruneThreshold);
        s_last_guild_react_ms[guild_id] = now_ms;
    }

    Intent it{};
    it.bot_id = chosen.id;
    it.body   = ChatIntent{GuildChatIntent{std::string(reply)}};
    // Guild chat is typically less time-pressured than party. Slightly
    // longer read/type window (1–4s end-to-end).
    const uint32 reply_len = uint32(std::strlen(reply));
    const uint32 read_ms3  = 600u + uint32(norm.size()) * 40u;
    const uint32 type_ms3  = reply_len * 65u + 250u;
    const uint32 jitter3   = (uint32(chosen.id) * 2654435761u) % 800u;
    it.defer_until_ms = now_ms + read_ms3 + type_ms3 + jitter3;
    Services::Intents(chosen.id).push(std::move(it));
}

namespace {

// SC-P1a: shared /say + /yell reaction core. Anchored on the speaking
// player's position, runs a grid-bounded PlayerListSearcher within `range`
// (CONFIG_LISTEN_RANGE_SAY / _YELL — never a magic number), gathers nearby
// V2 bots, classifies the line per-bot, then lets at most ONE answer:
//   * If a bot was addressed by name → that bot wins.
//   * Otherwise → one bot chosen stably by the guid-hash PickReplier, with a
//     probabilistic fire gate so a hub of bots doesn't all qualify and
//     answer every line. `one_in_n` is the inverse fire chance for the
//     ambient (non-addressed) case — say uses a low N (answers often), yell
//     a high N (answers rarely, matching how players treat yells).
// Cooldowns: per-bot (tiered, via the same g_last_react_ms map as party /
// whisper) AND per-area (g_last_area_react_ms) so crowded hubs stay calm at
// scale. Yell/say replies are biased toward greetings, direct-name mentions
// and location questions (handled by the classifier + the channel parameter
// below): an ambient "lol" in /say from a stranger you can't see is NOT
// answered — only socially-directed lines are.
void ReactSpoken(Player* sender, std::string const& msg, bool is_yell,
                 uint32 one_in_n)
{
    if (!sender || msg.empty()) return;
    if (sender->GetSession() && sender->GetSession()->IsBot()) return;
    if (msg.front() == ';' || msg.front() == '.' || msg.front() == '/') return;

    const std::string norm = Normalize(msg);
    if (norm.size() < 2) return;

    // Range from server config — the same listen ranges the core uses to
    // decide who HEARS the say/yell, so a bot only answers what it could
    // actually have heard.
    float range = sWorld->getFloatConfig(
        is_yell ? CONFIG_LISTEN_RANGE_YELL : CONFIG_LISTEN_RANGE_SAY);
    // Playerbot.Behavior.WorldBotSayReply: only bots within ~25 yd of the speaker answer, with a random roll,
    // a 60 s per-bot / 8 s global cooldown and a 1-4 s delay (see below).
    bool const world_say = Services::Config().behavior_world_bot_say_reply();
    if (world_say && range > 25.0f) range = 25.0f;
    std::string const sender_first = FirstNameLower(sender->GetName());

    // Per-area cooldown gate, keyed by (map<<32 | area). Cheap pre-check
    // before the (more expensive) grid walk.
    const uint64 area_key =
        (uint64(sender->GetMapId()) << 32) | uint64(sender->GetAreaId());
    const uint32 now_ms = GameTime::GetGameTimeMS();
    {
        std::lock_guard<std::mutex> lk(g_area_react_mtx);
        auto it = g_last_area_react_ms.find(area_key);
        if (it != g_last_area_react_ms.end() &&
            now_ms - it->second < kPerAreaReactCooldownMs)
            return;
    }

    // Grid-bounded nearby-player query anchored on the speaker.
    std::list<Player*> nearby;
    Position const senderPos = sender->GetPosition();
    Trinity::AnyPlayerInPositionRangeCheck check(&senderPos, range, /*reqAlive=*/true);
    Trinity::PlayerListSearcher<Trinity::AnyPlayerInPositionRangeCheck>
        searcher(sender, nearby, check);
    Cell::VisitWorldObjects(sender, searcher, range);

    BotRegistry& reg = Services::Registry();
    struct SCand { Player* bot; BotId id; BotAI* ai; ReactKind kind; bool addressed; };
    std::vector<SCand> candidates;
    candidates.reserve(8);

    for (Player* member : nearby)
    {
        if (!member || member == sender) continue;
        if (!member->GetSession() || !member->GetSession()->IsBot()) continue;
        if (!member->IsAlive()) continue;
        const BotId mid = member->GetGUID().GetCounter();
        if (!reg.has(mid)) continue;
        BotAI* ai = reg.ai(mid);
        if (!ai) continue;
        const Verbosity v = ai->personality().verbosity;
        if (v == Verbosity::Silent) continue;
        if (world_say)
        {
            // Busy bots stay quiet: combat, battlegrounds/arenas and raid groups.
            if (member->IsInCombat() || member->InBattleground() || member->InArena()) continue;
            if (Group const* g = member->GetGroup())
                if (g->isRaidGroup()) continue;
        }

        const std::string first = FirstNameLower(member->GetName());
        ReactKind kind = ClassifyMessage(norm, first);
        bool question = false;
        if (kind == ReactKind::None && world_say && BotTextMgr::instance().Enabled())
        {
            // A short question, or the speaker's own name, from a player standing next to the bot.
            question = norm.size() <= 48 && (msg.back() == '?' || (sender_first.size() >= 3 && ContainsWord(norm, sender_first.c_str())));
            if (question) kind = ReactKind::DirectAddress;
        }
        if (kind == ReactKind::None) continue;
        if (!question && !VerbosityAllows(v, kind)) continue;
        if (question && v == Verbosity::Terse) continue;

        const bool addr = !first.empty() && first.size() >= 3 &&
                          ContainsWord(norm, first.c_str());
        // Ambient (non-addressed) bias: in open /say|/yell a real player only
        // answers socially-directed lines — a greeting, a "gz", or an "anyone
        // know where X is?" (LocationQuery). They don't reply to a random
        // "lol" or "k" yelled across the zone. Addressed lines always pass.
        if (!addr)
        {
            const bool worth_answering =
                question ||
                kind == ReactKind::Greeting ||
                kind == ReactKind::Grats    ||
                kind == ReactKind::Thanks   ||
                kind == ReactKind::LocationQuery;
            if (!worth_answering) continue;
        }
        candidates.push_back({member, mid, ai, kind, addr});
    }
    if (candidates.empty()) return;

    const uint32 sender_low = sender->GetGUID().GetCounter();
    const char   first_ch   = norm[0];

    // Resolve Acknowledge flavor (rare in /say, but keeps replies consistent).
    AckFlavor ack_flavor = AckFlavor::Generic;
    if (IsDrinkCue(norm))                       ack_flavor = AckFlavor::Drink;
    else if (IsPauseCue(norm))                  ack_flavor = AckFlavor::Pause;
    else if (IsGoCue(norm))                     ack_flavor = AckFlavor::Go;
    else if (ContainsWord(norm, "inc") ||
             ContainsWord(norm, "incoming"))    ack_flavor = AckFlavor::Inc;

    // Prefer an addressed bot; else pick one stably by hash and apply the
    // probabilistic fire gate so not every ambient line is answered.
    SCand const* chosen = nullptr;
    for (auto const& c : candidates)
        if (c.addressed) { chosen = &c; break; }
    if (!chosen)
    {
        // Probabilistic ambient gate. Deterministic per (sender, line) so the
        // same line doesn't flip-flop across re-entries. one_in_n controls
        // how chatty the channel is (say low, yell high).
        // The deterministic roll below never changes for the same player + first letter, so a given
        // "hi" was either always or never answered; the world-bot path rolls for real instead.
        if (world_say)
        {
            // 70% for a greeting, 40% for other lines; yells half of that.
            uint32 pct = (candidates.front().kind == ReactKind::Greeting) ? 70u : 40u;
            if (is_yell) pct /= 2;
            if (urand(0, 99) >= pct) return;
            chosen = &candidates[urand(0, uint32(candidates.size()) - 1)];
        }
        else
        {
            const uint32 roll = (sender_low ^ uint32(uint8(first_ch)) * 2654435761u);
            if (one_in_n > 1 && (roll % one_in_n) != 0) return;
            const size_t idx = PickReplier(sender_low, first_ch, candidates.size());
            chosen = &candidates[idx];
        }
    }

    // Per-bot tiered cooldown + always-stamp (SC-P1b parity).
    const uint32 cd = chosen->addressed ? kAddressedReactCooldownMs
                                        : (world_say ? 60u * 1000u : kPerBotReactCooldownMs);
    // Global cooldown (world-bot path): one say/yell answer per 8 s server-wide.
    static std::mutex s_global_mtx;
    static uint32 s_global_last_ms = 0;
    if (world_say)
    {
        std::lock_guard<std::mutex> lk(s_global_mtx);
        if (s_global_last_ms != 0 && now_ms - s_global_last_ms < 8000u)
            return;
    }
    {
        std::lock_guard<std::mutex> lk(g_react_mtx);
        auto it = g_last_react_ms.find(chosen->id);
        if (it != g_last_react_ms.end() && now_ms - it->second < cd)
            return;
        // SC-P3b: bound map growth (see PruneStale).
        PruneStale(g_last_react_ms, now_ms, 2u * kPerBotReactCooldownMs,
                   kReactMapPruneThreshold);
        g_last_react_ms[chosen->id] = now_ms;
    }
    // Stamp the per-area cooldown now that a reply is committed.
    {
        std::lock_guard<std::mutex> lk(g_area_react_mtx);
        // SC-P3b: areas are visited transiently (players roam); without a
        // sweep this map accretes a key per (map,area) ever spoken in. Keep
        // 2x the per-area cooldown.
        PruneStale(g_last_area_react_ms, now_ms, 2u * kPerAreaReactCooldownMs,
                   kReactMapPruneThreshold);
        g_last_area_react_ms[area_key] = now_ms;
    }

    const uint32 sel = sender_low ^ uint32(chosen->id) ^ now_ms;
    const Politeness pol = chosen->ai ? chosen->ai->personality().politeness
                                      : Politeness::Neutral;
    std::string reply_str;
    if (BuildSmallTalkReply(chosen->bot, chosen->ai, msg, sender, sel, chosen->addressed, reply_str))
    {
    }
    else if (chosen->kind == ReactKind::LocationQuery)
        reply_str = BuildLocationReply(chosen->bot, sel, pol);
    else
    {
        if (world_say && BotTextMgr::instance().Enabled())
        {
            // ike3 text pools: a greeting gets its 'hello' reply half the time, a question / bare name the generated reply.
            if (chosen->kind == ReactKind::DirectAddress)
                reply_str = BotChatter::GenerateReply(chosen->bot->GetName(), msg, sender->GetName());
            else if (chosen->kind == ReactKind::Greeting && (sel & 1u))
                reply_str = BotTextMgr::instance().GetReply(ReplyType::Hello, sender->GetName());
        }
        if (reply_str.empty())
        {
            char const* reply = PickReply(chosen->kind, chosen->addressed, sel,
                                          ack_flavor, pol);
            if (!reply || !*reply) return;
            reply_str = reply;
        }
    }
    if (world_say)
    {
        {
            std::lock_guard<std::mutex> lk(s_global_mtx);
            s_global_last_ms = now_ms ? now_ms : 1u;
        }
        TC_LOG_INFO("playerbot.v2", "[say_reply] {} -> {}: {}", chosen->bot->GetName(), sender->GetName(), reply_str);
    }

    // The bot answers in the SAME channel it heard (say↔say, yell↔yell), via
    // the racial-language-aware say()/yell() API path.
    const size_t reply_len = reply_str.size();
    Intent it{};
    it.bot_id = chosen->id;
    if (is_yell)
        it.body = ChatIntent{YellChatIntent{std::move(reply_str)}};
    else
        it.body = ChatIntent{SayChatIntent{std::move(reply_str)}};
    const BotPersonality& p = chosen->ai ? chosen->ai->personality()
                                         : DefaultPersonality();
    it.defer_until_ms = world_say ? now_ms + 1000u + urand(0, 3000)
                                  : ComputeReplyDeferMs(now_ms, p, norm.size(),
                                                        reply_len, uint32(chosen->id));
    uint32 const say_defer_ms = it.defer_until_ms;
    Services::Intents(chosen->id).push(std::move(it));

    // Playerbot.Behavior.SocialEmotes: a hello from a player within 12 yd is answered with a wave as well.
    if (world_say && !is_yell && chosen->kind == ReactKind::Greeting && Services::Config().behavior_social_emotes() &&
        chosen->bot->IsWithinDist(sender, 12.0f))
    {
        Intent wave{};
        wave.bot_id = chosen->id;
        wave.body = PerformEmoteIntent{3 /*EMOTE_ONESHOT_WAVE*/, sender->GetGUID()};
        wave.defer_until_ms = say_defer_ms + urand(100, 600);
        Services::Intents(chosen->id).push(std::move(wave));
    }
}

} // namespace

void BotChatReactor::ReactSay(Player* sender, std::string const& msg)
{
    // Say is answered relatively often (1-in-2 ambient gate) since /say is
    // close-range and conversational.
    ReactSpoken(sender, msg, /*is_yell=*/false, /*one_in_n=*/2u);
}

void BotChatReactor::ReactYell(Player* sender, std::string const& msg)
{
    // Yell is answered rarely (1-in-6 ambient gate) — players mostly ignore
    // zone yells unless directly named (addressed bots still bypass the gate).
    ReactSpoken(sender, msg, /*is_yell=*/true, /*one_in_n=*/6u);
}

// SC-P2c: reciprocate a nearby player's text emote. A targeted emote (wave AT
// a specific bot) is answered by THAT bot; an untargeted one by a single
// nearby bot picked by hash. The reply is a reciprocal PerformEmoteIntent on
// a human-paced delay with the same per-bot cooldown namespace.
// ike3 EmoteActionBase::ReceiveEmote (EmoteAction.cpp:150): the answer a bot gives to a text emote
// aimed at it - a line from the emote_reply_* texts (empty key = animation only) and the animation
// it plays. Entries ike3 commented out, or tied to its strategy changes (beckon/follow), are left out.
namespace {

struct EmoteAnswer { uint32 text_emote; char const* key; uint32 anim; };
constexpr EmoteAnswer kEmoteAnswers[] = {
    { TEXT_EMOTE_BONK,         nullptr,                 EMOTE_ONESHOT_CRY },
    { TEXT_EMOTE_SALUTE,       nullptr,                 EMOTE_ONESHOT_SALUTE },
    { TEXT_EMOTE_WAVE,         "emote_reply_greet",     EMOTE_ONESHOT_WAVE },
    { TEXT_EMOTE_GREET,        "emote_reply_greet",     EMOTE_ONESHOT_WAVE },
    { TEXT_EMOTE_HAIL,         "emote_reply_greet",     EMOTE_ONESHOT_WAVE },
    { TEXT_EMOTE_HELLO,        "emote_reply_greet",     EMOTE_ONESHOT_WAVE },
    { TEXT_EMOTE_WELCOME,      "emote_reply_greet",     EMOTE_ONESHOT_WAVE },
    { TEXT_EMOTE_INTRODUCE,    "emote_reply_greet",     EMOTE_ONESHOT_WAVE },
    { TEXT_EMOTE_DANCE,        "emote_reply_dance",     EMOTE_ONESHOT_DANCE },
    { TEXT_EMOTE_FLIRT,        "emote_reply_love",      EMOTE_ONESHOT_SHY },
    { TEXT_EMOTE_KISS,         "emote_reply_love",      EMOTE_ONESHOT_SHY },
    { TEXT_EMOTE_HUG,          "emote_reply_love",      EMOTE_ONESHOT_SHY },
    { TEXT_EMOTE_BLUSH,        "emote_reply_love",      EMOTE_ONESHOT_SHY },
    { TEXT_EMOTE_SMILE,        "emote_reply_love",      EMOTE_ONESHOT_SHY },
    { TEXT_EMOTE_LOVE,         "emote_reply_love",      EMOTE_ONESHOT_SHY },
    { TEXT_EMOTE_FLEX,         "emote_reply_flex",      EMOTE_ONESHOT_APPLAUD },
    { TEXT_EMOTE_ANGRY,        "emote_reply_angry",     EMOTE_ONESHOT_QUESTION },
    { TEXT_EMOTE_GLARE,        "emote_reply_angry",     EMOTE_ONESHOT_QUESTION },
    { TEXT_EMOTE_BLAME,        "emote_reply_angry",     EMOTE_ONESHOT_QUESTION },
    { TEXT_EMOTE_FART,         "emote_reply_stink",     EMOTE_ONESHOT_POINT },
    { TEXT_EMOTE_BURP,         "emote_reply_stink",     EMOTE_ONESHOT_POINT },
    { TEXT_EMOTE_GASP,         "emote_reply_stink",     EMOTE_ONESHOT_POINT },
    { TEXT_EMOTE_NOSEPICK,     "emote_reply_stink",     EMOTE_ONESHOT_POINT },
    { TEXT_EMOTE_SNIFF,        "emote_reply_stink",     EMOTE_ONESHOT_POINT },
    { TEXT_EMOTE_STINK,        "emote_reply_stink",     EMOTE_ONESHOT_POINT },
    { TEXT_EMOTE_JOKE,         "emote_reply_joke",      EMOTE_ONESHOT_LAUGH },
    { TEXT_EMOTE_CHICKEN,      "emote_reply_chicken",   EMOTE_ONESHOT_RUDE },
    { TEXT_EMOTE_APOLOGIZE,    "emote_reply_apologize", EMOTE_ONESHOT_POINT },
    { TEXT_EMOTE_APPLAUD,      "emote_reply_applaud",   EMOTE_ONESHOT_BOW },
    { TEXT_EMOTE_CLAP,         "emote_reply_applaud",   EMOTE_ONESHOT_BOW },
    { TEXT_EMOTE_CONGRATULATE, "emote_reply_applaud",   EMOTE_ONESHOT_BOW },
    { TEXT_EMOTE_HAPPY,        "emote_reply_applaud",   EMOTE_ONESHOT_BOW },
    { TEXT_EMOTE_BEG,          "emote_reply_beg",       EMOTE_ONESHOT_NO },
    { TEXT_EMOTE_GROVEL,       "emote_reply_beg",       EMOTE_ONESHOT_NO },
    { TEXT_EMOTE_PLEAD,        "emote_reply_beg",       EMOTE_ONESHOT_NO },
    { TEXT_EMOTE_BITE,         "emote_reply_bite",      EMOTE_ONESHOT_ROAR },
    { TEXT_EMOTE_POKE,         "emote_reply_bite",      EMOTE_ONESHOT_ROAR },
    { TEXT_EMOTE_SCRATCH,      "emote_reply_bite",      EMOTE_ONESHOT_ROAR },
    { TEXT_EMOTE_BORED,        "emote_reply_bored",     EMOTE_ONESHOT_NO },
    { TEXT_EMOTE_BOW,          nullptr,                 EMOTE_ONESHOT_BOW },
    { TEXT_EMOTE_CURTSEY,      nullptr,                 EMOTE_ONESHOT_BOW },
    { TEXT_EMOTE_BRB,          "emote_reply_brb",       EMOTE_ONESHOT_EAT },
    { TEXT_EMOTE_SIT,          "emote_reply_brb",       EMOTE_ONESHOT_EAT },
    { TEXT_EMOTE_AGREE,        "emote_reply_agree",     EMOTE_ONESHOT_EXCLAMATION },
    { TEXT_EMOTE_NOD,          "emote_reply_agree",     EMOTE_ONESHOT_EXCLAMATION },
    { TEXT_EMOTE_AMAZE,        "emote_reply_amaze",     EMOTE_ONESHOT_FLEX },
    { TEXT_EMOTE_COWER,        "emote_reply_amaze",     EMOTE_ONESHOT_FLEX },
    { TEXT_EMOTE_CRINGE,       "emote_reply_amaze",     EMOTE_ONESHOT_FLEX },
    { TEXT_EMOTE_EYE,          "emote_reply_amaze",     EMOTE_ONESHOT_FLEX },
    { TEXT_EMOTE_KNEEL,        "emote_reply_amaze",     EMOTE_ONESHOT_FLEX },
    { TEXT_EMOTE_PEER,         "emote_reply_amaze",     EMOTE_ONESHOT_FLEX },
    { TEXT_EMOTE_SURRENDER,    "emote_reply_amaze",     EMOTE_ONESHOT_FLEX },
    { TEXT_EMOTE_PRAISE,       "emote_reply_amaze",     EMOTE_ONESHOT_FLEX },
    { TEXT_EMOTE_SCARED,       "emote_reply_amaze",     EMOTE_ONESHOT_FLEX },
    { TEXT_EMOTE_COMMEND,      "emote_reply_amaze",     EMOTE_ONESHOT_FLEX },
    { TEXT_EMOTE_BLEED,        "emote_reply_bleed",     EMOTE_ONESHOT_KNEEL },
    { TEXT_EMOTE_MOURN,        "emote_reply_bleed",     EMOTE_ONESHOT_KNEEL },
    { TEXT_EMOTE_FLOP,         "emote_reply_bleed",     EMOTE_ONESHOT_KNEEL },
    { TEXT_EMOTE_BLINK,        "emote_reply_blink",     EMOTE_ONESHOT_KICK },
    { TEXT_EMOTE_BOUNCE,       "emote_reply_bark",      EMOTE_ONESHOT_POINT },
    { TEXT_EMOTE_BARK,         "emote_reply_bark",      EMOTE_ONESHOT_POINT },
    { TEXT_EMOTE_BYE,          "emote_reply_bye",       EMOTE_ONESHOT_WAVE },
    { TEXT_EMOTE_CACKLE,       "emote_reply_laugh",     EMOTE_ONESHOT_LAUGH },
    { TEXT_EMOTE_LAUGH,        "emote_reply_laugh",     EMOTE_ONESHOT_LAUGH },
    { TEXT_EMOTE_CHUCKLE,      "emote_reply_laugh",     EMOTE_ONESHOT_LAUGH },
    { TEXT_EMOTE_GIGGLE,       "emote_reply_laugh",     EMOTE_ONESHOT_LAUGH },
    { TEXT_EMOTE_GUFFAW,       "emote_reply_laugh",     EMOTE_ONESHOT_LAUGH },
    { TEXT_EMOTE_ROFL,         "emote_reply_laugh",     EMOTE_ONESHOT_LAUGH },
    { TEXT_EMOTE_SNICKER,      "emote_reply_laugh",     EMOTE_ONESHOT_LAUGH },
    { TEXT_EMOTE_CONFUSED,     "emote_reply_confused",  EMOTE_ONESHOT_QUESTION },
    { TEXT_EMOTE_CURIOUS,      "emote_reply_confused",  EMOTE_ONESHOT_QUESTION },
    { TEXT_EMOTE_FIDGET,       "emote_reply_confused",  EMOTE_ONESHOT_QUESTION },
    { TEXT_EMOTE_FROWN,        "emote_reply_confused",  EMOTE_ONESHOT_QUESTION },
    { TEXT_EMOTE_SHRUG,        "emote_reply_confused",  EMOTE_ONESHOT_QUESTION },
    { TEXT_EMOTE_SIGH,         "emote_reply_confused",  EMOTE_ONESHOT_QUESTION },
    { TEXT_EMOTE_STARE,        "emote_reply_confused",  EMOTE_ONESHOT_QUESTION },
    { TEXT_EMOTE_TAP,          "emote_reply_confused",  EMOTE_ONESHOT_QUESTION },
    { TEXT_EMOTE_SURPRISED,    "emote_reply_confused",  EMOTE_ONESHOT_QUESTION },
    { TEXT_EMOTE_WHINE,        "emote_reply_confused",  EMOTE_ONESHOT_QUESTION },
    { TEXT_EMOTE_BOGGLE,       "emote_reply_confused",  EMOTE_ONESHOT_QUESTION },
    { TEXT_EMOTE_LOST,         "emote_reply_confused",  EMOTE_ONESHOT_QUESTION },
    { TEXT_EMOTE_PONDER,       "emote_reply_confused",  EMOTE_ONESHOT_QUESTION },
    { TEXT_EMOTE_SNUB,         "emote_reply_confused",  EMOTE_ONESHOT_QUESTION },
    { TEXT_EMOTE_SERIOUS,      "emote_reply_confused",  EMOTE_ONESHOT_QUESTION },
    { TEXT_EMOTE_EYEBROW,      "emote_reply_confused",  EMOTE_ONESHOT_QUESTION },
    { TEXT_EMOTE_COUGH,        "emote_reply_germs",     EMOTE_ONESHOT_POINT },
    { TEXT_EMOTE_DROOL,        "emote_reply_germs",     EMOTE_ONESHOT_POINT },
    { TEXT_EMOTE_SPIT,         "emote_reply_germs",     EMOTE_ONESHOT_POINT },
    { TEXT_EMOTE_LICK,         "emote_reply_germs",     EMOTE_ONESHOT_POINT },
    { TEXT_EMOTE_BREATH,       "emote_reply_germs",     EMOTE_ONESHOT_POINT },
    { TEXT_EMOTE_CRY,          "emote_reply_cry",       EMOTE_ONESHOT_CRY },
    { TEXT_EMOTE_CRACK,        "emote_reply_crack",     EMOTE_ONESHOT_ROAR },
    { TEXT_EMOTE_EAT,          "emote_reply_eat",       EMOTE_ONESHOT_EAT },
    { TEXT_EMOTE_DRINK,        "emote_reply_eat",       EMOTE_ONESHOT_EAT },
    { TEXT_EMOTE_GLOAT,        "emote_reply_gloat",     EMOTE_ONESHOT_CRY },
    { TEXT_EMOTE_MOCK,         "emote_reply_gloat",     EMOTE_ONESHOT_CRY },
    { TEXT_EMOTE_TEASE,        "emote_reply_gloat",     EMOTE_ONESHOT_CRY },
    { TEXT_EMOTE_EMBARRASS,    "emote_reply_gloat",     EMOTE_ONESHOT_CRY },
    { TEXT_EMOTE_HUNGRY,       "emote_reply_hungry",    EMOTE_ONESHOT_EAT },
    { TEXT_EMOTE_LAYDOWN,      "emote_reply_tired",     EMOTE_ONESHOT_KNEEL },
    { TEXT_EMOTE_TIRED,        "emote_reply_tired",     EMOTE_ONESHOT_KNEEL },
    { TEXT_EMOTE_YAWN,         "emote_reply_tired",     EMOTE_ONESHOT_KNEEL },
    { TEXT_EMOTE_MOAN,         "emote_reply_sexy",      EMOTE_ONESHOT_NO },
    { TEXT_EMOTE_MOON,         "emote_reply_sexy",      EMOTE_ONESHOT_NO },
    { TEXT_EMOTE_SEXY,         "emote_reply_sexy",      EMOTE_ONESHOT_NO },
    { TEXT_EMOTE_SHAKE,        "emote_reply_sexy",      EMOTE_ONESHOT_NO },
    { TEXT_EMOTE_WHISTLE,      "emote_reply_sexy",      EMOTE_ONESHOT_NO },
    { TEXT_EMOTE_CUDDLE,       "emote_reply_sexy",      EMOTE_ONESHOT_NO },
    { TEXT_EMOTE_PURR,         "emote_reply_sexy",      EMOTE_ONESHOT_NO },
    { TEXT_EMOTE_SHIMMY,       "emote_reply_sexy",      EMOTE_ONESHOT_NO },
    { TEXT_EMOTE_SMIRK,        "emote_reply_sexy",      EMOTE_ONESHOT_NO },
    { TEXT_EMOTE_WINK,         "emote_reply_sexy",      EMOTE_ONESHOT_NO },
    { TEXT_EMOTE_NO,           "emote_reply_no",        EMOTE_ONESHOT_QUESTION },
    { TEXT_EMOTE_VETO,         "emote_reply_no",        EMOTE_ONESHOT_QUESTION },
    { TEXT_EMOTE_DISAGREE,     "emote_reply_no",        EMOTE_ONESHOT_QUESTION },
    { TEXT_EMOTE_DOUBT,        "emote_reply_no",        EMOTE_ONESHOT_QUESTION },
    { TEXT_EMOTE_PANIC,        "emote_reply_panic",     EMOTE_ONESHOT_EXCLAMATION },
    { TEXT_EMOTE_POINT,        "emote_reply_point",     EMOTE_ONESHOT_POINT },
    { TEXT_EMOTE_RUDE,         "emote_reply_rude",      EMOTE_ONESHOT_RUDE },
    { TEXT_EMOTE_RASP,         "emote_reply_rude",      EMOTE_ONESHOT_RUDE },
    { TEXT_EMOTE_ROAR,         "emote_reply_roar",      EMOTE_ONESHOT_ROAR },
    { TEXT_EMOTE_THREATEN,     "emote_reply_roar",      EMOTE_ONESHOT_ROAR },
    { TEXT_EMOTE_CALM,         "emote_reply_roar",      EMOTE_ONESHOT_ROAR },
    { TEXT_EMOTE_DUCK,         "emote_reply_roar",      EMOTE_ONESHOT_ROAR },
    { TEXT_EMOTE_TAUNT,        "emote_reply_roar",      EMOTE_ONESHOT_ROAR },
    { TEXT_EMOTE_PITY,         "emote_reply_roar",      EMOTE_ONESHOT_ROAR },
    { TEXT_EMOTE_GROWL,        "emote_reply_roar",      EMOTE_ONESHOT_ROAR },
    { TEXT_EMOTE_OPENFIRE,     "emote_reply_roar",      EMOTE_ONESHOT_ROAR },
    { TEXT_EMOTE_ENCOURAGE,    "emote_reply_roar",      EMOTE_ONESHOT_ROAR },
    { TEXT_EMOTE_ENEMY,        "emote_reply_roar",      EMOTE_ONESHOT_ROAR },
    { TEXT_EMOTE_TALK,         nullptr,                 EMOTE_ONESHOT_TALK },
    { TEXT_EMOTE_TALKEX,       nullptr,                 EMOTE_ONESHOT_YES },
    { TEXT_EMOTE_TALKQ,        "emote_reply_talk",      EMOTE_ONESHOT_TALK },
    { TEXT_EMOTE_LISTEN,       "emote_reply_talk",      EMOTE_ONESHOT_TALK },
    { TEXT_EMOTE_THANK,        "emote_reply_thank",     EMOTE_ONESHOT_BOW },
    { TEXT_EMOTE_VICTORY,      "emote_reply_cheer",     EMOTE_ONESHOT_CHEER },
    { TEXT_EMOTE_CHEER,        "emote_reply_cheer",     EMOTE_ONESHOT_CHEER },
    { TEXT_EMOTE_TOAST,        "emote_reply_cheer",     EMOTE_ONESHOT_CHEER },
    { TEXT_EMOTE_COLD,         "emote_reply_cold",      EMOTE_ONESHOT_QUESTION },
    { TEXT_EMOTE_SHIVER,       "emote_reply_cold",      EMOTE_ONESHOT_QUESTION },
    { TEXT_EMOTE_THIRSTY,      "emote_reply_cold",      EMOTE_ONESHOT_QUESTION },
    { TEXT_EMOTE_COMFORT,      "emote_reply_comfort",   EMOTE_ONESHOT_CRY },
    { TEXT_EMOTE_SOOTHE,       "emote_reply_comfort",   EMOTE_ONESHOT_CRY },
    { TEXT_EMOTE_PAT,          "emote_reply_comfort",   EMOTE_ONESHOT_CRY },
    { TEXT_EMOTE_INSULT,       "emote_reply_insult",    EMOTE_ONESHOT_CRY },
    { TEXT_EMOTE_JK,           "emote_reply_jk",        EMOTE_ONESHOT_POINT },
    { TEXT_EMOTE_RAISE,        "emote_reply_raise",     EMOTE_ONESHOT_POINT },
    { TEXT_EMOTE_READY,        "emote_reply_ready",     EMOTE_ONESHOT_SALUTE },
    { TEXT_EMOTE_SHOO,         "emote_reply_shoo",      EMOTE_ONESHOT_KICK },
    { TEXT_EMOTE_SLAP,         "emote_reply_slap",      EMOTE_ONESHOT_CRY },
    { TEXT_EMOTE_STAND,        "emote_reply_stand",     EMOTE_ONESHOT_NONE },
    { TEXT_EMOTE_TICKLE,       "emote_reply_tickle",    EMOTE_ONESHOT_LAUGH },
    { TEXT_EMOTE_VIOLIN,       "emote_reply_violin",    EMOTE_ONESHOT_TALK },
    { TEXT_EMOTE_GOODLUCK,     "emote_reply_goodluck",  EMOTE_ONESHOT_TALK },
    { TEXT_EMOTE_BRANDISH,     "emote_reply_brandish",  EMOTE_ONESHOT_BEG },
};

EmoteAnswer const* FindEmoteAnswer(uint32 text_emote)
{
    for (EmoteAnswer const& a : kEmoteAnswers)
        if (a.text_emote == text_emote)
            return &a;
    return nullptr;
}

} // namespace

void BotChatReactor::ReactEmote(Player* sender, uint32 emote_id, uint32 text_emote_id, ObjectGuid target)
{
    if (!sender) return;
    if (sender->GetSession() && sender->GetSession()->IsBot()) return;

    BotRegistry& reg = Services::Registry();
    const uint32 now_ms = GameTime::GetGameTimeMS();

    // Reciprocal emote map — answer a wave with a wave, a salute with a
    // salute, a bow with a bow, a cheer with a cheer, applaud with applaud.
    // Falls back to a friendly wave for anything not explicitly mapped so a
    // bot never apes a /rude or /spit. Emote ids are SharedDefines.h Emote
    // enum values, matching the constants BotCommandParser uses for /wave.
    auto reciprocal = [](uint32 in) -> uint32 {
        switch (in)
        {
            case 3:   return 3;    // EMOTE_ONESHOT_WAVE    -> wave back
            case 66:  return 66;   // EMOTE_ONESHOT_SALUTE  -> salute back
            case 2:   return 2;    // EMOTE_ONESHOT_BOW     -> bow back
            case 4:   return 4;    // EMOTE_ONESHOT_CHEER   -> cheer back
            case 21:  return 21;   // EMOTE_ONESHOT_APPLAUD -> applaud back
            case 24:  return 3;    // EMOTE_ONESHOT_SHY     -> friendly wave
            default:  return 3;    // safe default: wave (never apes /rude /spit)
        }
    };

    // Helper: emit a reciprocal emote from one bot toward the sender.
    auto emit_emote = [&](Player* bot) -> bool {
        if (!bot || !bot->IsAlive()) return false;
        if (bot == sender) return false;
        if (!bot->GetSession() || !bot->GetSession()->IsBot()) return false;
        const BotId mid = bot->GetGUID().GetCounter();
        if (!reg.has(mid)) return false;
        BotAI* ai = reg.ai(mid);
        if (!ai) return false;
        if (ai->personality().verbosity == Verbosity::Silent) return false;
        // Per-bot cooldown (shared namespace with chat replies).
        {
            std::lock_guard<std::mutex> lk(g_react_mtx);
            auto it = g_last_react_ms.find(mid);
            if (it != g_last_react_ms.end() &&
                now_ms - it->second < kAddressedReactCooldownMs)
                return false;
            // SC-P3b: bound map growth (see PruneStale).
            PruneStale(g_last_react_ms, now_ms, 2u * kPerBotReactCooldownMs,
                       kReactMapPruneThreshold);
            g_last_react_ms[mid] = now_ms;
        }
        Intent it{};
        it.bot_id = mid;
        it.body   = PerformEmoteIntent{reciprocal(emote_id), sender->GetGUID()};
        it.defer_until_ms = ComputeReplyDeferMs(now_ms, ai->personality(),
                                                /*in_len=*/8, /*reply_len=*/0,
                                                uint32(mid));
        Services::Intents(mid).push(std::move(it));
        return true;
    };

    // ike3 answer to an emote aimed at the bot: the emote_reply_* line (say or yell) and the matching
    // animation, through the same per-bot cooldown. False when ike3 had no answer for this emote or
    // the chatter is switched off - the caller then falls back to the reciprocal animation.
    auto emit_answer = [&](Player* bot) -> bool {
        EmoteAnswer const* answer = FindEmoteAnswer(text_emote_id);
        if (!answer || !BotTextMgr::instance().Enabled()) return false;
        if (!bot || !bot->IsAlive() || bot == sender) return true;
        if (!bot->GetSession() || !bot->GetSession()->IsBot()) return true;
        const BotId mid = bot->GetGUID().GetCounter();
        if (!reg.has(mid)) return true;
        BotAI* ai = reg.ai(mid);
        if (!ai || ai->personality().verbosity == Verbosity::Silent) return true;
        {
            std::lock_guard<std::mutex> lk(g_react_mtx);
            auto it = g_last_react_ms.find(mid);
            if (it != g_last_react_ms.end() &&
                now_ms - it->second < kAddressedReactCooldownMs)
                return true;
            PruneStale(g_last_react_ms, now_ms, 2u * kPerBotReactCooldownMs,
                       kReactMapPruneThreshold);
            g_last_react_ms[mid] = now_ms;
        }
        BotTextMgr::Line line;
        if (answer->key)
            line = BotTextMgr::instance().Pick(answer->key, { { "%s", sender->GetName() } });
        const uint32 defer = ComputeReplyDeferMs(now_ms, ai->personality(), /*in_len=*/8,
                                                 line.text.size(), uint32(mid));
        if (answer->anim != EMOTE_ONESHOT_NONE)
        {
            Intent anim{};
            anim.bot_id = mid;
            anim.body   = PerformEmoteIntent{answer->anim, sender->GetGUID()};
            anim.defer_until_ms = defer;
            Services::Intents(mid).push(std::move(anim));
        }
        if (!line.text.empty())
        {
            Intent say{};
            say.bot_id = mid;
            if (line.yell)
                say.body = ChatIntent{YellChatIntent{std::move(line.text)}};
            else
                say.body = ChatIntent{SayChatIntent{std::move(line.text)}};
            say.defer_until_ms = defer;
            Services::Intents(mid).push(std::move(say));
        }
        return true;
    };

    // Targeted emote → only the targeted bot answers: ike3's line + animation where ike3 had one,
    // otherwise the reciprocal animation.
    if (!target.IsEmpty() && target.IsPlayer())
    {
        if (Player* tgt = ObjectAccessor::FindConnectedPlayer(target))
            if (!emit_answer(tgt))
                emit_emote(tgt);
        return;
    }

    // Untargeted emote → pick one nearby bot (range = text-emote listen
    // range) to wave/cheer back. Grid-bounded query, single replier by hash.
    const float range = sWorld->getFloatConfig(CONFIG_LISTEN_RANGE_TEXTEMOTE);
    std::list<Player*> nearby;
    Position const senderPos = sender->GetPosition();
    Trinity::AnyPlayerInPositionRangeCheck check(&senderPos, range, /*reqAlive=*/true);
    Trinity::PlayerListSearcher<Trinity::AnyPlayerInPositionRangeCheck>
        searcher(sender, nearby, check);
    Cell::VisitWorldObjects(sender, searcher, range);

    std::vector<Player*> bots;
    bots.reserve(8);
    for (Player* member : nearby)
    {
        if (!member || member == sender) continue;
        if (!member->GetSession() || !member->GetSession()->IsBot()) continue;
        if (!member->IsAlive()) continue;
        if (!reg.has(member->GetGUID().GetCounter())) continue;
        bots.push_back(member);
    }
    if (bots.empty()) return;
    const uint32 sender_low = sender->GetGUID().GetCounter();
    const size_t idx = PickReplier(sender_low, char(emote_id & 0xFF), bots.size());
    emit_emote(bots[idx]);
}

// SC-P2b: welcome a freshly-joined guild member. One online bot guildmate
// (deterministic by guid hash) drops a welcome line a few seconds later.
// Per-guild throttle prevents a recruiting spree from spamming chat.
void BotChatReactor::ReactGuildJoin(uint64 guild_id, ObjectGuid joiner_guid,
                                    std::string const& joiner_name)
{
    if (guild_id == 0) return;
    Guild* g = sGuildMgr->GetGuildById(guild_id);
    if (!g) return;

    // Don't welcome auto-spawned bot recruits — only real-player joins (a bot
    // welcoming a bot is pure noise). The joining player may be offline at add
    // time; if online and flagged as a bot, skip.
    if (Player* jp = ObjectAccessor::FindConnectedPlayer(joiner_guid))
        if (jp->GetSession() && jp->GetSession()->IsBot())
            return;

    static std::mutex                           s_join_mtx;
    static std::unordered_map<uint64, uint32_t> s_last_join_ms;
    constexpr uint32_t kPerGuildJoinCooldownMs = 30u * 1000u;
    const uint32 now_ms = GameTime::GetGameTimeMS();
    {
        std::lock_guard<std::mutex> lk(s_join_mtx);
        auto it = s_last_join_ms.find(guild_id);
        if (it != s_last_join_ms.end() &&
            now_ms - it->second < kPerGuildJoinCooldownMs)
            return;
    }

    // Collect online bot guildmates (excluding the joiner). Pick one stably.
    BotRegistry& reg = Services::Registry();
    struct WCand { BotId id; uint8 rank; };
    std::vector<WCand> cands;
    cands.reserve(8);
    for (auto const& [mguid, member] : g->GetMembers())
    {
        if (!member.IsOnline()) continue;
        if (mguid == joiner_guid) continue;
        Player* mp = ObjectAccessor::FindConnectedPlayer(mguid);
        if (!mp || !mp->GetSession() || !mp->GetSession()->IsBot()) continue;
        if (!mp->IsAlive()) continue;
        const BotId mid = mguid.GetCounter();
        if (!reg.has(mid)) continue;
        BotAI* ai = reg.ai(mid);
        if (!ai || ai->personality().verbosity == Verbosity::Silent) continue;
        cands.push_back({mid, static_cast<uint8>(member.GetRankId())});
    }
    if (cands.empty()) return;

    // Prefer the highest rank (lowest rank id) as the "greeter", tie-broken by
    // a stable hash so the same officer tends to welcome people.
    std::sort(cands.begin(), cands.end(),
        [](WCand const& a, WCand const& b) { return a.rank < b.rank; });
    const uint32 seed = uint32(guild_id) ^ uint32(joiner_guid.GetCounter());
    // Pick among the top-rank tier for variety.
    uint8 top_rank = cands.front().rank;
    size_t top_n = 0;
    while (top_n < cands.size() && cands[top_n].rank == top_rank) ++top_n;
    const BotId greeter = cands[seed % top_n].id;

    // Compose a welcome line. Address the joiner by first name where possible.
    std::string first = joiner_name;
    if (auto sp = first.find(' '); sp != std::string::npos) first.resize(sp);
    static constexpr char const* kTemplates[] = {
        "welcome {}!", "welcome to the guild {} :)", "hey {}, welcome!",
        "welcome aboard {}!", "{} welcome! :)", "glad to have you {}!",
    };
    constexpr size_t kN = sizeof(kTemplates) / sizeof(*kTemplates);
    std::string line = kTemplates[seed % kN];
    if (auto pos = line.find("{}"); pos != std::string::npos)
    {
        if (!first.empty()) line.replace(pos, 2, first);
        else                line.replace(pos, 2, "friend");
    }

    // Stamp throttle only now that we will emit.
    {
        std::lock_guard<std::mutex> lk(s_join_mtx);
        // SC-P3b: bound per-guild map growth (see PruneStale).
        PruneStale(s_last_join_ms, now_ms, 2u * kPerGuildJoinCooldownMs,
                   kReactMapPruneThreshold);
        s_last_join_ms[guild_id] = now_ms;
    }

    Intent it{};
    it.bot_id = greeter;
    it.body   = ChatIntent{GuildChatIntent{std::move(line)}};
    // A few-second delay so the welcome lands AFTER the system join line and
    // reads like a person noticing the join notification, not a trigger.
    BotPersonality const& gp = reg.ai(greeter) ? reg.ai(greeter)->personality()
                                               : DefaultPersonality();
    const uint32 base = 2500u + (seed % 2500u);   // 2.5–5s noticing delay
    it.defer_until_ms = now_ms + base + gp.response_delay_ms;
    Services::Intents(greeter).push(std::move(it));
}

} // namespace Playerbot
