// BotTextMgr / BotChatter - see header.

#include "BotTextMgr.h"

#include "BotAI.h"
#include "BotIntent.h"
#include "BotIntentEmitter.h"
#include "BotPersonality.h"
#include "BotRegistry.h"
#include "BotSnapshotView.h"
#include "../Services.h"
#include "../Threading/IntentQueue.h"
#include "../Util/ConfigReader.h"

#include "Config.h"
#include "Creature.h"
#include "CreatureData.h"
#include "DB2Stores.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "Group.h"
#include "GroupReference.h"
#include "ItemTemplate.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "QuestDef.h"
#include "Random.h"
#include "StringFormat.h"
#include "World.h"
#include "WorldSession.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <mutex>
#include <sstream>

namespace Playerbot {

// ---------------------------------------------------------------------------
// BotTextMgr
// ---------------------------------------------------------------------------

BotTextMgr& BotTextMgr::instance()
{
    static BotTextMgr mgr;
    return mgr;
}

void BotTextMgr::Load()
{
    enabled_           = sConfigMgr->GetBoolDefault("Playerbot.Chatter.Enabled", true, true);
    chance_scale_      = std::max(0.0f, sConfigMgr->GetFloatDefault("Playerbot.Chatter.ChanceScale", 1.0f, true));
    group_cooldown_ms_ = uint32(std::max(0, sConfigMgr->GetIntDefault("Playerbot.Chatter.GroupCooldownMs", 20000, true)));
    bot_cooldown_ms_   = uint32(std::max(0, sConfigMgr->GetIntDefault("Playerbot.Chatter.BotCooldownMs", 60000, true)));

    auto ms = [](char const* key, int32 def) { return uint32(std::max(0, sConfigMgr->GetIntDefault(key, def, true))); };
    speech_.callouts_enabled            = sConfigMgr->GetBoolDefault("Playerbot.Chatter.Callouts.Enabled", true, true);
    speech_.callout_key_cooldown_ms     = ms("Playerbot.Chatter.Callouts.KeyCooldownMs", 60000);
    speech_.callout_group_cooldown_ms   = ms("Playerbot.Chatter.Callouts.GroupCooldownMs", 8000);
    speech_.suggest_enabled             = sConfigMgr->GetBoolDefault("Playerbot.Chatter.Suggest.Enabled", true, true);
    speech_.suggest_bot_cooldown_ms     = std::max<uint32>(60000, ms("Playerbot.Chatter.Suggest.BotCooldownMs", 1800000));
    speech_.suggest_channel_cooldown_ms = ms("Playerbot.Chatter.Suggest.ChannelCooldownMs", 180000);
    speech_.suggest_global_cooldown_ms  = ms("Playerbot.Chatter.Suggest.GlobalCooldownMs", 10000);
    speech_.global_max_per_minute       = ms("Playerbot.Chatter.GlobalMaxPerMinute", 60);

    texts_.clear();
    chance_.clear();
    count_ = 0;

    // Realm-independent content: lives in the shared playerbot schema (migration 0019), reached over
    // the character connection by qualifying with the configured schema name, as BotNamePool does.
    std::string const shared = sConfigMgr->GetStringDefault("Playerbot.SharedDatabase", "playerbot", true);

    if (QueryResult res = CharacterDatabase.Query(Trinity::StringFormat(
            "SELECT name, text, say_type, reply_type, weight FROM `{}`.playerbot_v2_bot_text ORDER BY id", shared).c_str()))
    {
        do
        {
            Field* f = res->Fetch();
            Row row;
            row.text       = f[1].GetString();
            row.say_type   = f[2].GetUInt8();
            row.reply_type = f[3].GetUInt8();
            row.weight     = f[4].GetUInt16();
            if (row.weight == 0)
                continue;   // a line switched off by its weight
            texts_[f[0].GetString()].push_back(std::move(row));
            ++count_;
        } while (res->NextRow());
    }

    if (QueryResult res = CharacterDatabase.Query(Trinity::StringFormat(
            "SELECT name, chance FROM `{}`.playerbot_v2_bot_text_chance", shared).c_str()))
    {
        do
        {
            Field* f = res->Fetch();
            chance_[f[0].GetString()] = f[1].GetFloat();
        } while (res->NextRow());
    }

    TC_LOG_INFO("server.loading", "[PlayerbotV2] Bot texts: {} line(s) under {} key(s), {} chance row(s); chatter {} (scale {:.2f}).",
        count_, texts_.size(), chance_.size(), enabled_ ? "on" : "off", chance_scale_);
}

bool BotTextMgr::HasKey(std::string_view key) const
{
    auto it = texts_.find(std::string(key));
    return it != texts_.end() && !it->second.empty();
}

void BotTextMgr::Fill(std::string& text, TextPlaceholders const& ph)
{
    if (ph.empty() || text.find('%') == std::string::npos)
        return;
    // Longest placeholder first, so %quest_obj_available is never eaten by a shorter key that
    // happens to be its prefix.
    std::vector<std::pair<std::string, std::string> const*> order;
    order.reserve(ph.size());
    for (auto const& p : ph)
        if (!p.first.empty())
            order.push_back(&p);
    std::sort(order.begin(), order.end(), [](auto const* a, auto const* b) { return a->first.size() > b->first.size(); });
    for (auto const* p : order)
    {
        for (size_t pos = text.find(p->first); pos != std::string::npos; pos = text.find(p->first, pos + p->second.size()))
            text.replace(pos, p->first.size(), p->second);
    }
}

template <typename Accept>
BotTextMgr::Row const* BotTextMgr::PickWeighted(std::vector<Row> const& rows, Accept&& accept)
{
    uint64 total = 0;
    for (Row const& r : rows)
        if (accept(r))
            total += r.weight;
    if (total == 0)
        return nullptr;
    uint64 roll = urand(0, uint32(std::min<uint64>(total, 0xFFFFFFFFu) - 1));
    for (Row const& r : rows)
    {
        if (!accept(r))
            continue;
        if (roll < r.weight)
            return &r;
        roll -= r.weight;
    }
    return nullptr;
}

BotTextMgr::Line BotTextMgr::Pick(std::string_view key, TextPlaceholders const& ph) const
{
    Line out;
    auto it = texts_.find(std::string(key));
    if (it == texts_.end())
        return out;
    Row const* row = PickWeighted(it->second, [](Row const&) { return true; });
    if (!row)
        return out;
    out.text = row->text;
    out.yell = row->say_type == 1;
    Fill(out.text, ph);
    return out;
}

std::string BotTextMgr::GetReply(ReplyType type, std::string const& other_name) const
{
    auto it = texts_.find("reply");
    if (it == texts_.end())
        return {};
    Row const* row = PickWeighted(it->second, [type](Row const& r) { return r.reply_type == uint8(type); });
    if (!row)
        return {};
    std::string text = row->text;
    Fill(text, { { "%s", other_name } });
    return text;
}

bool BotTextMgr::RollChance(std::string_view key) const
{
    auto it = chance_.find(std::string(key));
    if (it == chance_.end())
        return true;
    float const pct = it->second * chance_scale_;
    if (pct >= 100.0f)
        return true;
    if (pct <= 0.0f)
        return false;
    return frand(0.0f, 100.0f) < pct;
}

// ---------------------------------------------------------------------------
// BotChatter
// ---------------------------------------------------------------------------

namespace BotChatter {

namespace {

std::string Lower(std::string s)
{
    for (char& c : s)
        c = char(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string AreaName(uint32 area_id)
{
    if (AreaTableEntry const* ae = sAreaTableStore.LookupEntry(area_id))
    {
        char const* n = ae->AreaName[sWorld->GetDefaultDbcLocale()];
        if (n && *n)
            return n;
    }
    return BotTextMgr::instance().Get("string_unknown_area");
}

std::string ItemLink(ItemTemplate const* proto, uint8 level)
{
    uint32 const quality = std::min<uint32>(proto->GetQuality(), MAX_ITEM_QUALITY - 1);
    // Same hyperlink shape TrinityCore's own `.list mail` uses.
    return Trinity::StringFormat("|c{:X}|Hitem:{}:0:0:0:0:0:0:0:{}:0:0:0:0:0|h[{}]|h|r",
        ItemQualityColors[quality], proto->GetId(), uint32(level), proto->GetName(sWorld->GetDefaultDbcLocale()));
}

std::string QuestLink(Player const* bot, Quest const* quest)
{
    return Trinity::StringFormat("|cffffff00|Hquest:{}:{}:{}:0:0|h[{}]|h|r",
        quest->GetQuestId(), bot->GetQuestLevel(quest), bot->GetQuestMinLevel(quest), quest->GetLogTitle());
}

// The bot's party has a real player in it (owner or any human) - the audience the broadcasts are for.
// World bots grouped only with other bots, and solo bots, stay quiet.
bool GroupHasRealPlayer(Player const* bot)
{
    Group const* group = bot->GetGroup();
    if (!group)
        return false;
    for (GroupReference const& ref : group->GetMembers())
    {
        Player const* m = ref.GetSource();
        if (m && m != bot && m->GetSession() && !m->GetSession()->IsBot())
            return true;
    }
    return false;
}

BotAI* ChattyAi(Player const* bot, Verbosity min_verbosity)
{
    if (!bot || !bot->IsAlive() || !BotTextMgr::instance().Enabled())
        return nullptr;
    // Big raids: forty bots talking at once is spam (ike3's quiet-raid rule).
    if (Group const* g = bot->GetGroup(); g && g->GetMembersCount() > 15)
        return nullptr;
    BotId const id = bot->GetGUID().GetCounter();
    BotRegistry& reg = Services::Registry();
    if (!reg.has(id))
        return nullptr;
    BotAI* ai = reg.ai(id);
    if (!ai || ai->personality().verbosity < min_verbosity)
        return nullptr;
    return ai;
}

void PushChat(BotId id, ChatIntent&& chat, uint32 defer_ms)
{
    Intent it{};
    it.bot_id = id;
    it.body = std::move(chat);
    it.defer_until_ms = defer_ms ? GameTime::GetGameTimeMS() + defer_ms : 0;
    Services::Intents(id).push(std::move(it));
}

// One unsolicited party line from a companion, through every gate: chatter on, a real player in the
// group, the bot not silenced by personality, the key's chance, then the per-group/per-bot limiter.
void BroadcastToParty(Player* bot, std::string_view key, TextPlaceholders& ph)
{
    BotTextMgr const& mgr = BotTextMgr::instance();
    if (!ChattyAi(bot, Verbosity::Normal) || !GroupHasRealPlayer(bot) || !mgr.HasKey(key))
        return;
    if (!mgr.RollChance(key))
        return;
    BotId const id = bot->GetGUID().GetCounter();
    if (!TakeSlot(Slot::Broadcast, bot->GetGroup()->GetGUID().GetCounter(), id, GameTime::GetGameTimeMS()))
        return;
    AddSelfPlaceholders(ph, bot->GetClass(), bot->GetRace(), bot->GetLevel(), bot->GetZoneId(), bot->GetAreaId());
    std::string text = mgr.Get(key, ph);
    if (text.empty())
        return;
    // A beat later, like a person glancing at the loot window or quest log before typing.
    PushChat(id, ChatIntent{ PartyChatIntent{ std::move(text) } }, urand(1500, 4000));
}

// Last quest status seen per (bot, quest), so an INCOMPLETE that follows nothing reads as "accepted"
// while one that follows COMPLETE (a quest item was lost) does not. Entries leave on turn-in/abandon.
std::mutex                             g_quest_mtx;
std::unordered_map<uint64, uint8>      g_quest_status;

std::mutex                             g_slot_mtx;
std::unordered_map<uint64, uint32>     g_group_greet_ms;
std::unordered_map<uint64, uint32>     g_group_broadcast_ms;
std::unordered_map<BotId, uint32>      g_bot_broadcast_ms;

constexpr uint32 kGroupGreetWindowMs = 6000;   // one hello per group per 6s
constexpr size_t kLimiterPruneAt     = 4096;

template <typename Map>
void Prune(Map& m, uint32 now_ms, uint32 keep_ms)
{
    if (m.size() < kLimiterPruneAt)
        return;
    for (auto it = m.begin(); it != m.end();)
        it = (now_ms - it->second > keep_ms) ? m.erase(it) : std::next(it);
}

} // namespace

void AddSelfPlaceholders(TextPlaceholders& ph, uint8 cls, uint8 race, uint8 level, uint32 zone_id, uint32 area_id)
{
    LocaleConstant const loc = sWorld->GetDefaultDbcLocale();
    std::string cls_name, race_name;
    if (ChrClassesEntry const* c = sChrClassesStore.LookupEntry(cls))
        cls_name = c->Name[loc];
    if (ChrRacesEntry const* r = sChrRacesStore.LookupEntry(race))
        race_name = r->Name[loc];
    ph.emplace_back("%my_class", cls_name);
    ph.emplace_back("%my_race", race_name);
    ph.emplace_back("%my_level", std::to_string(level));
    ph.emplace_back("%zone_name", AreaName(zone_id));
    ph.emplace_back("%area_name", AreaName(area_id));
}

bool TakeSlot(Slot slot, uint64 group_low, BotId bot, uint32 now_ms)
{
    BotTextMgr const& mgr = BotTextMgr::instance();
    std::lock_guard lk(g_slot_mtx);
    if (slot == Slot::Greet || slot == Slot::LevelUp)
    {
        uint64 const key = (uint64(slot) << 60) ^ group_low;
        auto it = g_group_greet_ms.find(key);
        if (it != g_group_greet_ms.end() && now_ms - it->second < kGroupGreetWindowMs)
            return false;
        Prune(g_group_greet_ms, now_ms, kGroupGreetWindowMs);
        g_group_greet_ms[key] = now_ms;
        return true;
    }
    auto git = g_group_broadcast_ms.find(group_low);
    if (git != g_group_broadcast_ms.end() && now_ms - git->second < mgr.GroupCooldownMs())
        return false;
    auto bit = g_bot_broadcast_ms.find(bot);
    if (bit != g_bot_broadcast_ms.end() && now_ms - bit->second < mgr.BotCooldownMs())
        return false;
    Prune(g_group_broadcast_ms, now_ms, mgr.GroupCooldownMs());
    Prune(g_bot_broadcast_ms, now_ms, mgr.BotCooldownMs());
    g_group_broadcast_ms[group_low] = now_ms;
    g_bot_broadcast_ms[bot] = now_ms;
    return true;
}

std::string GreetLine(bool follow_owner, uint64 group_low, BotId bot, uint32 now_ms)
{
    BotTextMgr const& mgr = BotTextMgr::instance();
    char const* key = (follow_owner && mgr.HasKey("hello_follow")) ? "hello_follow" : "hello";
    if (!mgr.Enabled() || !mgr.HasKey(key) || !mgr.RollChance(key))
        return {};
    if (!TakeSlot(Slot::Greet, group_low, bot, now_ms))
        return {};
    return mgr.Get(key);
}

std::string LevelUpLine(uint8 level, uint8 cls, uint8 race, uint32 zone_id, uint32 area_id,
                        uint64 group_low, BotId bot, uint32 now_ms)
{
    BotTextMgr const& mgr = BotTextMgr::instance();
    if (!mgr.Enabled())
        return {};
    // ike3 BroadcastHelper::BroadcastLevelup: the level cap and every tenth level get their own lines.
    char const* key = "broadcast_levelup_generic";
    if (level >= uint8(sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL)))
        key = "broadcast_levelup_max_level";
    else if (level % 10 == 0)
        key = "broadcast_levelup_10x";
    if (!mgr.HasKey(key) || !mgr.RollChance(key))
        return {};
    if (!TakeSlot(Slot::LevelUp, group_low, bot, now_ms))
        return {};
    TextPlaceholders ph;
    AddSelfPlaceholders(ph, cls, race, level, zone_id, area_id);
    return mgr.Get(key, ph);
}

std::string GenerateReply(std::string const& bot_name, std::string const& msg, std::string const& sender_name)
{
    BotTextMgr const& mgr = BotTextMgr::instance();
    if (!mgr.Enabled())
        return {};

    std::vector<std::string> words;
    {
        std::istringstream ss(Lower(msg));
        std::string w;
        while (ss >> w)
        {
            while (!w.empty() && std::ispunct(static_cast<unsigned char>(w.back())))
                w.pop_back();
            words.push_back(w);
        }
    }
    if (words.empty())
        return {};

    // ike3: a greeting among the first eight words wins outright.
    for (size_t i = 0; i < words.size() && i < 8; ++i)
        if (words[i] == "hi" || words[i] == "hey" || words[i] == "hello" || words[i] == "wazzup")
            return mgr.GetReply(ReplyType::Hello, sender_name);

    // Question answers, keyed by the opening word as ike3 did.
    char const* question_key = nullptr;
    std::string const& w0 = words[0];
    if (w0.find("what") != std::string::npos)      question_key = "reply_question_what";
    else if (w0.find("who") != std::string::npos)  question_key = "reply_question_who";
    else if (w0 == "when")                         question_key = "reply_question_when";
    else if (w0 == "where")                        question_key = "reply_question_where";
    else if (w0 == "why")                          question_key = "reply_question_why";
    else if (w0 == "am" || w0 == "are" || w0 == "is") question_key = "reply_question_is";
    else if (msg.find('?') != std::string::npos)   question_key = "reply_question_generic";

    std::string const name_lower = Lower(bot_name);
    bool const named = !name_lower.empty() && Lower(msg).find(name_lower) != std::string::npos;

    // ike3: a question is answered as a question, except that four times in five a line naming the
    // bot gets the "that's my name" reply instead.
    if (question_key && (!named || urand(0, 4) == 0))
    {
        std::string text = mgr.Get(question_key, { { "%s", sender_name } });
        if (!text.empty())
            return text;
    }
    return mgr.GetReply(named ? ReplyType::Name : ReplyType::NotUnderstand, sender_name);
}

void OnItemLooted(Player* bot, uint32 item_id)
{
    ItemTemplate const* proto = sObjectMgr->GetItemTemplate(item_id);
    if (!proto)
        return;
    char const* key = nullptr;
    switch (proto->GetQuality())
    {
        case ITEM_QUALITY_POOR:      key = "broadcast_looting_item_poor";      break;
        case ITEM_QUALITY_NORMAL:    key = "broadcast_looting_item_normal";    break;
        case ITEM_QUALITY_UNCOMMON:  key = "broadcast_looting_item_uncommon";  break;
        case ITEM_QUALITY_RARE:      key = "broadcast_looting_item_rare";      break;
        case ITEM_QUALITY_EPIC:      key = "broadcast_looting_item_epic";      break;
        case ITEM_QUALITY_LEGENDARY: key = "broadcast_looting_item_legendary"; break;
        case ITEM_QUALITY_ARTIFACT:  key = "broadcast_looting_item_artifact";  break;
        default: return;
    }
    TextPlaceholders ph{ { "%item_link", ItemLink(proto, bot->GetLevel()) } };
    BroadcastToParty(bot, key, ph);
}

void OnQuestStatusChange(Player* bot, uint32 quest_id)
{
    if (!bot || !bot->GetSession() || !bot->GetSession()->IsBot())
        return;
    if (!Services::Registry().has(bot->GetGUID().GetCounter()))
        return;
    QuestStatus const status = bot->GetQuestStatus(quest_id);
    uint64 const key = (uint64(bot->GetGUID().GetCounter()) << 32) | quest_id;

    uint8 previous = QUEST_STATUS_NONE;
    {
        std::lock_guard lk(g_quest_mtx);
        auto it = g_quest_status.find(key);
        if (it != g_quest_status.end())
            previous = it->second;
        if (status == QUEST_STATUS_NONE || status == QUEST_STATUS_REWARDED)
        {
            if (it != g_quest_status.end())
                g_quest_status.erase(it);
        }
        else
        {
            // Bounded by the bots' quest logs; the clear is a backstop against abandoned entries
            // whose removal never reported a status change.
            if (g_quest_status.size() > 200000)
                g_quest_status.clear();
            g_quest_status[key] = uint8(status);
        }
    }
    if (previous == uint8(status))
        return;

    char const* text_key = nullptr;
    switch (status)
    {
        case QUEST_STATUS_INCOMPLETE:
            if (previous == QUEST_STATUS_NONE)
                text_key = "broadcast_quest_accepted_generic";
            break;
        case QUEST_STATUS_COMPLETE:  text_key = "broadcast_quest_update_complete";     break;
        case QUEST_STATUS_FAILED:    text_key = "broadcast_quest_update_failed_timer"; break;
        case QUEST_STATUS_REWARDED:  text_key = "broadcast_quest_turned_in";           break;
        default: break;
    }
    if (!text_key)
        return;
    Quest const* quest = sObjectMgr->GetQuestTemplate(quest_id);
    if (!quest)
        return;
    TextPlaceholders ph{ { "%quest_link", QuestLink(bot, quest) } };
    BroadcastToParty(bot, text_key, ph);
}

void OnCreatureKilled(Player* bot, Creature* killed)
{
    if (!bot || !killed || !bot->GetSession() || !bot->GetSession()->IsBot())
        return;
    char const* key = "broadcast_killed_normal";
    if (killed->IsPet())
        key = "broadcast_killed_pet";
    else if (killed->isWorldBoss())
        key = "broadcast_killed_worldboss";
    else
    {
        switch (killed->GetCreatureTemplate()->Classification)
        {
            case CreatureClassifications::Elite:     key = "broadcast_killed_elite";     break;
            case CreatureClassifications::RareElite: key = "broadcast_killed_rareelite"; break;
            case CreatureClassifications::Rare:      key = "broadcast_killed_rare";      break;
            case CreatureClassifications::Normal:                                         break;
            // ike3's default case: a rank it did not know (critters, trivial mobs).
            default:                                 key = "broadcast_killed_unknown";   break;
        }
    }
    TextPlaceholders ph{
        { "%victim_name", killed->GetName() },
        { "%victim_level", std::to_string(killed->GetLevel()) },
    };
    BroadcastToParty(bot, key, ph);
}

void OnPlayerKilled(Player* bot, Player* killed)
{
    if (!bot || !killed || bot == killed || !bot->GetSession() || !bot->GetSession()->IsBot())
        return;
    std::string victim_class;
    if (ChrClassesEntry const* c = sChrClassesStore.LookupEntry(killed->GetClass()))
        victim_class = c->Name[sWorld->GetDefaultDbcLocale()];
    TextPlaceholders ph{
        { "%victim_name", killed->GetName() },
        { "%victim_level", std::to_string(killed->GetLevel()) },
        { "%victim_class", victim_class },
    };
    BroadcastToParty(bot, "broadcast_killed_player", ph);
}

void SayGoodbye(Player* bot, Player* owner)
{
    if (!owner || !ChattyAi(bot, Verbosity::Terse))
        return;
    std::string text = BotTextMgr::instance().Get("goodbye");
    if (text.empty())
        return;
    PushChat(bot->GetGUID().GetCounter(), ChatIntent{ WhisperIntent{ owner->GetName(), std::move(text) } }, 0);
}

// ---------------------------------------------------------------------------
// SpeechTick - ike3 SayStrategy callouts and SuggestWhatToDoAction channel lines
// ---------------------------------------------------------------------------

namespace {

// ---- limiter state (bot workers tick bots concurrently: everything below is behind a lock) ----

// One companion's callout memory: the band each watched value was last seen in (so a line is said
// when the value CROSSES into a worse band, not on every tick it stays there), whether the fight's
// one-off lines were said, and when each kind of line was last said.
enum CalloutKind : uint8 { CK_CriticalHealth, CK_LowHealth, CK_LowMana, CK_NoAmmo, CK_LowAmmo, CK_Taunt, CK_Aoe, CK_Count };

struct CalloutState
{
    bool   primed = false;
    uint8  hp_band = 0;        // 0 fine, 1 low (< 50%), 2 critical (< 20%)
    bool   mana_low = false;   // < 15%
    uint8  ammo_band = 0;      // 0 fine, 1 low (< 100), 2 none
    bool   taunt_said = false; // once per fight
    bool   aoe_said = false;   // once per fight
    std::array<uint32, CK_Count> last_ms{};
};

constexpr size_t kSpeechShards = 16;
struct SpeechShard
{
    std::mutex                               mtx;
    std::unordered_map<BotId, CalloutState>  callouts;
    std::unordered_map<BotId, uint32>        suggest_ms;   // a world bot's last (or primed) suggestion time
};
std::array<SpeechShard, kSpeechShards> g_speech;
SpeechShard& ShardOf(BotId id) { return g_speech[id % kSpeechShards]; }

std::mutex                         g_speech_mtx;
std::unordered_map<uint64, uint32> g_callout_group_ms;    // group -> last callout
std::unordered_map<uint64, uint32> g_channel_ms;          // (team, channel, zone) -> last suggestion
uint32 g_minute_start_ms = 0;
uint32 g_minute_count    = 0;
// Realm-wide earliest time for the next channel line. Read without the lock as the cheap first
// gate, so thousands of idle world bots do not take a mutex every tick to learn "not yet".
std::atomic<uint32> g_suggest_next_ms{ 0 };

bool Elapsed(uint32 now_ms, uint32 since_ms, uint32 gap_ms) { return now_ms - since_ms >= gap_ms; }

// 0 = no channel line yet. Compared as a signed difference so the game-time clock's wrap is harmless.
bool SuggestGateOpen(uint32 now_ms)
{
    uint32 const next = g_suggest_next_ms.load(std::memory_order_relaxed);
    return next == 0 || int32(now_ms - next) >= 0;
}

// Realm-wide cap on the lines this file adds. Caller holds g_speech_mtx.
bool TakeGlobalLocked(uint32 now_ms)
{
    uint32 const cap = BotTextMgr::instance().Speech().global_max_per_minute;
    if (cap == 0)
        return true;
    if (!Elapsed(now_ms, g_minute_start_ms, 60000))
    {
        if (g_minute_count >= cap)
            return false;
    }
    else
    {
        g_minute_start_ms = now_ms;
        g_minute_count = 0;
    }
    ++g_minute_count;
    return true;
}

std::string CreatureName(NearbyUnit const& u)
{
    if (u.is_player)
        return {};
    if (CreatureTemplate const* ct = sObjectMgr->GetCreatureTemplate(u.entry))
        return ct->Name;
    return {};
}

// %my_role: the bot's spec and class ("Protection Warrior"), ike3's formatClass(bot, spec).
std::string RoleName(BotSnapshotView const& s)
{
    LocaleConstant const loc = sWorld->GetDefaultDbcLocale();
    std::string cls_name;
    if (ChrClassesEntry const* c = sChrClassesStore.LookupEntry(s.cls()))
        cls_name = c->Name[loc];
    if (ChrSpecializationEntry const* sp = sChrSpecializationStore.LookupEntry(s.spec()))
    {
        char const* n = sp->Name[loc];
        if (n && *n && sp->ClassID == s.cls())
            return Trinity::StringFormat("{} {}", n, cls_name);
    }
    return cls_name;
}

void EmitLine(BotIntentEmitter& em, std::string text, bool party)
{
    // ike3 SayAction: a line written as "/y ..." is yelled.
    if (text.rfind("/y ", 0) == 0)
        em.yell_world(text.substr(3));
    else if (party)
        em.say(std::move(text));
    else
        em.say_world(std::move(text));
}

// ---- companions: ike3 SayStrategy (combat) + the hunter's "no ammo" ----

void Callouts(BotAI& ai, BotSnapshotView const& s, BotIntentEmitter& em, uint32 now_ms)
{
    BotTextMgr const& mgr = BotTextMgr::instance();
    BotTextMgr::SpeechConfig const& cfg = mgr.Speech();
    // The audience is the owner: only a companion whose owner is in its group calls out.
    if (!cfg.callouts_enabled || !s.is_companion_with_owner() || !s.in_group())
        return;
    Verbosity const verbosity = ai.personality().verbosity;
    if (verbosity < Verbosity::Terse)
        return;

    bool const alive     = s.is_alive() && !s.is_ghost();
    bool const in_combat = alive && s.in_combat();
    int32 const hp       = s.hp_pct();
    bool const has_mana  = s.max_power(POWER_MANA) > 0;
    int32 const mana     = s.power_pct(POWER_MANA);
    bool const hunter    = s.cls() == CLASS_HUNTER && s.ammo_kind() != 0;
    uint32 const ammo    = s.ammo_count();
    bool const tank      = ai.effective_role(s) == Role::Tank;

    // What the snapshot says, before deciding what changed.
    uint8 hp_band_now = 0;
    if (alive && hp < 20)      hp_band_now = 2;
    else if (alive && hp < 50) hp_band_now = 1;

    NearbyUnit const* taunt_target = nullptr;
    if (in_combat && tank)
        if (NearbyUnit const* u = s.untaunted_enemy(30.f); u && !u->is_player && !u->untargetable && !u->is_pacified)
            taunt_target = u;

    bool big_pull = false;
    if (in_combat && !tank)
    {
        size_t engaged = 0;
        float bx, by, bz;
        s.position(bx, by, bz);
        for (NearbyUnit const& e : s.nearby_enemies())
        {
            if (e.victim.IsEmpty() || e.untargetable || e.is_pacified || e.hp <= 0)
                continue;
            float const dx = e.x - bx, dy = e.y - by, dz = e.z - bz;
            if (dx * dx + dy * dy + dz * dz <= 40.f * 40.f)
                ++engaged;
        }
        big_pull = s.fightable_attackers_count() >= 3 || engaged >= 4;   // ike3 "ranged medium aoe"
    }

    CalloutKind kind = CK_Count;
    char const* key = nullptr;
    {
        SpeechShard& sh = ShardOf(s.bot_id());
        std::lock_guard lk(sh.mtx);
        CalloutState& st = sh.callouts[s.bot_id()];

        // Bands with hysteresis: a value must recover clearly before the next drop is news again.
        uint8 hp_band = st.hp_band;
        if (!alive || hp >= 60)                 hp_band = 0;
        else if (hp_band_now > hp_band)         hp_band = hp_band_now;
        else if (hp_band == 2 && hp >= 30)      hp_band = 1;

        bool mana_low = st.mana_low;
        if (!alive || !has_mana || mana >= 30)  mana_low = false;
        else if (mana < 15)                     mana_low = true;

        uint8 ammo_band = 0;
        if (hunter)
        {
            if (ammo == 0)                      ammo_band = 2;
            else if (ammo < 100)                ammo_band = 1;
            else if (ammo < 150)                ammo_band = std::min<uint8>(st.ammo_band, 1);
        }

        bool const primed = st.primed;
        // A healer asks nobody for a heal: it heals itself (Deadmines 2026-10-09: the priest asked for healers while dying).
        bool const hp_worse   = in_combat && hp_band > st.hp_band && ai.effective_role(s) != Role::Healer;
        bool const mana_worse = in_combat && mana_low && !st.mana_low;
        // Ammo is news out of combat too (ike3: a non-combat trigger). On the first look a bot that
        // has no ammo at all says so; one that merely has little does not announce it on login.
        bool const ammo_worse = alive && ammo_band > st.ammo_band && (primed || ammo_band == 2);

        if (!in_combat)
        {
            st.taunt_said = false;
            st.aoe_said = false;
        }

        // Most urgent first; one line per tick.
        if (hp_worse && hp_band == 2)                     { kind = CK_CriticalHealth; key = "critical health"; }
        else if (ammo_worse && ammo_band == 2)            { kind = CK_NoAmmo;         key = "no ammo"; }
        else if (hp_worse && hp_band == 1)                { kind = CK_LowHealth;      key = "low health"; }
        else if (mana_worse)                              { kind = CK_LowMana;        key = "low mana"; }
        else if (ammo_worse && ammo_band == 1)            { kind = CK_LowAmmo;        key = "low ammo"; }
        else if (taunt_target && !st.taunt_said)          { kind = CK_Taunt;          key = "taunt"; st.taunt_said = true; }
        else if (big_pull && !st.aoe_said)                { kind = CK_Aoe;            key = "aoe"; st.aoe_said = true; }

        st.primed    = true;
        st.hp_band   = hp_band;
        st.mana_low  = mana_low;
        st.ammo_band = ammo_band;

        if (!key)
            return;
        bool const banter = kind == CK_Taunt || kind == CK_Aoe;
        if (verbosity < (banter ? Verbosity::Normal : Verbosity::Terse))
            return;
        if (!mgr.HasKey(key) || !mgr.RollChance(key))
            return;
        if (st.last_ms[kind] != 0 && !Elapsed(now_ms, st.last_ms[kind], cfg.callout_key_cooldown_ms))
            return;

        // Group gap and realm cap. Heal / ammo requests are not held back by the group gap.
        {
            std::lock_guard glk(g_speech_mtx);
            uint64 const group = s.group_guid().GetCounter();
            auto it = g_callout_group_ms.find(group);
            if (banter && it != g_callout_group_ms.end() && !Elapsed(now_ms, it->second, cfg.callout_group_cooldown_ms))
                return;
            if (!TakeGlobalLocked(now_ms))
                return;
            if (g_callout_group_ms.size() >= kLimiterPruneAt)
                Prune(g_callout_group_ms, now_ms, cfg.callout_group_cooldown_ms);
            g_callout_group_ms[group] = now_ms;
        }
        st.last_ms[kind] = now_ms ? now_ms : 1;
    }

    // ike3 SayAction placeholders.
    TextPlaceholders ph;
    if (taunt_target)
        ph.emplace_back("<target>", CreatureName(*taunt_target));
    else if (NearbyUnit const* t = s.target_info())
        ph.emplace_back("<target>", CreatureName(*t));
    ph.emplace_back("<randomfaction>", s.is_alliance() ? "Alliance" : "Horde");
    ph.emplace_back("<ammo>", s.ammo_kind() == 2 ? "bullets" : "arrows");
    ph.emplace_back("<subzone>", AreaName(s.area_id()));
    std::string text = mgr.Get(key, ph);
    if (text.empty() || text.find("<target>") != std::string::npos)
        return;
    // A taunt is shouted at the mob; the rest is said to the group.
    EmitLine(em, std::move(text), /*party*/ kind != CK_Taunt);
}

// ---- world bots: ike3 SuggestWhatToDoAction ----

constexpr uint8 kChannelGeneral = 1;   // ChatChannels.db2
constexpr uint8 kChannelTrade   = 2;

struct LevelledName { char const* name; uint8 min_level; uint8 team; };   // team 0 = both, 1 = Alliance, 2 = Horde

// ike3's vanilla instance list (SuggestWhatToDoAction::instance). ike3 offered every instance the bot
// had outlevelled; a level 60 asking for Ragefire Chasm reads as noise, so only instances up to 12
// levels below the bot are suggested.
constexpr LevelledName kInstances[] = {
    { "Ragefire Chasm", 15, 2 }, { "Deadmines", 18, 1 }, { "Wailing Caverns", 18, 0 }, { "Shadowfang Keep", 25, 0 },
    { "Blackfathom Deeps", 20, 0 }, { "Stockade", 20, 1 }, { "Gnomeregan", 35, 0 }, { "Razorfen Kraul", 35, 0 },
    { "Maraudon", 50, 0 }, { "Scarlet Monastery", 40, 0 }, { "Uldaman", 45, 0 }, { "Dire Maul", 58, 0 },
    { "Scholomance", 59, 0 }, { "Razorfen Downs", 40, 0 }, { "Stratholme", 59, 0 }, { "Zul'Farrak", 45, 0 },
    { "Blackrock Depths", 55, 0 }, { "Temple of Atal'Hakkar", 55, 0 }, { "Lower Blackrock Spire", 57, 0 },
};

// ike3's vanilla reputation list (SuggestWhatToDoAction::grindReputation).
constexpr LevelledName kFactions[] = {
    { "Argent Dawn", 60, 0 }, { "Bloodsail Buccaneers", 40, 0 }, { "Brood of Nozdormu", 60, 0 },
    { "Cenarion Circle", 55, 0 }, { "Darkmoon Faire", 20, 0 }, { "Hydraxian Waterlords", 60, 0 },
    { "Ravenholdt", 20, 0 }, { "Thorium Brotherhood", 40, 0 }, { "Timbermaw Hold", 50, 0 },
    { "Wintersaber Trainers", 50, 1 }, { "Booty Bay", 30, 0 }, { "Everlook", 40, 0 }, { "Gadgetzan", 50, 0 },
    { "Ratchet", 20, 0 },
};

// suggest_trade's %category. ike3 picked a reagent of the bot's own crafting skills; the vanilla
// gathering materials of the bot's level band stand in for that (cloth, ore, leather, herbs).
struct Material { uint32 item; uint8 min_level; uint8 max_level; };
constexpr Material kMaterials[] = {
    { 2589, 5, 20 }, { 2592, 15, 30 }, { 4306, 25, 40 }, { 4338, 35, 50 }, { 14047, 45, 60 },       // cloth
    { 2770, 5, 20 }, { 2771, 15, 30 }, { 2772, 28, 45 }, { 3858, 38, 55 }, { 10620, 48, 60 },       // ore
    { 2318, 5, 20 }, { 2319, 15, 30 }, { 4234, 25, 40 }, { 4304, 35, 50 }, { 8170, 45, 60 },        // leather
    { 2447, 5, 15 }, { 765, 5, 15 }, { 2450, 12, 25 }, { 2453, 18, 30 }, { 3356, 25, 40 },          // herbs
    { 3821, 35, 48 }, { 8838, 40, 55 }, { 13463, 48, 60 }, { 13466, 52, 60 },
    { 12363, 55, 60 }, { 7068, 40, 60 }, { 7076, 45, 60 },                                          // crystals, elementals
};

bool TeamFits(uint8 team, BotSnapshotView const& s)
{
    return team == 0 || (team == 1 && s.is_alliance()) || (team == 2 && s.is_horde());
}

// ike3 ChatHelper::formatMoney.
std::string FormatMoney(uint64 copper)
{
    if (!copper)
        return "0";
    uint64 const gold = copper / 10000;
    uint64 const silver = (copper % 10000) / 100;
    uint64 const cu = copper % 100;
    std::string out;
    if (gold)
        out += Trinity::StringFormat("{}g", gold);
    if (silver && gold < 50)
        out += Trinity::StringFormat("{}{}s", out.empty() ? "" : " ", silver);
    if (cu && gold < 10)
        out += Trinity::StringFormat("{}{}c", out.empty() ? "" : " ", cu);
    return out;
}

struct Suggestion
{
    char const*      key = nullptr;
    uint8            channel = kChannelGeneral;
    TextPlaceholders ph;
};

bool SuggestInstance(BotSnapshotView const& s, Suggestion& out)
{
    std::vector<char const*> pool;
    for (LevelledName const& i : kInstances)
        if (s.level() >= i.min_level && s.level() <= i.min_level + 12 && TeamFits(i.team, s))
            pool.push_back(i.name);
    if (pool.empty())
        return false;
    out.key = "suggest_instance";
    out.ph.emplace_back("%instance_name", pool[urand(0, uint32(pool.size() - 1))]);
    return true;
}

bool SuggestQuest(BotSnapshotView const& s, Suggestion& out)
{
    std::vector<QuestEntry const*> pool;
    for (QuestEntry const& q : s.quests())
        if (q.state == 0 && !q.unachievable)
            pool.push_back(&q);
    if (pool.empty())
        return false;
    QuestEntry const& q = *pool[urand(0, uint32(pool.size() - 1))];
    Quest const* quest = sObjectMgr->GetQuestTemplate(q.quest_id);
    if (!quest)
        return false;
    out.key = "suggest_quest";
    out.ph.emplace_back("%quest_link", Trinity::StringFormat("|cffffff00|Hquest:{}:{}:{}:0:0|h[{}]|h|r",
        q.quest_id, uint32(q.level), uint32(q.min_level), quest->GetLogTitle()));
    out.ph.emplace_back("%quest_level", std::to_string(q.level));
    return true;
}

bool SuggestMaterials(BotSnapshotView const& s, Suggestion& out)
{
    if (s.level() <= 5)
        return false;
    std::vector<ItemTemplate const*> pool;
    for (Material const& m : kMaterials)
        if (s.level() >= m.min_level && s.level() <= m.max_level)
            if (ItemTemplate const* proto = sObjectMgr->GetItemTemplate(m.item))
                pool.push_back(proto);
    if (pool.empty())
        return false;
    out.key = "suggest_trade";
    out.channel = kChannelTrade;
    out.ph.emplace_back("%category", ItemLink(pool[urand(0, uint32(pool.size() - 1))], s.level()));
    return true;
}

bool SuggestFaction(BotSnapshotView const& s, Suggestion& out)
{
    std::vector<char const*> pool;
    for (LevelledName const& f : kFactions)
        if (s.level() >= f.min_level && TeamFits(f.team, s))
            pool.push_back(f.name);
    if (pool.empty())
        return false;
    static constexpr char const* kRepLevels[] = { "honored", "revered", "exalted" };
    out.key = "suggest_faction";
    out.ph.emplace_back("%faction", pool[urand(0, uint32(pool.size() - 1))]);
    out.ph.emplace_back("%rep_level", kRepLevels[urand(0, 2)]);
    out.ph.emplace_back("%rndK", Trinity::StringFormat("{}K", urand(1, 5)));
    return true;
}

// ike3 SuggestTradeAction: an unbound trade good from the bags, full stacks first, priced in gold.
bool SuggestSell(BotSnapshotView const& s, Suggestion& out)
{
    std::vector<std::pair<ItemTemplate const*, uint32>> stacks, singles;
    for (InventoryItem const& it : s.bag_items())
    {
        if (it.item_class != ITEM_CLASS_TRADE_GOODS || it.quality < ITEM_QUALITY_NORMAL || it.is_quest_item)
            continue;
        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(it.entry);
        if (!proto || proto->GetBonding() != BIND_NONE)
            continue;
        uint32 const max_stack = proto->GetMaxStackSize();
        (max_stack > 1 && it.count == max_stack ? stacks : singles).emplace_back(proto, it.count);
    }
    auto& pool = stacks.empty() ? singles : stacks;
    if (pool.empty())
        return false;
    auto const& [proto, count] = pool[urand(0, uint32(pool.size() - 1))];
    uint64 const unit = proto->GetBuyPrice() ? proto->GetBuyPrice() : uint64(proto->GetSellPrice()) * 4;
    if (!unit)
        return false;
    std::string const link = ItemLink(proto, s.level());
    out.key = "suggest_sell";
    out.channel = kChannelTrade;
    out.ph.emplace_back("%item_formatted_link", count > 1 ? Trinity::StringFormat("{}x{}", link, count) : link);
    out.ph.emplace_back("%item_link", link);
    out.ph.emplace_back("%item_count", std::to_string(count));
    out.ph.emplace_back("%cost_gold", FormatMoney(unit * count));
    return true;
}

bool SuggestSomething(BotSnapshotView const&, Suggestion& out)
{
    out.key = "suggest_something";
    return true;
}

// ike3's per-kind broadcast chances (out of 30000) as relative odds of the kind being picked:
// instance 5000, quest 10000, grind materials 5000, reputation 5000, sell 300, something 30000.
struct SuggestKind { bool (*make)(BotSnapshotView const&, Suggestion&); uint32 weight; };
constexpr SuggestKind kSuggestKinds[] = {
    { &SuggestInstance, 5000 }, { &SuggestQuest, 10000 }, { &SuggestMaterials, 5000 },
    { &SuggestFaction, 5000 },  { &SuggestSell, 300 },    { &SuggestSomething, 30000 },
};

void Suggest(BotAI& ai, BotSnapshotView const& s, BotIntentEmitter& em, uint32 now_ms)
{
    BotTextMgr const& mgr = BotTextMgr::instance();
    BotTextMgr::SpeechConfig const& cfg = mgr.Speech();
    if (!cfg.suggest_enabled)
        return;
    // ike3: random (world) bots only, solo, out of instances. Companions have an owner.
    if (s.has_owner_character() || s.in_group() || !s.is_alive() || s.is_ghost() || s.in_combat()
        || s.is_in_instance() || s.in_battleground())
        return;
    if (ai.personality().verbosity < Verbosity::Normal)
        return;
    if (!SuggestGateOpen(now_ms))
        return;

    // Per bot: the first look primes a random point inside one cooldown, so a fleet that logs in
    // together does not queue up to talk the moment the realm gate opens.
    {
        SpeechShard& sh = ShardOf(s.bot_id());
        std::lock_guard lk(sh.mtx);
        auto [it, fresh] = sh.suggest_ms.try_emplace(s.bot_id(), 0u);
        if (fresh)
        {
            it->second = now_ms - urand(0, cfg.suggest_bot_cooldown_ms);
            return;
        }
        if (!Elapsed(now_ms, it->second, cfg.suggest_bot_cooldown_ms))
            return;
    }

    AreaTableEntry const* zone = sAreaTableStore.LookupEntry(s.zone_id());
    if (!zone || zone->GetFlags().HasFlag(AreaFlags::NoChatChannels))
        return;

    // Weighted pick among the kinds the bot has something to say about.
    Suggestion sug;
    {
        bool picked = false;
        std::array<bool, std::size(kSuggestKinds)> tried{};
        for (size_t attempt = 0; attempt < std::size(kSuggestKinds) && !picked; ++attempt)
        {
            uint32 total = 0;
            for (size_t i = 0; i < std::size(kSuggestKinds); ++i)
                if (!tried[i])
                    total += kSuggestKinds[i].weight;
            if (!total)
                break;
            uint32 roll = urand(0, total - 1);
            size_t chosen = 0;
            for (size_t i = 0; i < std::size(kSuggestKinds); ++i)
            {
                if (tried[i])
                    continue;
                if (roll < kSuggestKinds[i].weight) { chosen = i; break; }
                roll -= kSuggestKinds[i].weight;
            }
            tried[chosen] = true;
            sug = Suggestion{};
            picked = kSuggestKinds[chosen].make(s, sug) && mgr.HasKey(sug.key);
        }
        if (!picked)
            return;
    }

    // ike3 posted trade talk to Trade when in a city (sell 90%, materials 50%), else to General.
    bool const trade_here = zone->GetFlags().HasFlag(AreaFlags::AllowTradeChannel);
    if (sug.channel == kChannelTrade)
    {
        uint32 const trade_pct = std::string_view(sug.key) == "suggest_sell" ? 90 : 50;
        if (!trade_here || urand(1, 100) > trade_pct)
            sug.channel = kChannelGeneral;
    }
    // Trade is one channel for every city of a faction; General is per zone.
    uint64 const channel_key = (uint64(s.team()) << 48) | (uint64(sug.channel) << 32) |
                               (sug.channel == kChannelTrade ? 0u : s.zone_id());

    {
        std::lock_guard lk(g_speech_mtx);
        if (!SuggestGateOpen(now_ms))
            return;   // another bot took the realm slot meanwhile
        auto it = g_channel_ms.find(channel_key);
        if (it != g_channel_ms.end() && !Elapsed(now_ms, it->second, cfg.suggest_channel_cooldown_ms))
            return;
        if (!TakeGlobalLocked(now_ms))
            return;
        if (g_channel_ms.size() >= kLimiterPruneAt)
            Prune(g_channel_ms, now_ms, cfg.suggest_channel_cooldown_ms);
        g_channel_ms[channel_key] = now_ms;
        // The realm gap is jittered up to twice its length, so lines do not arrive on a beat.
        uint32 const gap = cfg.suggest_global_cooldown_ms + urand(0, cfg.suggest_global_cooldown_ms);
        uint32 const next = now_ms + std::max<uint32>(gap, 1);
        g_suggest_next_ms.store(next ? next : 1, std::memory_order_relaxed);
    }
    {
        SpeechShard& sh = ShardOf(s.bot_id());
        std::lock_guard lk(sh.mtx);
        sh.suggest_ms[s.bot_id()] = now_ms;
    }

    sug.ph.emplace_back("%my_role", RoleName(s));
    AddSelfPlaceholders(sug.ph, s.cls(), s.race(), s.level(), s.zone_id(), s.area_id());
    std::string text = mgr.Get(sug.key, sug.ph);
    if (text.empty())
        return;
    em.emit(ChatIntent{ ChannelChatIntent{ sug.channel, std::move(text) } });
}

// ---- companions: party banter and social emotes ----

constexpr uint32 kBanterBotCooldownMs = 90000;   // one banter line per bot per 90 s
constexpr uint32 kEmoteBotCooldownMs  = 90000;   // one social emote per bot per 90 s

std::mutex                         g_banter_mtx;
std::unordered_map<BotId, uint32>  g_bot_banter_ms;
std::unordered_map<BotId, uint32>  g_bot_emote_ms;

// Chance (percent) that a trigger is spoken at all: about 1 in 3, a wipe or a thank-you a little more often.
uint32 BanterChance(Banter kind)
{
    switch (kind)
    {
        case Banter::Wipe:
        case Banter::Thanks:  return 50;
        case Banter::BossKill: return 40;
        default:              return 33;
    }
}

// The roll first (a trigger that is not spoken burns no cooldown), then the group gap (shared with the ike3
// broadcasts: one unsolicited line per group per Playerbot.Chatter.GroupCooldownMs) and the 90 s per-bot gap.
bool TakeBanterSlot(Banter kind, uint64 group_low, BotId bot, uint32 now_ms)
{
    if (urand(1, 100) > BanterChance(kind))
        return false;
    uint32 const group_gap = BotTextMgr::instance().GroupCooldownMs();
    std::lock_guard lk(g_slot_mtx);
    auto git = g_group_broadcast_ms.find(group_low);
    if (git != g_group_broadcast_ms.end() && now_ms - git->second < group_gap)
        return false;
    auto bit = g_bot_banter_ms.find(bot);
    if (bit != g_bot_banter_ms.end() && now_ms - bit->second < kBanterBotCooldownMs)
        return false;
    Prune(g_group_broadcast_ms, now_ms, group_gap);
    Prune(g_bot_banter_ms, now_ms, kBanterBotCooldownMs);
    g_group_broadcast_ms[group_low] = now_ms;
    g_bot_banter_ms[bot] = now_ms;
    return true;
}

bool TakeEmoteSlot(BotId bot, uint32 now_ms)
{
    std::lock_guard lk(g_banter_mtx);
    auto it = g_bot_emote_ms.find(bot);
    if (it != g_bot_emote_ms.end() && now_ms - it->second < kEmoteBotCooldownMs)
        return false;
    Prune(g_bot_emote_ms, now_ms, kEmoteBotCooldownMs);
    g_bot_emote_ms[bot] = now_ms;
    return true;
}

struct LineSpan { char const* const* lines; size_t count; };
template <size_t N>
constexpr LineSpan Span(char const* const (&a)[N]) { return { a, N }; }

// Short, lowercase, the way people type in party chat. Class / role / personality lines are added to the base
// pool, so a warrior tank and a priest healer do not sound alike.
std::string PickBanter(Banter kind, uint8 cls, Role role, BotPersonality const& pers)
{
    bool const tank   = role == Role::Tank;
    bool const healer = role == Role::Healer;
    bool const caster = cls == CLASS_MAGE || cls == CLASS_WARLOCK || cls == CLASS_PRIEST;
    bool const polite = pers.politeness == Politeness::Polite;
    bool const rude   = pers.politeness == Politeness::Rude;
    bool const rp     = pers.verbosity == Verbosity::Roleplay;

    std::array<LineSpan, 6> pools{};
    size_t n = 0;
    auto add = [&](LineSpan s) { pools[n++] = s; };

    switch (kind)
    {
        case Banter::Ready:
        {
            static constexpr char const* kBase[]   = { "ready", "rdy", "ready", "yep ready", "good to go", "all set", "ready :)", "yup", "ready when you are" };
            static constexpr char const* kTank[]   = { "ready to tank", "tank ready" };
            static constexpr char const* kHealer[] = { "healer ready", "heals ready" };
            static constexpr char const* kCaster[] = { "ready, mana is good", "mana is good" };
            static constexpr char const* kRp[]     = { "aye, ready", "ready, lead on" };
            add(Span(kBase));
            if (tank)   add(Span(kTank));
            if (healer) add(Span(kHealer));
            else if (caster) add(Span(kCaster));
            if (rp)     add(Span(kRp));
            break;
        }
        case Banter::Wipe:
        {
            static constexpr char const* kBase[]   = { "my bad", "wp", "wp wp", "ouch", "rip", "well that went well", "oof", "that was rough", "rip us", "wow ok" };
            static constexpr char const* kTank[]   = { "my bad, bad pull", "sorry, lost them" };
            static constexpr char const* kHealer[] = { "sorry, too slow with the heals", "my bad, couldn't keep up" };
            static constexpr char const* kPolite[] = { "sorry everyone", "sorry guys, my fault" };
            static constexpr char const* kRude[]   = { "great job everyone", "wow" };
            add(Span(kBase));
            if (tank)   add(Span(kTank));
            if (healer) add(Span(kHealer));
            if (polite) add(Span(kPolite));
            if (rude)   add(Span(kRude));
            break;
        }
        case Banter::Thanks:
        {
            static constexpr char const* kBase[]   = { "ty", "thanks", "thx", "ty!", "tyvm", "ty rez", "thx rez", "ty :)", "thanks for the rez" };
            static constexpr char const* kPolite[] = { "thank you!", "much appreciated" };
            static constexpr char const* kRude[]   = { "finally", "about time" };
            add(Span(kBase));
            if (polite) add(Span(kPolite));
            if (rude)   add(Span(kRude));
            break;
        }
        case Banter::Ouch:
        {
            static constexpr char const* kBase[]   = { "ouch", "ow", "oof", "that hurt", "ugh", "argh", "rip", "owie" };
            static constexpr char const* kTank[]   = { "that stings", "ow, my armor" };
            static constexpr char const* kCaster[] = { "squishy, i know", "paper armor strikes again" };
            add(Span(kBase));
            if (tank)   add(Span(kTank));
            if (caster) add(Span(kCaster));
            break;
        }
        case Banter::BossKill:
        {
            static constexpr char const* kBase[]   = { "gg", "gg!", "gg wp", "nice", "well done", "nice one", "woo", "finally", "good one", "that went well" };
            static constexpr char const* kPolite[] = { "gg everyone", "well played all" };
            static constexpr char const* kTank[]   = { "easy", "gg, nice heals" };
            static constexpr char const* kRp[]     = { "victory!", "for glory, gg" };
            add(Span(kBase));
            if (polite) add(Span(kPolite));
            if (tank)   add(Span(kTank));
            if (rp)     add(Span(kRp));
            break;
        }
    }
    size_t total = 0;
    for (size_t i = 0; i < n; ++i)
        total += pools[i].count;
    if (!total)
        return {};
    size_t pick = urand(0, uint32(total - 1));
    for (size_t i = 0; i < n; ++i)
    {
        if (pick < pools[i].count)
            return pools[i].lines[pick];
        pick -= pools[i].count;
    }
    return {};
}

// Edge-driven banter for the events the snapshot shows (the world-thread kill listener covers boss kills):
// a companion that was alive on the last look and is dead on this one says "ouch" now and then. Only
// companions are tracked, so the map is bounded by the roster.
struct BanterEdge { bool primed = false; bool alive = true; };
std::array<std::pair<std::mutex, std::unordered_map<BotId, BanterEdge>>, kSpeechShards> g_banter_edges;

void BanterTick(BotAI& ai, BotSnapshotView const& s, BotIntentEmitter& em)
{
    if (!Services::Config().behavior_party_banter() || !s.is_companion_with_owner() || !s.in_group())
        return;
    bool const alive = s.is_alive() && !s.is_ghost();
    bool died = false;
    {
        auto& [mtx, map] = g_banter_edges[s.bot_id() % kSpeechShards];
        std::lock_guard lk(mtx);
        BanterEdge& e = map[s.bot_id()];
        died = e.primed && e.alive && !s.is_alive();
        e.primed = true;
        e.alive = alive;
    }
    if (!died)
        return;
    std::string line = BanterLine(Banter::Ouch, ai, s);
    if (!line.empty())
        em.say(std::move(line));
}

void PushEmote(BotId id, uint32 emote_id, uint32 defer_ms)
{
    Intent it{};
    it.bot_id = id;
    it.body = PerformEmoteIntent{ emote_id, ObjectGuid::Empty };
    it.defer_until_ms = defer_ms ? GameTime::GetGameTimeMS() + defer_ms : 0;
    Services::Intents(id).push(std::move(it));
}

} // namespace

std::string BanterLine(Banter kind, BotAI& ai, BotSnapshotView const& s)
{
    if (!Services::Config().behavior_party_banter())
        return {};
    // A companion, in a group that is not a big raid, with chatter on and a personality that talks.
    if (!BotTextMgr::instance().Enabled() || s.big_raid() || !s.in_group() || !s.is_companion_with_owner())
        return {};
    BotPersonality const& pers = ai.personality();
    if (pers.verbosity < Verbosity::Normal)
        return {};
    if (!TakeBanterSlot(kind, s.group_guid().GetCounter(), s.bot_id(), s.published_at_ms()))
        return {};
    return PickBanter(kind, s.cls(), ai.effective_role(s), pers);
}

void OnBossKilled(Player* killer, Creature* boss)
{
    if (!killer || !boss || !(boss->IsDungeonBoss() || boss->isWorldBoss()))
        return;
    Group* group = killer->GetGroup();
    if (!group || group->GetMembersCount() > 15)
        return;
    ConfigReader const& cfg = Services::Config();
    bool const banter = cfg.behavior_party_banter();
    bool const emotes = cfg.behavior_social_emotes();
    if (!banter && !emotes)
        return;
    bool const owner_killed = killer->GetSession() && !killer->GetSession()->IsBot();

    // Companions of the group on this map, near enough to have seen the kill, in random order.
    std::vector<Player*> cands;
    for (GroupReference const& ref : group->GetMembers())
    {
        Player* m = ref.GetSource();
        if (m && m->IsAlive() && m->GetMapId() == boss->GetMapId() && m->IsWithinDist(boss, 80.0f) &&
            m->GetSession() && m->GetSession()->IsBot())
            cands.push_back(m);
    }
    for (size_t i = cands.size(); i > 1; --i)
        std::swap(cands[i - 1], cands[urand(0, uint32(i - 1))]);

    uint32 const now_ms = GameTime::GetGameTimeMS();
    bool spoke = !banter, moved = !emotes;
    for (Player* m : cands)
    {
        if (spoke && moved)
            break;
        BotAI* ai = ChattyAi(m, Verbosity::Normal);
        if (!ai || !GroupHasRealPlayer(m))
            continue;
        BotId const id = m->GetGUID().GetCounter();
        if (!spoke && TakeBanterSlot(Banter::BossKill, group->GetGUID().GetCounter(), id, now_ms))
        {
            std::string text = PickBanter(Banter::BossKill, m->GetClass(), Role::Unknown, ai->personality());
            if (!text.empty())
                PushChat(id, ChatIntent{ PartyChatIntent{ std::move(text) } }, urand(2000, 5000));
            spoke = true;
        }
        // Bow when the real player (the owner) finished the boss, cheer when it fell to a bot.
        if (!moved && urand(1, 100) <= 50 && TakeEmoteSlot(id, now_ms))
        {
            PushEmote(id, owner_killed ? EMOTE_ONESHOT_BOW : EMOTE_ONESHOT_CHEER, urand(1500, 4500));
            moved = true;
        }
    }
}

void SpeechTick(BotAI& ai, BotSnapshotView const& s, BotIntentEmitter& em)
{
    if (!BotTextMgr::instance().Enabled())
        return;
    uint32 const now_ms = s.published_at_ms();
    Callouts(ai, s, em, now_ms);
    Suggest(ai, s, em, now_ms);
    BanterTick(ai, s, em);
}

} // namespace BotChatter

} // namespace Playerbot
