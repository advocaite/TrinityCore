// BotBridge - see header.

#include "BotBridge.h"
#include "BotAI.h"
#include "BotBridgeFormat.h"
#include "BotCommandParser.h"
#include "BotIntent.h"
#include "BotIntentEmitter.h"
#include "BotRegistry.h"
#include "BotSnapshot.h"
#include "ChannelTalk.h"
#include "ClassicUpkeep.h"
#include "Quest/QuestOrders.h"
#include "../Api/BotTradeShop.h"
#include "../Diagnostics/ScenarioHuman.h"
#include "../Fleet/BotGuildCharter.h"
#include "../Fleet/BotPool.h"
#include "World/ZonePicker.h"
#include "../Fleet/OwnerRegistry.h"
#include "../Fleet/PlayerCraftOrders.h"
#include "../Services.h"
#include "../Threading/IntentQueue.h"
#include "../Threading/SnapshotPublisher.h"
#include "../Util/ConfigReader.h"

#include "Cell.h"
#include "CellImpl.h"
#include "Creature.h"
#include "DB2Stores.h"
#include "GameObject.h"
#include "GameTime.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Group.h"
#include "Item.h"
#include "Log.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "ObjectGuid.h"
#include "ObjectMgr.h"
#include "PathGenerator.h"
#include "PetitionMgr.h"
#include "Player.h"
#include "QuestDef.h"
#include "SpellAuraEffects.h"
#include "SpellHistory.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "TradeData.h"
#include "TradePackets.h"
#include "Weather.h"
#include "World.h"
#include "WorldSession.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <ctime>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>

namespace Playerbot {

namespace {

namespace fs = std::filesystem;
using namespace Bridge;

constexpr char const* kRepliesFile = "replies.jsonl";
constexpr char const* kStatusFile  = "status.json";
constexpr char const* kRunningFlag = "running.flag";
constexpr std::size_t kMaxQueuedLines = 5000;      // writer queue; the oldest are dropped past this
constexpr std::size_t kMaxQueuedReplies = 1000;    // reply lines read but not handled yet
constexpr std::size_t kReadChunk = 256 * 1024;     // bytes of replies.jsonl read per second at most
constexpr uint32 kRepliesPerTick = 50;
constexpr char const* kRepliesOld = "replies.old.jsonl";
constexpr uintmax_t kRepliesRotateBytes = 1u << 20;   // replies.jsonl read to its end and past this: moved aside

std::atomic<bool> g_enabled{false};
std::atomic<bool> g_client_alive{false};

// ---- queued reports (any thread -> world thread) ----
struct Report
{
    BotId       bot = 0;
    std::string type;
    uint32      quest_id = 0;
    std::string detail;
    std::vector<std::pair<std::string, std::string>> fields;
    std::vector<std::pair<std::string, int64>> numbers;
};
std::mutex          g_in_mtx;
std::vector<Report> g_in;

// ---- writer thread ----
std::mutex              g_out_mtx;
std::condition_variable g_out_cv;
std::deque<std::string> g_out;            // event lines
std::string             g_status;         // status.json content, written when dirty
bool                    g_status_dirty = false;
bool                    g_out_stop = false;
fs::path                g_dir;            // set before the thread starts, then read only
uint64                  g_rotate_bytes = 20ull << 20;
uint32                  g_keep_days = 3;
std::atomic<uint64>     g_out_dropped{0};
std::thread             g_writer;

// ---- world thread only ----
bool        g_started = false;
uint64      g_next_id = 1;
EventGate   g_gate{300000, 30};
RepeatCounter g_casts;
RepeatCounter g_deaths;
uintmax_t   g_offset = 0;
fs::file_time_type g_replies_mtime{};   // the newest write to replies.jsonl seen (kept while the file is moved aside)
bool        g_replies_seen = false;
std::string g_carry;
std::deque<std::pair<std::string, uint32>> g_reply_lines;   // line, when it was read (ms)
uint32      g_next_read_ms = 0;
uint32      g_next_status_ms = 0;
uint32      g_started_ms = 0;
std::unordered_map<BotId, uint32> g_last_line_ms;
uint32      g_bad_lines = 0;
uint32      g_last_bad_warn_ms = 0;
struct ChatAsk { BotId bot = 0; std::string sender; ObjectGuid sender_guid; bool sender_is_bot = false; std::string channel; uint32 asked_ms = 0;
                 uint64 talk_key = 0;         // a bot-to-bot talk line (AskTalk), kept for TakeTalk
                 std::vector<BotId> group; }; // a group order (AskGroupOrder): the other bots that take the answer's action too
// The group's part of a group order: each other bot runs the answer's action a moment after the first (HandleGroupFollowups).
struct GroupFollowup { BotId bot = 0; ChatAsk ask; Reply reply; uint32 read_ms = 0, due_ms = 0; };
std::vector<GroupFollowup> g_group_followups;
// Channel posts (QueuePost): what the server asked for, by event id, until the answer comes; a bot's last post (for a player who
// whispers about it); the answers by other bots waiting for their time.
constexpr uint64 kNoTalkKey = ~uint64(0);   // ChatAsk::talk_key of a talk line nobody takes back (a greeting)
struct PostAsk { uint8 channel_id = 1; std::string link_name, link, example; bool answer = false; };
std::unordered_map<uint64, PostAsk> g_post_ask;
struct LastPost { std::string channel, text; uint32 ms = 0; };
std::unordered_map<BotId, LastPost> g_last_post;
struct PostAnswer { uint32 due_ms = 0; BotId poster = 0; std::string poster_name, text; uint8 channel_id = 1; uint32 team = 0, zone = 0;
                    uint8 poster_level = 1; };
std::vector<PostAnswer> g_post_answers;
std::deque<uint32> g_post_answer_minute;
std::string FinishPost(BotId bot, PostAsk const& post, std::string text, uint32 now_ms);
// Bot-to-bot talk (AskTalk): the lines said, by talk, until the talk takes them; the asks of the last minute.
std::unordered_map<uint64, std::vector<std::pair<std::string, std::string>>> g_talk_said;
std::deque<uint32> g_talk_minute;

// ---- actions (Playerbot.Bridge.Actions.*), world thread only ----
constexpr uint8 kRefQuestGiver = 1, kRefNpc = 2, kRefHostile = 3, kRefObject = 4;
RefTable<ObjectGuid> g_refs{60000};
ActionGate  g_act_gate;
std::vector<bool> g_allowed;
std::string g_allowed_src = "\x01";   // the config text g_allowed was read from (forces the first read)
// A world bot following a player who is not its group's leader: let go after the time is up or once he is 100 yards away.
struct Leash { ObjectGuid leader; uint32 until_ms = 0; };
std::unordered_map<BotId, Leash> g_leash;
// A damage spell a reply asked for: the bot is sent at the target and casts it once it is in range and sight (or gives up).
struct PendingCast { uint32 spell_id = 0; ObjectGuid target; uint32 until_ms = 0; };
std::unordered_map<BotId, PendingCast> g_pending_cast;
uint32      g_next_leash_ms = 0;
std::unordered_map<uint64, ChatAsk> g_chat;

int64 UtcNow() { return int64(std::time(nullptr)); }   // also used by the writer thread

void Push(std::string line)
{
    {
        std::lock_guard<std::mutex> lk(g_out_mtx);
        if (g_out.size() >= kMaxQueuedLines)
        {
            g_out.pop_front();
            ++g_out_dropped;
        }
        g_out.push_back(std::move(line));
    }
    g_out_cv.notify_one();
}

fs::path EventsPath(int64 utc)
{
    std::string const name = EventsFileName(utc);
    fs::path p = g_dir / name;
    std::error_code ec;
    // Past the size limit the hour goes on in events-...-1000-1.jsonl, -2, ...
    for (int n = 1; n < 50 && fs::exists(p, ec) && fs::file_size(p, ec) >= g_rotate_bytes; ++n)
        p = g_dir / (name.substr(0, name.size() - 6) + "-" + std::to_string(n) + ".jsonl");
    return p;
}

void PruneOld()
{
    std::error_code ec;
    auto const cutoff = fs::file_time_type::clock::now() - std::chrono::hours(24 * std::max<uint32>(g_keep_days, 1));
    for (fs::directory_entry const& e : fs::directory_iterator(g_dir, ec))
    {
        std::string const n = e.path().filename().string();
        if (n.rfind("events-", 0) == 0 && e.path().extension() == ".jsonl" && e.last_write_time(ec) < cutoff)
            fs::remove(e.path(), ec);
    }
}

void WriterLoop()
{
    auto next_prune = std::chrono::steady_clock::now();
    for (;;)
    {
        std::deque<std::string> lines;
        std::string status;
        bool stop = false;
        {
            std::unique_lock<std::mutex> lk(g_out_mtx);
            g_out_cv.wait_for(lk, std::chrono::seconds(1), [] { return g_out_stop || !g_out.empty() || g_status_dirty; });
            lines.swap(g_out);
            if (g_status_dirty)
            {
                status = g_status;
                g_status_dirty = false;
            }
            stop = g_out_stop;
        }
        if (!lines.empty())
        {
            std::ofstream f(EventsPath(UtcNow()), std::ios::app | std::ios::binary);
            for (std::string const& l : lines)
                f << l << '\n';
        }
        if (!status.empty())
        {
            // Through a temporary file, so a reader never sees half a status.
            std::error_code ec;
            fs::path const tmp = g_dir / "status.json.tmp";
            {
                std::ofstream f(tmp, std::ios::trunc | std::ios::binary);
                f << status << '\n';
            }
            fs::rename(tmp, g_dir / kStatusFile, ec);
        }
        if (std::chrono::steady_clock::now() >= next_prune)
        {
            PruneOld();
            next_prune = std::chrono::steady_clock::now() + std::chrono::hours(1);
        }
        if (stop)
            break;
    }
}

char const* ClassName(uint8 cls)
{
    static constexpr char const* kClass[] = { "", "warrior", "paladin", "hunter", "rogue", "priest", "death knight", "shaman",
                                               "mage", "warlock", "monk", "druid", "demon hunter", "evoker" };
    return cls < std::size(kClass) ? kClass[cls] : "";
}

char const* RaceName(uint8 race)
{
    static constexpr char const* kRace[] = { "", "human", "orc", "dwarf", "night elf", "undead", "tauren", "gnome", "troll",
                                              "goblin", "blood elf", "draenei" };
    return race < std::size(kRace) ? kRace[race] : "";
}

std::string AreaName(uint32 area_id)
{
    if (auto const* ae = sAreaTableStore.LookupEntry(area_id))
        return ae->AreaName[sWorld->GetDefaultDbcLocale()];
    return {};
}

Player* FindBot(BotId id)
{
    if (!Services::Registry().has(id))
        return nullptr;
    return ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(id));
}

// Fills the bot part of an event (name, class, level, zone, position, quest).
void FillBot(Event& e, Player* p, uint32 quest_id)
{
    e.bot = p->GetName();
    e.guid = p->GetGUID().GetCounter();
    e.cls = ClassName(p->GetClass());
    e.race = RaceName(p->GetRace());
    e.level = p->GetLevel();
    e.zone = AreaName(p->GetZoneId());
    e.area = AreaName(p->GetAreaId());
    e.map = p->GetMapId();
    e.x = p->GetPositionX();
    e.y = p->GetPositionY();
    e.z = p->GetPositionZ();
    e.has_pos = true;
    if (!quest_id)
        for (uint16 slot = 0; slot < MAX_QUEST_LOG_SIZE && !quest_id; ++slot)
            if (uint32 const q = p->GetQuestSlotQuestId(slot); q && p->GetQuestStatus(q) == QUEST_STATUS_INCOMPLETE)
                quest_id = q;
    if (quest_id)
    {
        e.quest_id = quest_id;
        if (Quest const* q = sObjectMgr->GetQuestTemplate(quest_id))
            e.quest_name = q->GetLogTitle();
    }
}

void Write(Event& e)
{
    e.id = g_next_id++;
    e.time = IsoTime(UtcNow());
    Push(BuildEventLine(e));
}

// The newest crash report in `folder` (by modification time), empty when there is none.
fs::path NewestCrash(fs::path const& folder)
{
    std::error_code ec;
    fs::path best;
    fs::file_time_type best_time{};
    for (fs::directory_entry const& e : fs::directory_iterator(folder, ec))
    {
        if (!e.is_regular_file(ec))
            continue;
        std::string const ext = e.path().extension().string();
        if (ext != ".txt" && ext != ".dmp" && ext != ".log")
            continue;
        auto const t = e.last_write_time(ec);
        // Prefer the readable .txt of a crash over its .dmp when both have the same time.
        if (best.empty() || t > best_time || (t == best_time && ext == ".txt"))
        {
            best = e.path();
            best_time = t;
        }
    }
    return best;
}

void Start(ConfigReader::BridgeConfig const& cfg, uint32 now_ms)
{
    std::error_code ec;
    g_dir = fs::absolute(fs::path(cfg.folder), ec);
    fs::create_directories(g_dir, ec);
    if (!fs::is_directory(g_dir, ec))
    {
        TC_LOG_ERROR("server.worldserver", "[PlayerbotV2] Bot bridge: cannot make the folder {}; the bridge stays off.", g_dir.string());
        g_enabled = false;
        return;
    }
    g_rotate_bytes = uint64(cfg.rotate_mb) << 20;
    g_keep_days = cfg.keep_days;

    // Replies written before this start belong to the last run.
    g_offset = fs::exists(g_dir / kRepliesFile, ec) ? fs::file_size(g_dir / kRepliesFile, ec) : 0;
    g_replies_seen = false;
    g_carry.clear();
    g_reply_lines.clear();
    g_chat.clear();
    g_last_line_ms.clear();
    g_leash.clear();

    {
        std::lock_guard<std::mutex> lk(g_out_mtx);
        g_out_stop = false;
    }
    // Ids go on from one run to the next (seconds since 1970 x 1000), so an answer to an event of the last run never meets a new one.
    g_next_id = std::max<uint64>(g_next_id, uint64(UtcNow()) * 1000u);
    g_writer = std::thread(WriterLoop);
    g_started = true;
    g_started_ms = now_ms;

    // running.flag is removed on a clean shutdown; still there means the last run ended without one (a crash or a kill).
    Event e;
    e.type = "start";
    bool const unclean = fs::exists(g_dir / kRunningFlag, ec);
    e.numbers.emplace_back("unclean_stop", unclean ? 1 : 0);
    if (unclean)
    {
        fs::path const crash = NewestCrash(fs::absolute(fs::path(cfg.crash_folder), ec));
        if (!crash.empty())
        {
            e.fields.emplace_back("last_crash", crash.string());
            auto const age = fs::file_time_type::clock::now() - fs::last_write_time(crash, ec);
            e.numbers.emplace_back("last_crash_age_s", int64(std::chrono::duration_cast<std::chrono::seconds>(age).count()));
        }
        e.detail = "the last run did not end with a clean shutdown";
    }
    Write(e);
    {
        std::ofstream f(g_dir / kRunningFlag, std::ios::trunc);
        f << IsoTime(UtcNow()) << '\n';
    }
    TC_LOG_INFO("server.loading", "[PlayerbotV2] Bot bridge on: events and replies in {}{}.", g_dir.string(),
                unclean ? " (the last run did not stop cleanly)" : "");
}

void Stop(char const* why)
{
    if (!g_started)
        return;
    Event e;
    e.type = "stop";
    e.detail = why;
    Write(e);
    {
        std::lock_guard<std::mutex> lk(g_out_mtx);
        g_out_stop = true;
    }
    g_out_cv.notify_all();
    if (g_writer.joinable())
        g_writer.join();
    std::error_code ec;
    fs::remove(g_dir / kRunningFlag, ec);
    g_started = false;
    g_client_alive = false;
    g_pending_cast.clear();
}

std::string BuildNear(Player* p, uint32 now_ms);   // below, with the actions

void DrainReports(ConfigReader::BridgeConfig const& cfg, uint32 now_ms)
{
    std::vector<Report> in;
    {
        std::lock_guard<std::mutex> lk(g_in_mtx);
        in.swap(g_in);
    }
    if (!cfg.events)
        return;
    for (Report& r : in)
    {
        uint32 repeats = 0;
        if (!g_gate.Allow(r.bot, r.type, now_ms, &repeats))
            continue;
        Player* p = FindBot(r.bot);
        if (!p)
            continue;
        Event e;
        e.type = std::move(r.type);
        FillBot(e, p, r.quest_id);
        e.detail = std::move(r.detail);
        e.fields = std::move(r.fields);
        e.numbers = std::move(r.numbers);
        if (repeats)   // the same problem came back this often since the last event of this bot and type
            e.numbers.emplace_back("repeats", repeats);
        if (cfg.actions && e.type == "stuck")
            e.raw.emplace_back("near", BuildNear(p, now_ms));
        Write(e);
    }
    if (uint32 const dropped = g_gate.TakeDropped())
    {
        Event e;
        e.type = "dropped";
        e.numbers.emplace_back("count", dropped);
        e.detail = "events left out: more than MaxEventsPerSec in one second";
        // Not through the gate: one line per update at most.
        Write(e);
    }
}

void QueueReport(Report r)
{
    std::lock_guard<std::mutex> lk(g_in_mtx);
    if (g_in.size() < 2000)
        g_in.push_back(std::move(r));
}

// Reads up to kReadChunk new bytes of `path` from `offset` into the reply queue.
void ReadFrom(fs::path const& path, uintmax_t size, uint32 now_ms)
{
    std::ifstream f(path, std::ios::binary);
    if (!f)
        return;
    f.seekg(std::streamoff(g_offset));
    std::string chunk(std::size_t(std::min<uintmax_t>(size - g_offset, kReadChunk)), '\0');
    f.read(chunk.data(), std::streamsize(chunk.size()));
    chunk.resize(std::size_t(f.gcount()));
    g_offset += chunk.size();
    for (std::string& l : SplitLines(g_carry, chunk))
    {
        if (g_reply_lines.size() >= kMaxQueuedReplies)
            g_reply_lines.pop_front();
        g_reply_lines.emplace_back(std::move(l), now_ms);
    }
}

void ReadReplies(ConfigReader::BridgeConfig const& cfg, uint32 now_ms)
{
    std::error_code ec;
    fs::path const path = g_dir / kRepliesFile;
    auto const now_ft = fs::file_time_type::clock::now();
    auto const alive = [&] { return g_replies_seen && now_ft - g_replies_mtime < std::chrono::seconds(cfg.client_timeout_sec); };
    if (!fs::exists(path, ec))
    {
        // Moved aside by the clean-up below and not written again yet: the program still counts as running until the timeout.
        g_client_alive = alive();
        g_offset = 0;
        g_carry.clear();
        return;
    }
    auto const mtime = fs::last_write_time(path, ec);
    if (!ec && (!g_replies_seen || mtime > g_replies_mtime))
    {
        g_replies_mtime = mtime;
        g_replies_seen = true;
    }
    g_client_alive = alive();
    uintmax_t const size = fs::file_size(path, ec);
    if (ec)
        return;
    if (size < g_offset)   // the program emptied or replaced the file: start over at its top
    {
        g_offset = 0;
        g_carry.clear();
    }
    if (size > g_offset)
        ReadFrom(path, size, now_ms);

    // Clean-up: a file read to its end and past 1 MB is renamed to replies.old.jsonl (the older one is replaced); the program's
    // next write makes a new replies.jsonl. On Windows the rename fails while the program has the file open, and is tried again
    // on the next read. Lines added between the read above and the rename are read once more from the renamed file.
    if (g_offset >= kRepliesRotateBytes && g_offset == size && g_carry.empty())
    {
        fs::path const old = g_dir / kRepliesOld;
        fs::remove(old, ec);
        fs::rename(path, old, ec);
        if (!ec)
        {
            if (uintmax_t const old_size = fs::file_size(old, ec); !ec && old_size > g_offset)
                ReadFrom(old, old_size, now_ms);
            g_offset = 0;
            g_carry.clear();
        }
    }
}

// delay_ms: the program's typing time for this line (it no longer waits before it writes the reply, so an action in the same
// reply happens at once and only the text waits); -1 when the program did not send one: a short typing time of our own.
void Say(BotId bot, std::string const& channel, std::string const& to, std::string text, uint32 now_ms, int32 delay_ms)
{
    uint32 const typing_ms = delay_ms >= 0 ? uint32(delay_ms) : std::min<uint32>(uint32(text.size()) * 30u, 2500u);
    Intent it{};
    it.bot_id = bot;
    if (channel == "whisper")
        it.body = ChatIntent{WhisperIntent{to, std::move(text)}};
    else if (channel == "party")
        it.body = ChatIntent{PartyChatIntent{std::move(text)}};
    else if (channel == "raid")
        it.body = ChatIntent{RaidChatIntent{std::move(text)}};
    else if (channel == "yell")
        it.body = ChatIntent{YellChatIntent{std::move(text)}};
    else if (channel == "general" || channel == "trade")
        it.body = ChatIntent{ChannelChatIntent{uint8(channel == "trade" ? 2 : 1), std::move(text)}};
    else
        it.body = ChatIntent{SayChatIntent{std::move(text)}};
    it.defer_until_ms = now_ms + typing_ms;
    Services::Intents(bot).push(std::move(it));
}

// ---- actions ----

template <class T>
bool PushIntent(BotId id, T body)
{
    IntentQueue* q = Services::Registry().intents(id);
    IntentId* next = Services::Registry().next_intent_id(id);
    if (!q || !next)
        return false;
    BotIntentEmitter emit(q, id, /*source*/ 0, next);
    return emit.emit(std::move(body));
}

void RefreshAllowed(ConfigReader::BridgeConfig const& cfg)
{
    if (cfg.actions_allowed == g_allowed_src)
        return;
    g_allowed = ParseAllowedActions(cfg.actions_allowed);
    g_allowed_src = cfg.actions_allowed;
}

bool OwnsBot(Player const* p, BotId bot)
{
    return p && p->GetSession() && Services::Owners().IsOwner(bot, p->GetSession()->GetAccountId(), p->GetGUID().GetCounter());
}

// The "near" list of an event: up to 3 quest givers with what they offer this bot, 2 other NPCs within 40 yards, 3 named objects
// within 30 yards (kind "object", for go_to), and the 3
// nearest kinds of hostile creature within Actions.NearYards (60), each listed once by its nearest one with "count" saying how many
// of that name are in range ("see the wolves over there?"). Nearest first, each with a ref number for a reply. From the bot's latest
// snapshot; names and places from the world.
std::string BuildNear(Player* p, uint32 now_ms)
{
    BotId const id = p->GetGUID().GetCounter();
    auto snap = Services::Snapshots().latest(id);
    if (!snap)
        return {};
    std::vector<NearItem> items;
    std::vector<ObjectGuid> listed;

    std::vector<std::pair<ObjectGuid, std::vector<uint32>>> givers;
    for (auto const& off : snap->quest_discovery.quest_offers)
    {
        auto it = std::find_if(givers.begin(), givers.end(), [&](auto const& g) { return g.first == off.giver; });
        if (it == givers.end())
            givers.emplace_back(off.giver, std::vector<uint32>{ off.quest_id });
        else if (it->second.size() < 5)
            it->second.push_back(off.quest_id);
    }
    std::vector<NearItem> part;
    for (auto const& [guid, quests] : givers)
    {
        WorldObject* wo = ObjectAccessor::GetWorldObject(*p, guid);
        if (!wo || !wo->IsInMap(p))
            continue;
        float const d = p->GetDistance(wo);
        if (d > 40.0f)
            continue;
        NearItem n;
        n.kind = "quest_giver";
        n.name = wo->GetName();
        n.dist = d;
        for (uint32 q : quests)
            if (Quest const* t = sObjectMgr->GetQuestTemplate(q))
                n.quests.emplace_back(q, t->GetLogTitle());
        part.push_back(std::move(n));
        listed.push_back(guid);
    }
    auto take = [&](std::vector<NearItem>& v, std::vector<ObjectGuid> const& guids, std::size_t max, uint8 kind)
    {
        std::vector<std::size_t> order(v.size());
        for (std::size_t i = 0; i < order.size(); ++i)
            order[i] = i;
        std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) { return v[a].dist < v[b].dist; });
        for (std::size_t k = 0; k < order.size() && k < max; ++k)
        {
            NearItem n = std::move(v[order[k]]);
            n.ref = g_refs.Add(id, guids[order[k]], kind, now_ms);
            items.push_back(std::move(n));
        }
    };
    take(part, listed, 3, kRefQuestGiver);

    float const hostile_yards = float(Services::Config().bridge().actions_near_yards);
    auto units = [&](std::vector<NearbyUnit> const& from, bool hostile, std::size_t max, uint8 kind)
    {
        std::vector<NearItem> v;
        std::vector<ObjectGuid> guids;
        for (NearbyUnit const& u : from)
        {
            if (u.is_player || std::find(listed.begin(), listed.end(), u.guid) != listed.end())
                continue;
            if (!hostile && u.npc_flags == 0)
                continue;
            Unit* unit = ObjectAccessor::GetUnit(*p, u.guid);
            if (!unit || !unit->IsAlive() || unit->GetTypeId() != TYPEID_UNIT || unit->GetCharmerOrOwnerPlayerOrPlayerItself())
                continue;
            float const d = p->GetDistance(unit);
            if (d > (hostile ? hostile_yards : 40.0f))
                continue;
            listed.push_back(u.guid);
            if (hostile)
            {
                // One entry per name: the nearest one, counting the rest.
                auto same = std::find_if(v.begin(), v.end(), [&](NearItem const& o) { return o.name == unit->GetName(); });
                if (same != v.end())
                {
                    ++same->count;
                    if (d < same->dist)
                    {
                        same->dist = d;
                        same->level = unit->GetLevel();
                        guids[std::size_t(same - v.begin())] = u.guid;
                    }
                    continue;
                }
            }
            NearItem n;
            n.kind = hostile ? "hostile" : "npc";
            n.name = unit->GetName();
            n.level = unit->GetLevel();
            n.dist = d;
            v.push_back(std::move(n));
            guids.push_back(u.guid);
        }
        take(v, guids, max, kind);
    };
    units(snap->combat.nearby_enemies, true, 3, kRefHostile);
    units(snap->combat.nearby_friends, false, 2, kRefNpc);

    // Up to 3 named objects within 30 yards (a mailbox, a campfire, a bench, an anvil): "go stand by the mailbox". One per name.
    std::list<GameObject*> objects;
    Trinity::GameObjectInRangeCheck check(p->GetPositionX(), p->GetPositionY(), p->GetPositionZ(), 30.0f);
    Trinity::GameObjectListSearcher<Trinity::GameObjectInRangeCheck> searcher(p, objects, check);
    Cell::VisitGridObjects(p, searcher, 30.0f);
    std::vector<NearItem> objs;
    std::vector<ObjectGuid> obj_guids;
    for (GameObject* go : objects)
    {
        if (!go || !go->isSpawned() || go->GetName().empty() || go->GetGoType() == GAMEOBJECT_TYPE_TRAP ||
            go->GetGoType() == GAMEOBJECT_TYPE_TRANSPORT || go->GetGoType() == GAMEOBJECT_TYPE_MAP_OBJ_TRANSPORT ||
            go->GetOwnerGUID() == p->GetGUID())
            continue;
        float const d = p->GetDistance(go);
        auto same = std::find_if(objs.begin(), objs.end(), [&](NearItem const& o) { return o.name == go->GetName(); });
        if (same != objs.end())
        {
            if (d < same->dist)
            {
                same->dist = d;
                obj_guids[std::size_t(same - objs.begin())] = go->GetGUID();
            }
            continue;
        }
        NearItem n;
        n.kind = "object";
        n.name = go->GetName();
        n.dist = d;
        objs.push_back(std::move(n));
        obj_guids.push_back(go->GetGUID());
    }
    take(objs, obj_guids, 3, kRefObject);
    return BuildNearJson(items);
}

void WriteActionResult(BotId bot, std::string const& bot_name, uint64 reply_to, Act act, std::string const& why, std::string const& detail)
{
    Event e;
    e.type = "action_result";
    if (Player* p = FindBot(bot))
        FillBot(e, p, 0);
    else
        e.bot = bot_name;
    e.fields.emplace_back("act", ActName(act));
    if (!why.empty())
        e.fields.emplace_back("why", why);
    if (!detail.empty())
        e.detail = detail;
    e.numbers.emplace_back("ok", why.empty() ? 1 : 0);
    if (reply_to)
        e.numbers.emplace_back("reply_to", int64(reply_to));
    Write(e);
}

// Why `bot` must not attack `t` (empty: it may). Only creatures that are fair game: never a player or a player's pet, never a
// friend, never a neutral NPC that offers something (vendor, trainer, quest giver) or a civilian, never a mob another player has
// already tagged, nothing beyond Actions.NearYards or more than 5 levels above the bot.
std::string AttackRefusal(Player* bot, Unit* t)
{
    float const reach = float(Services::Config().bridge().actions_near_yards);
    if (!t || !t->IsInWorld() || t->GetMap() != bot->GetMap())
        return "that target is gone";
    if (t->GetTypeId() == TYPEID_PLAYER || t->GetCharmerOrOwnerPlayerOrPlayerItself())
        return "I never attack a player or a player's pet";
    Creature* c = t->ToCreature();
    if (!c)
        return "that is not a creature";
    if (!t->IsAlive())
        return "it is already dead";
    if (bot->GetDistance(t) > reach)
        return fmt::format("it is more than {:.0f} yards away", reach);
    if (bot->IsFriendlyTo(t))
        return "it is friendly";
    if (c->IsCivilian() || (!bot->IsHostileTo(t) && c->GetNpcFlags() != 0))
        return "I won't attack a peaceful NPC";
    if (c->hasLootRecipient() && !c->isTappedBy(bot))
        return "someone else is already fighting it";
    if (t->GetLevel() > bot->GetLevel() + 5)
        return "it is too high level for me";
    if (!bot->IsValidAttackTarget(t))
        return "I can't attack that";
    return {};
}

std::string JoinLines(std::vector<std::string> const& lines)
{
    std::string out;
    for (std::string const& l : lines)
        out += (out.empty() ? "" : " ") + l;
    return out.size() > 300 ? out.substr(0, 300) : out;
}

// The word for a zone's weather.
char const* WeatherWord(WeatherState w)
{
    switch (w)
    {
        case WEATHER_STATE_FOG:              return "foggy";
        case WEATHER_STATE_DRIZZLE:          return "drizzle";
        case WEATHER_STATE_LIGHT_RAIN:       return "light rain";
        case WEATHER_STATE_MEDIUM_RAIN:      return "rain";
        case WEATHER_STATE_HEAVY_RAIN:       return "heavy rain";
        case WEATHER_STATE_LIGHT_SNOW:       return "light snow";
        case WEATHER_STATE_MEDIUM_SNOW:      return "snow";
        case WEATHER_STATE_HEAVY_SNOW:       return "heavy snow";
        case WEATHER_STATE_LIGHT_SANDSTORM:  return "light sandstorm";
        case WEATHER_STATE_MEDIUM_SANDSTORM: return "sandstorm";
        case WEATHER_STATE_HEAVY_SANDSTORM:  return "heavy sandstorm";
        case WEATHER_STATE_THUNDERS:         return "thunderstorm";
        case WEATHER_STATE_BLACKRAIN:        return "black rain";
        case WEATHER_STATE_BLACKSNOW:        return "black snow";
        default:                             return "clear";
    }
}

// Morning / afternoon / evening / night on the server's clock (the game's day follows it).
char const* TimeOfDayWord()
{
    std::time_t const t = GameTime::GetGameTime();
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &t);
#else
    localtime_r(&t, &local);
#endif
    int const h = local.tm_hour;
    return h < 5 ? "night" : h < 12 ? "morning" : h < 18 ? "afternoon" : h < 22 ? "evening" : "night";
}

// ---- spells a player may ask for ("show me Fireball", "heal me", "buff me") ----

struct KnownSpell { SpellItem item; uint32 id = 0; };

// What a spell does for a player who asks, or nullptr when it is not something a reply may cast. Only plain damage on one target,
// heals, buffs and resurrection; never anything that moves or summons people (teleports, portals, summons, hearthstone), takes
// control of a mind, makes or opens things, kills outright, learns, tames, duels, mounts, or has a cooldown over 10 minutes.
// The one exception is Basic Campfire (Cooking), kind "campfire", cast where the bot stands.
constexpr uint32 kSpellBasicCampfire = 818;   // Cooking: "make a campfire"

char const* SpellKind(SpellInfo const* info)
{
    if (info && info->Id == kSpellBasicCampfire)
        return "campfire";
    if (!info || info->IsPassive() || info->HasAttribute(SPELL_ATTR0_DO_NOT_DISPLAY_SPELLBOOK_AURA_ICON_COMBAT_LOG) || info->IsProfession())
        return nullptr;
    for (SpellEffects e : { SPELL_EFFECT_TELEPORT_UNITS, SPELL_EFFECT_SUMMON_PLAYER, SPELL_EFFECT_TELEPORT_TO_RETURN_POINT, SPELL_EFFECT_BIND,
                            SPELL_EFFECT_TRADE_SKILL, SPELL_EFFECT_CREATE_ITEM, SPELL_EFFECT_OPEN_LOCK, SPELL_EFFECT_DUEL, SPELL_EFFECT_INSTAKILL,
                            SPELL_EFFECT_LEARN_SPELL, SPELL_EFFECT_SUMMON, SPELL_EFFECT_SUMMON_PET, SPELL_EFFECT_TAMECREATURE,
                            SPELL_EFFECT_SELF_RESURRECT })
        if (info->HasEffect(e))
            return nullptr;
    for (AuraType t : { SPELL_AURA_MOD_CHARM, SPELL_AURA_MOD_POSSESS, SPELL_AURA_MOUNTED, SPELL_AURA_FLY, SPELL_AURA_BIND_SIGHT })
        if (info->HasAura(t))
            return nullptr;
    if (std::max(info->RecoveryTime, info->CategoryRecoveryTime) > 600000u)
        return nullptr;
    if (info->HasEffect(SPELL_EFFECT_RESURRECT))
        return "resurrect";
    if (info->IsPositive())
    {
        if (info->HasEffect(SPELL_EFFECT_HEAL) || info->HasAura(SPELL_AURA_PERIODIC_HEAL))
            return "heal";
        if (info->HasEffect(SPELL_EFFECT_APPLY_AURA))
            return info->NeedsExplicitUnitTarget() ? "buff" : "self";
        return nullptr;
    }
    bool const hurts = info->HasEffect(SPELL_EFFECT_SCHOOL_DAMAGE) || info->HasEffect(SPELL_EFFECT_WEAPON_DAMAGE) ||
                       info->HasEffect(SPELL_EFFECT_NORMALIZED_WEAPON_DMG) || info->HasEffect(SPELL_EFFECT_WEAPON_PERCENT_DAMAGE) ||
                       info->HasEffect(SPELL_EFFECT_WEAPON_DAMAGE_NOSCHOOL) || info->HasAura(SPELL_AURA_PERIODIC_DAMAGE);
    return hurts && info->NeedsExplicitUnitTarget() ? "damage" : nullptr;
}

// The spells of `bot` a reply may cast: in its spell book, the highest rank it knows of each, one per name, sorted by kind and name,
// at most 30.
std::vector<KnownSpell> UsableSpells(Player* bot)
{
    std::vector<KnownSpell> out;
    std::unordered_set<std::string> seen;
    LocaleConstant const loc = sWorld->GetDefaultDbcLocale();
    for (auto const& [spell_id, ps] : bot->GetSpellMap())
    {
        if (ps.state == PLAYERSPELL_REMOVED || ps.disabled || !ps.active)
            continue;
        SpellInfo const* info = sSpellMgr->GetSpellInfo(spell_id, DIFFICULTY_NONE);
        char const* kind = SpellKind(info);
        if (!kind)
            continue;
        if (info->IsRanked())
            if (SpellInfo const* next = info->GetNextRankSpell(); next && bot->HasSpell(next->Id))
                continue;
        std::string const name = info->SpellName ? std::string(info->SpellName->Str[loc]) : std::string();
        std::string const key = SpellNameKey(name);
        if (key.empty() || !seen.insert(key).second)
            continue;
        out.push_back(KnownSpell{ SpellItem{ name, kind }, spell_id });
    }
    std::sort(out.begin(), out.end(), [](KnownSpell const& a, KnownSpell const& b)
    {
        return a.item.kind != b.item.kind ? a.item.kind < b.item.kind : a.item.name < b.item.name;
    });
    if (out.size() > 30)
        out.resize(30);
    return out;
}

// True when `target` has `info` or another rank of it.
bool HasSpellAura(Unit const* target, SpellInfo const* info)
{
    for (SpellInfo const* r = info->GetFirstRankSpell(); r; r = r->GetNextRankSpell())
        if (target->HasAura(r->Id))
            return true;
    return target->HasAura(info->Id);
}

// The game-side facts CheckActionContext needs for `bot` and the player `speaker` (nullptr: the program acts on its own).
ActionContext MakeContext(ConfigReader::BridgeConfig const& cfg, Player* bot, Player* speaker)
{
    BotId const bot_id = bot->GetGUID().GetCounter();
    ActionContext ctx;
    ctx.owned = Services::Owners().GetOwner(bot_id).account_id != 0;
    ctx.has_speaker = speaker != nullptr;
    ctx.speaker_is_owner = ctx.owned && OwnsBot(speaker, bot_id);
    ctx.speaker_controls = speaker && BotCommandParser::ControlsForGroupOrder(speaker, bot);
    if (Group const* g = bot->GetGroup())
        for (GroupReference const& itr : g->GetMembers())
            if (Player const* m = itr.GetSource(); m && m != bot && Scenario::PlaysAsHuman(m))
                ctx.grouped_with_human = true;
    ctx.in_instance = bot->GetMap() && bot->GetMap()->Instanceable();
    ctx.in_combat = bot->IsInCombat();
    ctx.owned_bots_allowed = cfg.actions_owned_bots;
    return ctx;
}

// Once a world update: a damage spell waiting for its bot to come in range and sight of the target goes out, at most 15 s after
// it was asked for.
void RunPendingCasts(uint32 now_ms)
{
    for (auto it = g_pending_cast.begin(); it != g_pending_cast.end();)
    {
        PendingCast const& pc = it->second;
        Player* bot = FindBot(it->first);
        Unit* target = bot && bot->IsInWorld() ? ObjectAccessor::GetUnit(*bot, pc.target) : nullptr;
        SpellInfo const* info = sSpellMgr->GetSpellInfo(pc.spell_id, DIFFICULTY_NONE);
        if (!bot || !bot->IsAlive() || !target || !target->IsAlive() || !info || target->GetMap() != bot->GetMap())
        {
            it = g_pending_cast.erase(it);
            continue;
        }
        if (int32(now_ms - pc.until_ms) >= 0)
        {
            TC_LOG_INFO("playerbot.v2", "[bridge] cast {} for {} given up: never got in range of {}", pc.spell_id, bot->GetName(), target->GetName());
            it = g_pending_cast.erase(it);
            continue;
        }
        float const range = std::max(info->GetMaxRange(false, bot), NOMINAL_MELEE_RANGE);
        if (bot->GetDistance(target) <= range && bot->IsWithinLOSInMap(target) && !bot->IsNonMeleeSpellCast(false))
        {
            PushIntent(it->first, CastSpellIntent{ pc.spell_id, pc.target });
            it = g_pending_cast.erase(it);
            continue;
        }
        ++it;
    }
}

// True when `text` (lower-cased here) holds every word of `words` ("goldshire", "smith argus").
bool NameHasWords(std::string text, std::string const& words)
{
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    std::size_t at = 0;
    while (at < words.size())
    {
        std::size_t const end = std::min(words.find(' ', at), words.size());
        if (end > at && text.find(words.substr(at, end - at)) == std::string::npos)
            return false;
        at = end + 1;
    }
    return true;
}

// The nearest place a go_to "place" names (ParsePlace) within `radius` yards of the bot: an NPC by its role (innkeeper, flight
// master, ...), a mailbox, or an NPC, object or area of the zone by its name. Words left after the role are the area or the
// NPC's name ("the inn at goldshire"). Empty when found (`pos` and `name` set), else why not.
std::string FindPlace(Player* bot, std::string const& place, float radius, Position& pos, std::string& name)
{
    std::string rest;
    PlaceKind const kind = ParsePlace(place, rest);
    if (kind == PlaceKind::Name && rest.empty())
        return "which place?";
    LocaleConstant const locale = sWorld->GetDefaultDbcLocale();
    auto in_area = [&](WorldObject const* wo)
    {
        uint32 area = bot->GetMap()->GetAreaId(bot->GetPhaseShift(), wo->GetPosition());
        for (int depth = 0; area && depth < 4; ++depth)
        {
            AreaTableEntry const* e = sAreaTableStore.LookupEntry(area);
            if (!e)
                break;
            if (NameHasWords(e->AreaName[locale], rest) || NameHasWords(e->AreaName[LOCALE_enUS], rest))
                return true;
            area = e->ParentAreaID;
        }
        return false;
    };
    // The words left must be its name or its area ("the inn at goldshire"); none left: any of that kind.
    auto fits = [&](WorldObject const* wo) { return rest.empty() || NameHasWords(wo->GetName(), rest) || in_area(wo); };

    WorldObject const* best = nullptr;
    float best_d = radius + 1.0f;
    auto consider = [&](WorldObject const* wo)
    {
        float const d = bot->GetDistance(wo);
        if (d < best_d)
        {
            best = wo;
            best_d = d;
        }
    };
    if (kind == PlaceKind::Mailbox || kind == PlaceKind::Name)
    {
        std::vector<GameObject*> objects;
        bot->GetGameObjectListWithOptionsInGrid(objects, radius, FindGameObjectOptions());
        for (GameObject const* go : objects)
        {
            if (!go->isSpawned())
                continue;
            if (kind == PlaceKind::Mailbox ? go->GetGoType() == GAMEOBJECT_TYPE_MAILBOX && fits(go) : NameHasWords(go->GetName(), rest))
                consider(go);
        }
    }
    if (kind != PlaceKind::Mailbox)
    {
        NPCFlags flag = UNIT_NPC_FLAG_NONE;
        switch (kind)
        {
            case PlaceKind::Innkeeper:    flag = UNIT_NPC_FLAG_INNKEEPER; break;
            case PlaceKind::FlightMaster: flag = UNIT_NPC_FLAG_FLIGHTMASTER; break;
            case PlaceKind::Vendor:       flag = UNIT_NPC_FLAG_VENDOR_MASK; break;
            case PlaceKind::Repair:       flag = UNIT_NPC_FLAG_REPAIR; break;
            case PlaceKind::Banker:       flag = UNIT_NPC_FLAG_BANKER; break;
            case PlaceKind::Trainer:      flag = UNIT_NPC_FLAG_TRAINER; break;
            case PlaceKind::StableMaster: flag = UNIT_NPC_FLAG_STABLEMASTER; break;
            case PlaceKind::Auctioneer:   flag = UNIT_NPC_FLAG_AUCTIONEER; break;
            case PlaceKind::SpiritHealer: flag = UNIT_NPC_FLAG_SPIRIT_HEALER; break;
            default: break;
        }
        std::vector<Creature*> creatures;
        FindCreatureOptions opt;
        opt.IsAlive = FindCreatureAliveState::Alive;
        bot->GetCreatureListWithOptionsInGrid(creatures, radius, opt);
        // By name: the NPC itself first; an area by name goes to the nearest friendly NPC with a role there (a town's people).
        WorldObject const* by_area = nullptr;
        float by_area_d = radius + 1.0f;
        for (Creature const* c : creatures)
        {
            if (c->IsHostileTo(bot) || c->IsPet() || c->IsTotem())
                continue;
            if (kind != PlaceKind::Name)
            {
                if (c->HasNpcFlag(flag) && fits(c))
                    consider(c);
                continue;
            }
            if (NameHasWords(c->GetName(), rest))
                consider(c);
            else if (c->GetNpcFlags() != UNIT_NPC_FLAG_NONE && in_area(c))
            {
                float const d = bot->GetDistance(c);
                if (d < by_area_d)
                {
                    by_area = c;
                    by_area_d = d;
                }
            }
        }
        if (!best && by_area)
        {
            best = by_area;
            best_d = by_area_d;
        }
    }
    if (!best)
        return fmt::format("I don't know a {} within {:.0f} yards", place.substr(0, 40), radius);
    pos = best->GetPosition();
    name = best->GetName();
    return {};
}

// "trade me some stuff": the bot opens a trade window with the player who asked (its owner, or the player leading its group:
// CheckActionContext) and puts in what he asked for, or what helps him (TradeShop::PickGifts). The game's own trade does the rest:
// nothing changes hands until he presses Accept, and the bot accepts once he has (API::trade_with, BotTrade::Decide).
// `speaks_later`: the bot tells him itself how it goes (the conjure order), so the program's line is not said.
std::string RunTrade(Player* bot, Player* speaker, std::string const& items, std::string& detail, bool& speaks_later)
{
    if (!speaker)
        return "nobody asked";
    // A window already open between the two of them (he opened it, or an earlier ask did): the things go into that one. Before,
    // every later ask was refused with "I'm already trading" while the first window sat empty (Karrz, 2026-11-08).
    TradeData* const open = bot->GetTradeData();
    bool const same_window = open && open->GetTrader() == speaker;
    if (open && !same_window)
        return "I'm already trading with someone else";
    if (!same_window && speaker->GetTradeData())
        return "you're already trading with someone";
    if (speaker->GetMap() != bot->GetMap() || !bot->IsWithinDistInMap(speaker, TRADE_DISTANCE - 1.0f))
        return "come closer so we can trade";
    std::vector<Item*> const gifts = TradeShop::PickGifts(bot, speaker, items);
    if (gifts.empty())
    {
        // No water in the bags, but a mage makes it: conjure first, the craft order opens the trade when it is done.
        std::string ask = items;
        std::transform(ask.begin(), ask.end(), ask.begin(), [](unsigned char c) { return char(std::tolower(c)); });
        char const* make = ask.find("water") != std::string::npos || ask.find("drink") != std::string::npos ? "water"
                         : ask.find("food") != std::string::npos || ask.find("bread") != std::string::npos ? "food" : nullptr;
        if (make && bot->GetClass() == CLASS_MAGE)
        {
            ::Playerbot::V2::PlayerCraftOrders::Order(speaker, bot, std::string("craft ") + make + " 20");
            speaks_later = true;   // the order whispers what it makes (or why it can't), and trades when done
            detail = std::string("no ") + make + " in my bags: conjuring some for " + speaker->GetName() + " (a craft order; it trades when done)";
            return {};
        }
        return items.empty() ? "I've nothing in my bags you could use" : "I don't have any " + items;
    }
    std::vector<ObjectGuid> guids;
    std::string names;
    for (Item* it : gifts)
    {
        guids.push_back(it->GetGUID());
        if (!names.empty())
            names += ", ";
        names += it->GetTemplate()->GetName(DEFAULT_LOCALE);
        if (it->GetCount() > 1)
            names += " x" + std::to_string(it->GetCount());
    }
    TradeShop::StoreGift(bot, speaker->GetGUID(), std::move(guids));
    if (same_window)
    {
        detail = "putting " + names + " into the open trade with " + speaker->GetName();
        return {};
    }
    // The bot proposes the trade as a player would (CMSG_INITIATE_TRADE); the handler makes every check (distance, faction, level,
    // logging out, already trading). The bot's trade code then begins the window and fills it.
    WorldPacket packet(CMSG_INITIATE_TRADE);
    WorldPackets::Trade::InitiateTrade init(std::move(packet));
    init.Guid = speaker->GetGUID();
    bot->GetSession()->HandleInitiateTradeOpcode(init);
    if (!bot->GetTradeData())
        return "the trade did not open";
    detail = "trading " + names + " to " + speaker->GetName();
    return {};
}

// Runs one checked action. Returns why it could not run (empty: done; `detail` then says what was done).
// When the asking player may already order this bot (its owner, or the player leading its group), the action goes through the
// same order code as his own whisper (BotCommandParser), so it behaves exactly like that order. Otherwise (a world bot on its own,
// or the program acting by itself) the same intents are queued directly, with shorter holds.
Petition* CharterOf(Player* player);

std::string RunAction(ConfigReader::BridgeConfig const& cfg, Player* bot, Player* speaker, Action const& a, uint32 now_ms, std::string& detail,
                      bool& speaks_later)
{
    BotId const id = bot->GetGUID().GetCounter();
    BotAI* ai = Services::Registry().ai(id);
    if (!ai)
        return "I'm not ready yet";
    bool const controls = speaker && BotCommandParser::ControlsForGroupOrder(speaker, bot);
    auto order = [&](char const* text) -> std::string
    {
        return BotCommandParser::DispatchSingle(speaker, bot, text, "bridge") ? std::string() : std::string("the order was not taken");
    };
    auto person = [&](std::string const& t) -> Player*
    {
        if (t.empty() || t == "speaker")
            return speaker;
        if (OwnsBot(speaker, id))
            return speaker;
        OwnerBinding const ob = Services::Owners().GetOwner(id);
        if (ob.account_id && ob.player_guid)
            return ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(ob.player_guid));
        return nullptr;
    };
    auto person_missing = [&](std::string const& t) -> std::string
    {
        return t == "owner" ? "my owner is not online" : "nobody asked: use \"target\":\"owner\" or answer a chat_in";
    };
    auto close_to = [&](Player* who, float max_yd) -> std::string
    {
        if (who == bot)
            return "that is me";
        if (who->GetMap() != bot->GetMap() || !who->IsInWorld())
            return "they are not where I am";
        if (bot->GetDistance(who) > max_yd)
            return fmt::format("they are more than {:.0f} yards away", max_yd);
        return {};
    };
    auto ref_object = [&](uint8& kind) -> WorldObject*
    {
        auto const* e = g_refs.Find(id, a.ref, now_ms);
        if (!e)
            return nullptr;
        kind = e->kind;
        WorldObject* wo = ObjectAccessor::GetWorldObject(*bot, e->id);
        return wo && wo->IsInMap(bot) ? wo : nullptr;
    };
    uint32 const follow_ms = cfg.actions_follow_minutes * 60000u;
    bool const moves = a.act == Act::Follow || a.act == Act::Flee || a.act == Act::GoTo || a.act == Act::Attack ||
                       a.act == Act::Mount || a.act == Act::Unstuck || a.act == Act::AcceptQuest || a.act == Act::TurnIn ||
                       a.act == Act::Cast || a.act == Act::Trade;
    if (moves && (bot->IsInFlight() || bot->GetTransport()))
        return "I'm on a flight or a ship right now";

    switch (a.act)
    {
        case Act::Follow:
        {
            Player* who = person(a.target);
            if (!who)
                return person_missing(a.target);
            if (std::string w = close_to(who, 50.0f); !w.empty())
                return w;
            g_leash.erase(id);
            detail = "following " + who->GetName();
            if (controls && who == speaker)
                return order("follow");
            PushIntent(id, FollowIntent{ who->GetGUID(), 3.0f, 3.14159265f });
            ai->enter_manual(BotAI::OwnerCommand::Follow, who->GetGUID(), now_ms, follow_ms);
            ai->set_last_owner_name(who->GetName());
            if (!OwnsBot(who, id))
                g_leash[id] = Leash{ who->GetGUID(), now_ms + follow_ms };
            return {};
        }
        case Act::Stay:
            g_leash.erase(id);
            detail = "staying here";
            if (controls)
                return order("stay");
            PushIntent(id, StopMovementIntent{ true });
            ai->enter_manual(BotAI::OwnerCommand::Stay, ObjectGuid::Empty, now_ms, follow_ms);
            ai->set_owner_follow_order(false);
            return {};
        case Act::Resume:
            g_leash.erase(id);
            detail = "back to my own plans";
            if (controls)
                return order("resume");
            ai->exit_manual();
            ai->release_owner_lead(false);
            return {};
        case Act::Attack:
        {
            Unit* target = nullptr;
            if (a.ref)
            {
                uint8 kind = 0;
                WorldObject* wo = ref_object(kind);
                if (!wo)
                    return "that ref is unknown, gone or older than 60 seconds";
                if (kind != kRefHostile)
                    return "that ref is not a hostile";
                target = wo->ToUnit();
            }
            else
            {
                if (!speaker)
                    return "attack needs a \"ref\", or a player's selected target";
                target = speaker->GetSelectedUnit();
                if (!target)
                    return "the player has nothing selected";
            }
            if (std::string w = AttackRefusal(bot, target); !w.empty())
                return w;
            detail = "attacking " + target->GetName();
            if (controls && !a.ref)
                return order("attack");
            PushIntent(id, StartAttackIntent{ target->GetGUID() });
            ai->enter_manual(BotAI::OwnerCommand::Engage, target->GetGUID(), now_ms);
            return {};
        }
        case Act::StopAttack:
            detail = "stopped fighting";
            if (controls)
                return order("stop");
            PushIntent(id, StopAttackIntent{});
            PushIntent(id, PetSetCommandStateIntent{ 1 });
            ai->close_owner_order_window();
            if (ai->owner_command() == BotAI::OwnerCommand::Engage)
                ai->exit_manual();
            return {};
        case Act::Flee:
        {
            Player* who = person(a.target);
            if (!who)
                return person_missing(a.target);
            if (std::string w = close_to(who, 50.0f); !w.empty())
                return w;
            detail = "running to " + who->GetName();
            if (controls && who == speaker)
                return order("flee");
            PushIntent(id, StopAttackIntent{});
            PushIntent(id, CancelCastIntent{});
            PushIntent(id, FollowIntent{ who->GetGUID(), 2.0f, 0.0f });
            ai->start_flee(who->GetGUID(), now_ms);
            ai->enter_manual(BotAI::OwnerCommand::Follow, who->GetGUID(), now_ms);
            return {};
        }
        case Act::GoTo:
        {
            float x = 0, y = 0, z = 0;
            if (a.ref)
            {
                uint8 kind = 0;
                WorldObject* wo = ref_object(kind);
                if (!wo)
                    return "that ref is unknown, gone or older than 60 seconds";
                if (kind == kRefHostile)
                    return "that ref is a hostile (use attack)";
                wo->GetPosition(x, y, z);
                detail = "walking to " + wo->GetName();
            }
            else if (a.has_point)
            {
                x = a.x;
                y = a.y;
                z = a.z;
                if (!a.has_z)
                {
                    z = bot->GetMap()->GetHeight(bot->GetPhaseShift(), x, y, bot->GetPositionZ() + 10.0f, true, 50.0f);
                    if (z <= INVALID_HEIGHT)
                        return "there is no ground there";
                }
                detail = fmt::format("walking to ({:.0f}, {:.0f})", x, y);
            }
            else if (!a.place.empty())
            {
                Position at;
                std::string where;
                if (std::string w = FindPlace(bot, a.place, float(cfg.actions_max_goto_yards), at, where); !w.empty())
                    return w;
                at.GetPosition(x, y, z);
                detail = "walking to " + where;
            }
            else
            {
                Player* who = person(a.target);
                if (!who)
                    return person_missing(a.target);
                if (who->GetMap() != bot->GetMap())
                    return "they are not where I am";
                who->GetPosition(x, y, z);
                detail = "walking to " + who->GetName();
            }
            if (bot->GetExactDist2d(x, y) > float(cfg.actions_max_goto_yards))
                return fmt::format("that is more than {} yards away (walking only, no travel)", cfg.actions_max_goto_yards);
            PathGenerator path(bot);
            path.CalculatePath(x, y, z, false);
            if (path.GetPathType() & PATHFIND_NOPATH)
                return "I can't find a way to walk there";
            g_leash.erase(id);
            PushIntent(id, MoveToIntent{ x, y, z, true });
            ai->enter_manual(BotAI::OwnerCommand::Stay, ObjectGuid::Empty, now_ms, 180000);
            ai->set_owner_follow_order(false);
            if (speaker)
                ai->set_last_owner_name(speaker->GetName());
            return {};
        }
        case Act::AcceptQuest:
        {
            auto snap = Services::Snapshots().latest(id);
            bool offered = false;
            if (snap)
                for (auto const& off : snap->quest_discovery.quest_offers)
                    offered = offered || off.quest_id == a.quest;
            if (!offered)
                return "nobody close offers me that quest";
            detail = JoinLines(QuestOrders::AcceptAll(speaker, bot, std::to_string(a.quest)));
            return {};
        }
        case Act::TurnIn:
        {
            std::vector<std::string> const lines = QuestOrders::TurnIn(speaker, bot, a.quest ? std::to_string(a.quest) : std::string());
            detail = JoinLines(lines);
            if (!lines.empty() && lines.front().rfind("I can't", 0) == 0)
                return lines.front();
            return {};
        }
        case Act::DropQuest:
            if (bot->GetQuestStatus(a.quest) == QUEST_STATUS_NONE)
                return "that quest is not in my log";
            detail = JoinLines(QuestOrders::Drop(speaker, bot, std::to_string(a.quest)));
            return {};
        case Act::ShareQuest:
            if (!bot->GetGroup())
                return "I'm not in a group";
            detail = JoinLines(QuestOrders::Share(speaker, bot, a.quest ? std::to_string(a.quest) : std::string()));
            return {};
        case Act::Unstuck:
            PushIntent(id, UnstuckIntent{ 5.0f });
            detail = "nudged 5 yards";
            return {};
        case Act::Sit:
            PushIntent(id, SetStandStateIntent{ 1 });
            detail = "sitting";
            return {};
        case Act::Stand:
            PushIntent(id, SetStandStateIntent{ 0 });
            detail = "standing";
            return {};
        case Act::Mount:
            if (bot->IsMounted())
                return "I'm already mounted";
            if (!bot->IsOutdoors())
                return "I can't mount indoors";
            PushIntent(id, MountIntent{ 0 });
            detail = "mounting";
            return {};
        case Act::Dismount:
            if (!bot->IsMounted())
                return "I'm not mounted";
            PushIntent(id, DismountIntent{});
            detail = "dismounting";
            return {};
        case Act::Cast:
        {
            if (!cfg.actions_spells)
                return "spells are turned off on this server (Playerbot.Bridge.Actions.Spells)";
            std::vector<KnownSpell> const spells = UsableSpells(bot);
            std::string const key = SpellNameKey(a.spell);
            auto known = std::find_if(spells.begin(), spells.end(), [&](KnownSpell const& k) { return SpellNameKey(k.item.name) == key; });
            if (known == spells.end())
                return "I don't have a spell called " + a.spell.substr(0, 40);
            SpellInfo const* info = sSpellMgr->GetSpellInfo(known->id, DIFFICULTY_NONE);
            if (!info)
                return "I don't have that spell";
            std::string const& name = known->item.name;
            std::string const& kind = known->item.kind;
            if (!bot->GetSpellHistory()->IsReady(info))
                return name + " is on cooldown";
            if (kind == "damage")
            {
                Unit* target = nullptr;
                if (a.ref)
                {
                    uint8 rkind = 0;
                    WorldObject* wo = ref_object(rkind);
                    if (!wo)
                        return "that ref is unknown, gone or older than 60 seconds";
                    if (rkind != kRefHostile)
                        return "that ref is not a hostile";
                    target = wo->ToUnit();
                }
                else
                {
                    if (!speaker)
                        return "a damage spell needs a \"ref\", or a player's selected target";
                    target = speaker->GetSelectedUnit();
                    if (!target)
                        return "the player has nothing selected";
                }
                if (std::string w = AttackRefusal(bot, target); !w.empty())
                    return w;
                // The bot goes at the target like an attack order, and the spell goes out once it is in range and sight.
                PushIntent(id, StartAttackIntent{ target->GetGUID() });
                ai->enter_manual(BotAI::OwnerCommand::Engage, target->GetGUID(), now_ms);
                g_pending_cast[id] = PendingCast{ known->id, target->GetGUID(), now_ms + 15000 };
                detail = "casting " + name + " at " + target->GetName();
                return {};
            }
            if (kind == "campfire")
            {
                if (!bot->IsOutdoors())
                    return "can't make a fire indoors";
                PushIntent(id, CastSpellIntent{ known->id, ObjectGuid::Empty });
                detail = "making a campfire";
                return {};
            }
            Unit* target = bot;
            if (kind != "self" && a.target != "self")
            {
                if (a.target == "speaker_target")
                {
                    if (!speaker)
                        return "nobody asked: use \"target\":\"speaker\" or answer a chat_in";
                    target = speaker->GetSelectedUnit();
                    if (!target)
                        return "the player has nothing selected";
                }
                else
                {
                    Player* who = person(a.target);
                    if (!who)
                        return person_missing(a.target);
                    target = who;
                }
            }
            if (target != bot)
            {
                if (!target->ToPlayer())
                    return "I only do that for players";
                if (!bot->IsFriendlyTo(target))
                    return "I won't help an enemy";
                if (target->GetMap() != bot->GetMap() || !target->IsInWorld())
                    return "they are not where I am";
                float const range = info->GetMaxRange(true, bot);
                if (bot->GetDistance(target) > range)
                    return fmt::format("too far away for {}: come within {:.0f} yards", name, range);
                if (!bot->IsWithinLOSInMap(target))
                    return "I can't see them from here";
            }
            if (kind == "resurrect")
            {
                if (target->IsAlive())
                    return "they are not dead";
            }
            else if (!target->IsAlive())
                return "they are dead (a resurrect would help)";
            else if (kind == "heal" && target->IsFullHealth())
                return "they are not hurt";
            else if ((kind == "buff" || kind == "self") && HasSpellAura(target, info))
                return "they already have " + name;
            PushIntent(id, CastSpellIntent{ known->id, target->GetGUID() });
            detail = "casting " + name + (target == bot ? std::string(" on myself") : " on " + target->GetName());
            return {};
        }
        case Act::Emote:
            PushIntent(id, PerformEmoteIntent{ EmoteId(a.emote), speaker ? speaker->GetGUID() : ObjectGuid::Empty });
            detail = a.emote;
            return {};
        case Act::Order:
            // Checked by SafeOrder and CheckActionContext: the asking player owns the bot or leads its group, so the order runs
            // exactly as if he had typed it (the parser answers him as for his own whisper).
            if (!speaker)
                return "nobody asked";
            detail = "order \"" + a.order + "\"";
            return order(a.order.c_str());
        case Act::Trade:
            return RunTrade(bot, speaker, a.items, detail, speaks_later);
        case Act::SignCharter:
        {
            // As his "Request Signature": the same checks and the same result in his charter window (BotSignOfferedCharter).
            if (!speaker)
                return "nobody asked";
            Petition* p = CharterOf(speaker);
            if (!p)
                return "you don't carry a guild charter";
            if (std::string w = close_to(speaker, 30.0f); !w.empty())
                return w;
            switch (V2::BotSignOfferedCharter(bot, p->PetitionGuid))
            {
                case V2::CharterSignResult::Ok:             detail = "signed the charter for " + p->PetitionName; return {};
                case V2::CharterSignResult::AlreadySigned:  return "I signed it already";
                case V2::CharterSignResult::SignerInGuild:  return "I'm in a guild already";
                case V2::CharterSignResult::SignaturesFull: return "it is full";
                case V2::CharterSignResult::WrongFaction:   return "we are of different factions";
                case V2::CharterSignResult::SignerBusy:     return "I'm getting my own charter signed";
                case V2::CharterSignResult::OwnerCantSign:  return "that is my own charter";
                default:                                return "that charter is gone";
            }
        }
        case Act::Step:
        {
            // A few yards from where the bot stands. back / forward / left / right are as the asking player faces (what he sees on
            // his screen: "move left" is his left), away / closer are from him. The spot is the first one free of walls on that
            // line, on the ground, and the bot must be able to walk there.
            if (!speaker)
                return "nobody asked";
            if (speaker->GetMap() != bot->GetMap() || bot->GetDistance(speaker) > 60.0f)
                return "you are not near me";
            float const face = speaker->GetOrientation();
            float angle = face;
            if (a.dir == "back")
                angle = face + float(M_PI);
            else if (a.dir == "left")
                angle = face + float(M_PI) / 2.0f;
            else if (a.dir == "right")
                angle = face - float(M_PI) / 2.0f;
            else if (a.dir == "away" || a.dir == "closer")
            {
                if (bot->GetExactDist2d(speaker) < 0.5f)
                    angle = face + float(M_PI);
                else
                    angle = speaker->GetAbsoluteAngle(bot) + (a.dir == "closer" ? float(M_PI) : 0.0f);
            }
            float yards = a.yards;
            if (a.dir == "closer")
                yards = std::min(yards, std::max(0.0f, bot->GetExactDist2d(speaker) - 2.0f));
            if (yards < 1.0f)
                return "I'm already right next to you";
            Position at = bot->GetPosition();
            bot->MovePositionToFirstCollision(at, yards, Position::NormalizeOrientation(angle) - bot->GetOrientation());
            if (bot->GetExactDist2d(at.GetPositionX(), at.GetPositionY()) < 1.0f)
                return "there is no room that way";
            PathGenerator path(bot);
            path.CalculatePath(at.GetPositionX(), at.GetPositionY(), at.GetPositionZ(), false);
            if (path.GetPathType() & (PATHFIND_NOPATH | PATHFIND_INCOMPLETE))
                return "I can't walk that way";
            g_leash.erase(id);
            PushIntent(id, MoveToIntent{ at.GetPositionX(), at.GetPositionY(), at.GetPositionZ(), true });
            // Out of a fight the bot waits on its new spot like after go_to; in one (the fire, a cleave) it holds there briefly and
            // then fights on as before.
            ai->enter_manual(BotAI::OwnerCommand::Stay, ObjectGuid::Empty, now_ms, bot->IsInCombat() ? 8000 : 180000);
            ai->set_owner_follow_order(false);
            ai->set_last_owner_name(speaker->GetName());
            detail = fmt::format("stepping {} {:.0f} yards", a.dir, bot->GetExactDist2d(at.GetPositionX(), at.GetPositionY()));
            return {};
        }
        default:
            return "no such action";
    }
}

struct ActionOutcome
{
    bool ok = false;
    bool speaks_later = false;   // the action tells the player itself how it goes
    std::string why;             // refused: why not
};

// One reply's action: every check, then RunAction, then an action_result event. `ask` is the chat_in it answers (nullptr: the
// program acts on its own), `read_ms` when the line was read.
ActionOutcome HandleAction(ConfigReader::BridgeConfig const& cfg, BotId bot_id, std::string const& bot_name, uint64 reply_to, ChatAsk const* ask,
                           Reply const& r, uint32 read_ms, uint32 now_ms)
{
    Action const& a = r.action;
    auto refuse = [&](std::string why)
    {
        g_act_gate.NoteRefused(bot_id, now_ms);
        TC_LOG_INFO("playerbot.v2", "[bridge] action {} for {} refused: {}", a.act == Act::None ? "?" : ActName(a.act), bot_name, why);
        WriteActionResult(bot_id, bot_name, reply_to, a.act, why, {});
        ActionOutcome out;
        out.why = std::move(why);
        return out;
    };
    if (!cfg.actions)
        return refuse("actions are turned off on this server (Playerbot.Bridge.Actions.Enable)");
    if (!r.action_error.empty())
        return refuse(r.action_error);
    RefreshAllowed(cfg);
    if (std::size_t(a.act) >= g_allowed.size() || !g_allowed[std::size_t(a.act)])
        return refuse(std::string(ActName(a.act)) + " is turned off on this server (Playerbot.Bridge.Actions.Allowed)");
    uint32 const max_age_ms = cfg.actions_max_age_sec * 1000u;
    if (now_ms - read_ms > max_age_ms || (ask && now_ms - ask->asked_ms > max_age_ms))
        return refuse("too old: the moment has passed");
    Player* bot = FindBot(bot_id);
    if (!bot || !bot->IsInWorld())
        return refuse("the bot is not online");
    if (!bot->IsAlive())
        return refuse("I'm dead");
    Player* speaker = nullptr;
    if (ask)
    {
        speaker = ObjectAccessor::FindConnectedPlayer(ask->sender_guid);
        if (!speaker)
            return refuse("the player who asked is gone");
        if (ask->sender_is_bot && a.act != Act::Emote && a.act != Act::Sit && a.act != Act::Stand)
            return refuse("only players can ask a bot to do things");
    }

    ActionContext const ctx = MakeContext(cfg, bot, speaker);
    if (char const* why = CheckActionContext(a.act, ctx))
        return refuse(why);

    g_act_gate.SetLimits(cfg.actions_bot_gap_ms, cfg.actions_per_sec);
    if (char const* why = g_act_gate.Allow(bot_id, a.act, now_ms))
        return refuse(why);

    std::string detail;
    ActionOutcome out;
    std::string const why = RunAction(cfg, bot, speaker, a, now_ms, detail, out.speaks_later);
    if (!why.empty())
        return refuse(why);
    TC_LOG_INFO("playerbot.v2", "[bridge] action {} for {}{}: {}", ActName(a.act), bot_name,
                speaker ? " (asked by " + speaker->GetName() + ")" : std::string(), detail);
    WriteActionResult(bot_id, bot_name, reply_to, a.act, {}, detail);
    out.ok = true;
    return out;
}

// Once a second: a world bot following a player it does not belong to lets go when the time is up, or when the player is gone
// or 100 yards away.
void CheckLeashes(uint32 now_ms)
{
    for (auto it = g_leash.begin(); it != g_leash.end();)
    {
        BotAI* ai = Services::Registry().ai(it->first);
        Player* b = FindBot(it->first);
        bool keep = ai && b && ai->owner_command() == BotAI::OwnerCommand::Follow && ai->owner_target() == it->second.leader;
        if (keep)
        {
            Player* l = ObjectAccessor::FindConnectedPlayer(it->second.leader);
            if (!l || l->GetMap() != b->GetMap() || b->GetDistance(l) > 100.0f || int32(now_ms - it->second.until_ms) >= 0)
            {
                ai->exit_manual();
                PushIntent(it->first, StopMovementIntent{ true });
                keep = false;
            }
        }
        it = keep ? std::next(it) : g_leash.erase(it);
    }
}

// A group order's action for the rest of the group, one bot every 300 ms (a group that all turns at once looks scripted). A ref
// is the answering bot's (its NEAR list): each bot gets its own ref for the same thing.
void QueueGroupFollowups(BotId first, ChatAsk const& ask, Reply const& r, uint32 read_ms, uint32 now_ms)
{
    ObjectGuid ref_guid;
    uint8 ref_kind = 0;
    if (r.action.ref)
        if (auto const* e = g_refs.Find(first, r.action.ref, now_ms))
        {
            ref_guid = e->id;
            ref_kind = e->kind;
        }
    uint32 due = now_ms;
    for (BotId b : ask.group)
    {
        if (b == first)
            continue;
        GroupFollowup f;
        f.bot = b;
        f.ask = ask;
        f.ask.bot = b;
        f.ask.group.clear();
        f.reply = r;
        f.reply.say.clear();
        if (r.action.ref)
            f.reply.action.ref = ref_guid.IsEmpty() ? 0 : g_refs.Add(b, ref_guid, ref_kind, now_ms);
        f.read_ms = read_ms;
        f.due_ms = (due += 300);
        g_group_followups.push_back(std::move(f));
    }
}

void HandleGroupFollowups(ConfigReader::BridgeConfig const& cfg, uint32 now_ms)
{
    for (std::size_t i = 0; i < g_group_followups.size();)
    {
        GroupFollowup& f = g_group_followups[i];
        if (int32(now_ms - f.due_ms) < 0)
        {
            ++i;
            continue;
        }
        GroupFollowup const done = std::move(f);
        g_group_followups.erase(g_group_followups.begin() + std::ptrdiff_t(i));
        Player* p = FindBot(done.bot);
        if (!p || !Services::HasIntents(done.bot))
            continue;
        HandleAction(cfg, done.bot, p->GetName(), done.reply.reply_to, &done.ask, done.reply, done.read_ms, now_ms);   // logs itself
    }
}

void HandleReplies(ConfigReader::BridgeConfig const& cfg, uint32 now_ms)
{
    HandleGroupFollowups(cfg, now_ms);
    std::erase_if(g_chat, [&](auto const& kv) { return now_ms - kv.second.asked_ms > cfg.reply_max_age_sec * 1000u; });
    for (uint32 n = 0; n < kRepliesPerTick && !g_reply_lines.empty(); ++n)
    {
        std::string const line = std::move(g_reply_lines.front().first);
        uint32 const read_ms = g_reply_lines.front().second;
        g_reply_lines.pop_front();
        Reply r;
        std::string error;
        if (!ParseReplyLine(line, r, error))
        {
            ++g_bad_lines;
            if (now_ms - g_last_bad_warn_ms > 60000 || g_last_bad_warn_ms == 0)
            {
                TC_LOG_WARN("server.worldserver", "[PlayerbotV2] Bot bridge: skipped a reply line ({}; {} bad line(s) since the last warning): {}",
                            error, g_bad_lines, line.substr(0, 200));
                g_last_bad_warn_ms = now_ms ? now_ms : 1;
                g_bad_lines = 0;
            }
            continue;
        }
        if (r.ping)
            continue;
        std::string text = CleanSayText(r.say, cfg.max_line_bytes);
        bool const has_action = r.action.act != Act::None || !r.action_error.empty();
        if (text.empty() && !has_action)
            continue;

        BotId bot = 0;
        std::string bot_name = r.bot;
        std::string channel = r.channel, to = r.to;
        ChatAsk ask;
        bool const answers = r.reply_to != 0;
        if (answers)
        {
            auto it = g_chat.find(r.reply_to);
            if (it == g_chat.end())   // unknown, already answered, or too old: the moment has passed
            {
                if (has_action && !r.bot.empty())
                    if (Player* p = ObjectAccessor::FindConnectedPlayerByName(r.bot); p && Services::Registry().has(p->GetGUID().GetCounter()))
                        WriteActionResult(p->GetGUID().GetCounter(), p->GetName(), r.reply_to, r.action.act,
                                          "that chat_in is unknown, already answered or too old", {});
                continue;
            }
            ask = it->second;
            bot = ask.bot;
            if (Player* p = FindBot(bot))
                bot_name = p->GetName();
            // A channel post goes where the server asked for it; a chat answer never goes to General or Trade.
            if (channel.empty() || ask.channel == "general" || ask.channel == "trade" || channel == "general" || channel == "trade")
                channel = ask.channel;
            if (channel == "whisper" && to.empty())
                to = ask.sender;
            g_chat.erase(it);
            if (auto pi = g_post_ask.find(r.reply_to); pi != g_post_ask.end())
            {
                PostAsk const post = std::move(pi->second);
                g_post_ask.erase(pi);
                if (!text.empty())
                    text = FinishPost(bot, post, std::move(text), now_ms);
            }
        }
        else
        {
            Player* p = ObjectAccessor::FindConnectedPlayerByName(r.bot);
            if (!p || !Services::Registry().has(p->GetGUID().GetCounter()))
                continue;   // only bots can be made to talk or act, never a real player
            bot = p->GetGUID().GetCounter();
            bot_name = p->GetName();
            if (channel.empty())
                channel = "say";
            if ((channel == "party" || channel == "raid") && !p->GetGroup())
                text.clear();
            if (channel == "general" || channel == "trade")
                text.clear();   // a bot posts in a channel only when the server asked for that post
            // Lines a bot says on its own: at most one every BotLineGapMs.
            if (auto g = g_last_line_ms.find(bot); !text.empty() && g != g_last_line_ms.end() && now_ms - g->second < cfg.bot_line_gap_ms)
                text.clear();
        }
        if (!Services::HasIntents(bot))
            continue;
        // The action runs first: the line is said only when it is true (LineAfterAction).
        if (has_action && !ask.talk_key)
        {
            ActionOutcome const done = HandleAction(cfg, bot, bot_name, r.reply_to, answers ? &ask : nullptr, r, read_ms, now_ms);
            if (answers && !ask.group.empty() && r.action_error.empty() && IsGroupAct(r.action.act))
                QueueGroupFollowups(bot, ask, r, read_ms, now_ms);
            std::string const said = text;
            text = LineAfterAction(r.action.act, done.ok, done.speaks_later, done.why, text);
            if (!answers && !done.ok)
                text.clear();   // a line of the program's own: nobody asked, nobody needs a "sorry"
            if (text != said)
                TC_LOG_INFO("playerbot.v2", "[bridge] {}'s line \"{}\" {} (action {})", bot_name, said,
                            text.empty() ? "not said" : "said as \"" + text + "\"", done.ok ? "speaks for itself" : "refused");
            if ((channel == "say" || channel == "yell") && !text.empty() && !done.ok && answers && !ask.sender.empty())
            {
                channel = "whisper";   // a "sorry" goes to the one who asked, not to the whole street
                to = ask.sender;
            }
        }
        if (!text.empty())
        {
            g_last_line_ms[bot] = now_ms;
            TC_LOG_INFO("playerbot.v2", "[bridge] bot {} -> {}{}: \"{}\"", bot, channel, to.empty() ? "" : " " + to, text);
            if (ask.talk_key && ask.talk_key != kNoTalkKey)
            {
                if (g_talk_said.size() > 256)
                    g_talk_said.clear();
                g_talk_said[ask.talk_key].emplace_back(bot_name, text);
            }
            Say(bot, channel, to, std::move(text), now_ms, r.delay_ms);
        }
    }
    if (g_last_line_ms.size() > 4096)
        std::erase_if(g_last_line_ms, [&](auto const& kv) { return now_ms - kv.second > 600000u; });
}

void WriteStatus(uint32 now_ms)
{
    std::string s = "{\"t\":\"" + IsoTime(UtcNow()) + "\",\"running\":1,\"uptime_s\":" + std::to_string((now_ms - g_started_ms) / 1000)
                  + ",\"bots\":" + std::to_string(Services::Registry().size()) + ",\"last_event\":" + std::to_string(g_next_id - 1)
                  + ",\"client_alive\":" + (g_client_alive ? "1" : "0") + ",\"dropped_lines\":" + std::to_string(g_out_dropped.load())
                  + ",\"skipped_repeats\":" + std::to_string(g_gate.TotalRepeats()) + "}";
    {
        std::lock_guard<std::mutex> lk(g_out_mtx);
        g_status = std::move(s);
        g_status_dirty = true;
    }
    g_out_cv.notify_one();
}

// The guild charter `player` carries, or null (the petition must be his and the charter item still in his bags).
Petition* CharterOf(Player* player)
{
    Petition* p = player ? sPetitionMgr->GetPetitionByOwner(player->GetGUID()) : nullptr;
    return p && player->GetItemByGuid(p->PetitionGuid) ? p : nullptr;
}

// Who the bot and the asking player are to each other, in words for the program, so a "want to join my guild?", "sign my charter?"
// or "want to quest together?" gets a true answer: "You are in the guild Iron Wolves. Karrz has no guild. Karrz carries a guild
// charter for Night Owls: 2 of 4 signatures, you have not signed it and can. Group: if Karrz invites you, you join.". The group
// line says what GroupInviteAcceptFire (PoliteWorldBots) would really do with his invite.
std::string SocialText(Player* bot, Player* sender)
{
    std::string out;
    std::string const bot_guild = bot->GetGuildName();
    out += bot_guild.empty() ? "You have no guild." : "You are in the guild " + bot_guild + ".";
    if (bot->GetGuildIdInvited() && !bot->GetGuildId())
        out += " You have a guild invite open.";
    std::string const his_guild = sender->GetGuildName();
    out += " " + sender->GetName() + (his_guild.empty() ? " has no guild." : " is in the guild " + his_guild +
           (!bot_guild.empty() && his_guild == bot_guild ? " (yours too)." : "."));
    if (Petition* p = CharterOf(sender))
    {
        bool signed_it = false;
        for (Signature const& sig : p->Signatures)
            signed_it = signed_it || sig.second == bot->GetGUID();
        char const* why_not = nullptr;
        if (signed_it)
            why_not = nullptr;
        else if (bot->GetGuildId() || bot->GetGuildIdInvited())
            why_not = "you can't, you are in a guild";
        else if (!sWorld->getBoolConfig(CONFIG_ALLOW_TWO_SIDE_INTERACTION_GUILD) && bot->GetTeam() != sender->GetTeam())
            why_not = "you can't, he is of the other faction";
        else if (p->Signatures.size() >= 10)
            why_not = "it is full";
        else if (BotAI const* ai = Services::Registry().ai(bot->GetGUID().GetCounter()); ai && ai->guild_charter_active())
            why_not = "you can't, you are getting your own charter signed";
        out += fmt::format(" {} carries a guild charter for {}: {} of {} signatures, {}.", sender->GetName(), p->PetitionName,
                           p->Signatures.size(), sWorld->getIntConfig(CONFIG_MIN_PETITION_SIGNS),
                           signed_it ? "you signed it already" : why_not ? why_not : "you have not signed it and can (sign_charter)");
    }
    BotId const id = bot->GetGUID().GetCounter();
    OwnerBinding const ob = Services::Owners().GetOwner(id);
    Group const* g = bot->GetGroup();
    std::string group;
    if (g && g->IsMember(sender->GetGUID()))
        group = "you are in a group with him already";
    else if (ob.account_id)
        group = OwnsBot(sender, id) ? "you are his own companion" : "you go with the player who owns you, not with others";
    else if (g)
        group = "you are in another group, you can't join his";
    else if (!bot->IsAlive())
        group = "you can't right now, you are dead";
    else if (bot->IsInCombat())
        group = "not now, you are fighting";
    else if (bot->InBattleground())
        group = "not now, you are in a battleground";
    else if (int32(sender->GetLevel()) > int32(bot->GetLevel()) + 5 && sender->GetSession() &&
             sender->GetSession()->GetSecurity() < SEC_GAMEMASTER && Services::Owners().BotsOwnedBy(sender->GetSession()->GetAccountId()).empty())
        group = "you would say no, he is too high level for you";
    else
        group = "if he invites you, you join him (tell him to invite you)";
    out += " Group: " + group + ".";
    if (!bot->GetGuildId() && !ob.account_id)
        out += " Guild: if he invites you to his guild, you join it.";
    return out;
}

// What the bot carries that a player may ask for, in words for the program: "food: Darnassian Bleu x3; drink: none; potions: none;
// bandages: none; can conjure: water, food". A model that does not know the bags promised water it did not have (Gun, 2026-11-08).
std::string CarryText(Player* bot)
{
    std::vector<std::pair<std::string, uint32>> food, drink, potions, bandages;
    auto add = [](std::vector<std::pair<std::string, uint32>>& list, std::string name, uint32 count)
    {
        for (auto& e : list)
            if (e.first == name)
            {
                e.second += count;
                return;
            }
        if (list.size() < 4)
            list.emplace_back(std::move(name), count);
    };
    bot->ForEachItem(ItemSearchLocation::Inventory, [&](Item* it)
    {
        ItemTemplate const* t = it ? it->GetTemplate() : nullptr;
        if (!t || t->GetClass() != ITEM_CLASS_CONSUMABLE)
            return ItemSearchCallbackResult::Continue;
        std::string name = t->GetName(DEFAULT_LOCALE);
        uint32 const sub = t->GetSubClass();
        if (sub == ITEM_SUBCLASS_FOOD_DRINK)
        {
            bool mana = false, health = false;
            for (ItemEffectEntry const* e : t->Effects)
                if (e)
                    if (SpellInfo const* si = sSpellMgr->GetSpellInfo(uint32(e->SpellID), DIFFICULTY_NONE))
                    {
                        mana = mana || si->HasAura(SPELL_AURA_MOD_POWER_REGEN);
                        health = health || si->HasAura(SPELL_AURA_MOD_REGEN);
                    }
            add(mana && !health ? drink : food, std::move(name), it->GetCount());
        }
        else if (sub == ITEM_SUBCLASS_POTION || sub == ITEM_SUBCLASS_ELIXIR || sub == ITEM_SUBCLASS_FLASK)
            add(potions, std::move(name), it->GetCount());
        else if (sub == ITEM_SUBCLASS_BANDAGE)
            add(bandages, std::move(name), it->GetCount());
        return ItemSearchCallbackResult::Continue;
    });
    auto list = [](char const* what, std::vector<std::pair<std::string, uint32>> const& l)
    {
        std::string s = std::string(what) + ": ";
        if (l.empty())
            return s + "none";
        for (size_t i = 0; i < l.size(); ++i)
            s += (i ? ", " : "") + l[i].first + (l[i].second > 1 ? " x" + std::to_string(l[i].second) : std::string());
        return s;
    };
    std::string s = list("food", food) + "; " + list("drink", drink) + "; " + list("potions", potions) + "; " + list("bandages", bandages);
    auto knows = [bot](uint32 spell) { return bot->HasSpell(spell); };
    bool const water = Classic::BestMageConjure(Classic::ConsumableKind::Drink, knows) != 0;
    bool const bread = Classic::BestMageConjure(Classic::ConsumableKind::Food, knows) != 0;
    if (water || bread)
        s += std::string("; can conjure: ") + (water ? "water" : "") + (water && bread ? ", " : "") + (bread ? "food" : "");
    // Gun promised water three times with full bags: a conjure that cannot fit makes nothing (2026-11-09).
    uint32 const free_slots = bot->GetFreeInventorySlotCount();
    s += free_slots ? "; free bag slots: " + std::to_string(free_slots)
                    : std::string("; bags FULL (0 free slots): you can't conjure, make or take anything until you sell or drop something");
    return s;
}

// ---- moments (Playerbot.Bridge.Moments): the lines a bot says on its own in party, written by the program ----
// BotChatter decides WHEN a bot speaks (its chance and cooldowns, unchanged); with a program answering chat it hands the moment
// here instead of saying a fixed line, and the program writes WHAT the bot says. Queued from any thread (bot workers, map
// threads), written on the world thread.
struct Moment { BotId bot = 0; std::string what, detail, example; uint32 queued_ms = 0; };
std::atomic<bool>   g_moments_on{false};
std::mutex          g_moment_mtx;
std::vector<Moment> g_moments;          // g_moment_mtx
std::deque<uint32>  g_moment_minute;    // g_moment_mtx: when the moments of the last minute were queued

// The real player a bot's party line is for: its owner when he is in the group, else the first real player in the group.
Player* MomentListener(Player* bot)
{
    Group* g = bot->GetGroup();
    if (!g)
        return nullptr;
    BotId const id = bot->GetGUID().GetCounter();
    Player* first = nullptr;
    for (GroupReference const& ref : g->GetMembers())
    {
        Player* m = ref.GetSource();
        if (!m || m == bot || !m->GetSession() || m->GetSession()->IsBot())
            continue;
        if (OwnsBot(m, id))
            return m;
        if (!first)
            first = m;
    }
    return first;
}

// The language of the player's game client ("ruRU", "enUS"): the program answers a Russian client in Russian when his line
// gives no language of its own (a moment, "ok", a link). GLIPT greeted his group in Russian and got English (2026-11-09).
std::string ClientLocale(Player const* player)
{
    WorldSession const* s = player ? player->GetSession() : nullptr;
    LocaleConstant const l = s ? s->GetSessionDbcLocale() : LOCALE_enUS;
    return l < TOTAL_LOCALES ? localeNames[l] : "enUS";
}

// The language most real players on the bot's map play in, for lines no one player asked for (a street talk, a General or
// Trade post): a Russian realm got English posts (Geshka, 2026-11-09). No player there: the realm's own language.
std::string ListenersLocale(Player const* bot)
{
    std::array<uint32, TOTAL_LOCALES> count{};
    if (Map* map = bot ? bot->GetMap() : nullptr)
        for (MapReference const& ref : map->GetPlayers())
        {
            Player const* p = ref.GetSource();
            if (p && p->GetSession() && !Services::Registry().has(p->GetGUID().GetCounter()))
            {
                LocaleConstant const l = p->GetSession()->GetSessionDbcLocale();
                if (l < TOTAL_LOCALES)
                    ++count[l];
            }
        }
    std::size_t best = std::size_t(sWorld->GetDefaultDbcLocale());
    for (std::size_t l = 0; l < count.size(); ++l)
        if (count[l] > count[best])
            best = l;
    return best < TOTAL_LOCALES ? localeNames[best] : "enUS";
}

void WriteMoment(ConfigReader::BridgeConfig const& cfg, Moment const& m, uint32 now_ms)
{
    Player* bot = FindBot(m.bot);
    if (!bot || !bot->IsInWorld())
        return;
    Player* to = MomentListener(bot);
    if (!to)
        return;
    Event e;
    e.type = "moment";
    FillBot(e, bot, 0);
    e.fields.emplace_back("what", m.what);
    e.fields.emplace_back("detail", m.detail);
    if (!m.example.empty())
        e.fields.emplace_back("example", m.example);
    e.fields.emplace_back("channel", "party");
    e.fields.emplace_back("to", to->GetName());
    e.fields.emplace_back("locale", ClientLocale(to));
    bool const owned = Services::Owners().GetOwner(m.bot).account_id != 0;
    e.numbers.emplace_back("to_is_owner", owned && OwnsBot(to, m.bot) ? 1 : 0);
    if (cfg.actions)
    {
        // A moment may come with one action for the player it is said to (offer him a trade, wave), checked as for his own ask.
        RefreshAllowed(cfg);
        e.raw.emplace_back("can", AllowedActsJson(MakeContext(cfg, bot, to), g_allowed));
    }
    if (cfg.swearing)
        e.numbers.emplace_back("swearing", 1);
    e.fields.emplace_back("weather", WeatherWord(bot->GetMap() ? bot->GetMap()->GetZoneWeather(bot->GetZoneId()) : WEATHER_STATE_FINE));
    e.fields.emplace_back("time_of_day", TimeOfDayWord());
    Write(e);
    if (g_chat.size() > 2048)
        g_chat.clear();
    // The answer comes back as a reply to this id, so it goes to party and its action is checked for `to`, as for a chat_in.
    g_chat[e.id] = ChatAsk{ m.bot, to->GetName(), to->GetGUID(), false, "party", now_ms };
    TC_LOG_INFO("playerbot.v2", "[bridge] moment {} for {} ({}): {}", m.what, bot->GetName(), to->GetName(), m.detail);
}

// ---- channel posts (Playerbot.Bridge.Talk): a world bot's WTS / LFM / question in General or Trade, written by the program, and
// now and then another bot answering it there, so the channels read like a live realm's and a post leads somewhere ----
struct Post { BotId bot = 0; uint8 channel_id = 1; std::string what, detail, example, link_name, link; uint32 queued_ms = 0; };
std::atomic<bool>  g_talk_on{false};
std::vector<Post>  g_posts;         // g_moment_mtx
std::deque<uint32> g_post_minute;   // g_moment_mtx

char const* ChannelWord(uint8 channel_id) { return channel_id == 2 ? "trade" : "general"; }
char const* ChannelTitle(uint8 channel_id) { return channel_id == 2 ? "Trade" : "General"; }

// Asks the program for one channel line: `scene` what to write, said by `bot` in that channel.
// The last few posts of one channel as the side reads it (Trade: the side's cities; General: one zone), so a new post does not echo
// them and an answer knows what was said. World thread.
std::unordered_map<uint64, std::deque<std::string>> g_recent_posts;

uint64 ChannelKey(uint32 team, uint8 channel_id, uint32 zone)
{
    return (uint64(team) << 40) | (uint64(channel_id) << 32) | (channel_id == 2 ? 0u : zone);
}

std::string RecentPostsText(uint64 key)
{
    auto it = g_recent_posts.find(key);
    if (it == g_recent_posts.end() || it->second.empty())
        return {};
    std::string out = " The last posts there (never repeat or copy them, write something else):";
    for (std::string const& p : it->second)
        out += " \"" + p + "\"";
    return out + ".";
}

void NoteRecentPost(uint64 key, std::string const& who, std::string const& plain)
{
    std::deque<std::string>& d = g_recent_posts[key];
    d.push_back(who + ": " + plain.substr(0, 120));
    while (d.size() > 6)
        d.pop_front();
    if (g_recent_posts.size() > 512)
        g_recent_posts.clear();
}

// `keep`: words the program must write exactly as they are (an item's name that becomes its link: no typo in it).
uint64 WriteChannelTalk(ConfigReader::BridgeConfig const& cfg, Player* bot, uint8 channel_id, std::string const& scene, uint32 now_ms,
                        std::string const& keep = {})
{
    Event e;
    e.type = "talk";
    FillBot(e, bot, 0);
    e.fields.emplace_back("channel", ChannelWord(channel_id));
    e.fields.emplace_back("scene", scene);
    e.fields.emplace_back("talk", "");
    e.fields.emplace_back("locale", ListenersLocale(bot));
    if (!keep.empty())
        e.fields.emplace_back("keep", keep);
    if (cfg.swearing)
        e.numbers.emplace_back("swearing", 1);
    e.fields.emplace_back("time_of_day", TimeOfDayWord());
    Write(e);
    if (g_chat.size() > 2048)
        g_chat.clear();
    g_chat[e.id] = ChatAsk{ bot->GetGUID().GetCounter(), std::string(), ObjectGuid::Empty, false, ChannelWord(channel_id), now_ms };
    return e.id;
}

void WritePost(ConfigReader::BridgeConfig const& cfg, Post const& p, uint32 now_ms)
{
    Player* bot = FindBot(p.bot);
    if (!bot || !bot->IsInWorld())
        return;
    std::string scene = fmt::format("You are in {}. You write ONE post in the {} channel, the way players post there: short, with the "
        "usual shorthand when it fits (WTS, WTB, LFM, LFG, LF1M, pst, PM me, any1, ty). What it is about: {}.", AreaName(bot->GetZoneId()),
        ChannelTitle(p.channel_id), p.detail);
    // The quote marks are escaped: without them the model read the code's own words and wrote "p.link_name" in Trade (PC 2026-11-09).
    if (!p.link_name.empty())
        scene += " Write the name \"" + p.link_name + "\" exactly like that, once.";
    scene += " " + ::Playerbot::V2::World::VanillaLevelPlaces(uint8(bot->GetLevel()));
    scene += RecentPostsText(ChannelKey(uint32(bot->GetTeam()), p.channel_id, bot->GetZoneId()));
    scene += " No greeting, no quotes, no hashtags.";
    uint64 const id = WriteChannelTalk(cfg, bot, p.channel_id, scene, now_ms, p.link_name);
    if (g_post_ask.size() > 256)
        g_post_ask.clear();
    g_post_ask[id] = PostAsk{ p.channel_id, p.link_name, p.link, p.example, false };
    TC_LOG_INFO("playerbot.v2", "[bridge] post {} for {} in {}: {}", p.what, bot->GetName(), ChannelTitle(p.channel_id), p.detail);
}

struct Greet { BotId bot = 0; ObjectGuid player; uint32 queued_ms = 0; };
std::vector<Greet> g_greets;   // g_moment_mtx

// A greeting the passer-by rule asked for: the program writes one short /say line to the player the bot just waved at.
void DrainGreets(uint32 now_ms)
{
    std::vector<Greet> list;
    {
        std::lock_guard<std::mutex> lk(g_moment_mtx);
        if (g_greets.empty())
            return;
        list.swap(g_greets);
    }
    for (Greet const& g : list)
    {
        Player* bot = FindBot(g.bot);
        Player* who = ObjectAccessor::FindConnectedPlayer(g.player);
        if (now_ms - g.queued_ms > 5000 || !bot || !who || !bot->IsInWorld() || who->GetMap() != bot->GetMap() || bot->GetDistance(who) > 20.0f)
            continue;
        std::string about = fmt::format("{}, a level {} {} {}", who->GetName(), who->GetLevel(), RaceName(who->GetRace()), ClassName(who->GetClass()));
        Unit::AuraEffectList const& mounts = who->GetAuraEffectsByType(SPELL_AURA_MOUNTED);
        if (!mounts.empty() && mounts.front()->GetSpellInfo())
            about += fmt::format(", riding a {}", mounts.front()->GetSpellInfo()->SpellName->Str[LOCALE_enUS]);
        if (std::string const guild = who->GetGuildName(); !guild.empty())
            about += ", of the guild " + guild;
        std::string const scene = fmt::format("You are in {}. {} walks past you, a stranger, and you just {}. Say one short line to him in "
            "/say the way a friendly stranger in town would (hi, a word about his mount or his gear, a quick question), or nothing: then "
            "answer with an empty say. Never more than one line.", AreaName(bot->GetAreaId() ? bot->GetAreaId() : bot->GetZoneId()), about,
            "waved at him");
        BotBridge::AskTalk(bot, scene, std::string(), 0, Services::Config().gathering().model_per_minute);
    }
}

void DrainPosts(ConfigReader::BridgeConfig const& cfg, uint32 now_ms)
{
    std::vector<Post> list;
    {
        std::lock_guard<std::mutex> lk(g_moment_mtx);
        if (g_posts.empty())
            return;
        list.swap(g_posts);
    }
    for (Post const& p : list)
        if (now_ms - p.queued_ms <= 10000)
            WritePost(cfg, p, now_ms);
}

// The model's post as it is said: the item's or quest's name becomes its link (the model can't write one); a post that left the
// name out is the fixed line instead. Kept as the bot's last post; now and then another bot will answer it.
std::string FinishPost(BotId bot, PostAsk const& post, std::string text, uint32 now_ms)
{
    if (post.answer)
    {
        if (Player* p = FindBot(bot))
            NoteRecentPost(ChannelKey(uint32(p->GetTeam()), post.channel_id, p->GetZoneId()), p->GetName(), text);
        return text;
    }
    if (!post.link_name.empty())
    {
        std::string low = text, name = post.link_name;
        std::transform(low.begin(), low.end(), low.begin(), [](unsigned char c) { return char(std::tolower(c)); });
        std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return char(std::tolower(c)); });
        std::size_t const at = low.find(name);
        if (at == std::string::npos)
        {
            if (post.example.empty())
                return {};
            text = post.example;
        }
        else
        {
            std::string const plain = text;
            text = plain.substr(0, at) + post.link + plain.substr(at + name.size());
        }
    }
    Player* p = FindBot(bot);
    if (!p)
        return text;
    uint32 entry = 0;
    std::string first;
    std::string const plain = V2::ChannelTalk::StripLinks(text, entry, first);
    if (g_last_post.size() > 4096)
        g_last_post.clear();
    g_last_post[bot] = LastPost{ ChannelTitle(post.channel_id), plain, now_ms };
    NoteRecentPost(ChannelKey(uint32(p->GetTeam()), post.channel_id, p->GetZoneId()), p->GetName(), plain);
    // Two posts in five get an answer from another bot of the side, 10 to 40 s later.
    if (urand(0, 4) < 2 && g_post_answers.size() < 16)
        g_post_answers.push_back(PostAnswer{ now_ms + urand(10000, 40000), bot, p->GetName(), plain, post.channel_id, uint32(p->GetTeam()),
                                             p->GetZoneId(), uint8(p->GetLevel()) });
    return text;
}

// A due answer: a free world bot of the poster's side (in a city for Trade, in the same zone for General) writes one line back.
void RunPostAnswers(ConfigReader::BridgeConfig const& cfg, uint32 now_ms)
{
    if (g_post_answers.empty())
        return;
    while (!g_post_answer_minute.empty() && now_ms - g_post_answer_minute.front() >= 60000)
        g_post_answer_minute.pop_front();
    for (auto it = g_post_answers.begin(); it != g_post_answers.end();)
    {
        if (int32(now_ms - it->due_ms) < 0) { ++it; continue; }
        PostAnswer const a = *it;
        it = g_post_answers.erase(it);
        if (g_post_answer_minute.size() >= 2 || now_ms - a.due_ms > 30000)
            continue;
        Player* pick = nullptr;
        uint32 seen = 0;
        Services::Registry().for_each([&](BotId id, BotRegistryEntry const&)
        {
            if (id == a.poster || seen >= 24)
                return;
            Player* p = FindBot(id);
            if (!p || !p->IsInWorld() || !p->IsAlive() || p->IsInCombat() || p->GetGroup() || uint32(p->GetTeam()) != a.team)
                return;
            if (Services::Owners().GetOwner(id).account_id || !p->GetMap() || p->GetMap()->Instanceable() || ::Playerbot::Pool::IsScenarioBot(id))
                return;
            AreaTableEntry const* zone = sAreaTableStore.LookupEntry(p->GetZoneId());
            if (!zone || zone->GetFlags().HasFlag(AreaFlags::NoChatChannels))
                return;
            if (a.channel_id == 2 ? !zone->GetFlags().HasFlag(AreaFlags::AllowTradeChannel) : p->GetZoneId() != a.zone)
                return;
            ++seen;
            if (urand(1, seen) == 1)
                pick = p;   // one at random among the first few that fit
        });
        if (!pick)
            continue;
        // The answer names places for the poster's level (he asked), and only what is true there: "Blackfathom Deeps is good for
        // grinding around 60" and "Westfall or Redridge" for a 60 were the model's own guesses (PC 2026-11-09).
        std::string const scene = fmt::format("You are in {}. In the {} channel {} (level {}) just wrote: \"{}\". You answer in that "
            "channel with one short line, the way players do: a question (how much? which dungeon? what level?), an offer, pst, inv, a "
            "joke or a small complaint. You may name {}. Never more than one line. If you name a place for {}: {}{}",
            AreaName(pick->GetZoneId()), ChannelTitle(a.channel_id), a.poster_name, uint32(a.poster_level), a.text, a.poster_name,
            a.poster_name, ::Playerbot::V2::World::VanillaLevelPlaces(a.poster_level),
            RecentPostsText(ChannelKey(a.team, a.channel_id, a.channel_id == 2 ? 0u : a.zone)));
        uint64 const id = WriteChannelTalk(cfg, pick, a.channel_id, scene, now_ms);
        g_post_ask[id] = PostAsk{ a.channel_id, {}, {}, {}, true };
        g_post_answer_minute.push_back(now_ms);
        TC_LOG_INFO("playerbot.v2", "[bridge] {} answers {}'s {} post", pick->GetName(), a.poster_name, ChannelTitle(a.channel_id));
    }
}

void DrainMoments(ConfigReader::BridgeConfig const& cfg, uint32 now_ms)
{
    std::vector<Moment> list;
    {
        std::lock_guard<std::mutex> lk(g_moment_mtx);
        if (g_moments.empty())
            return;
        list.swap(g_moments);
    }
    for (Moment const& m : list)
        if (now_ms - m.queued_ms <= 10000)   // older: the moment has passed
            WriteMoment(cfg, m, now_ms);
}


// ---- speaking up (part of the moments): a bot that needs something says so, and offers its owner what helps him ----
// A real friend says "bags full, need a vendor" or "got a blue here that fits you, want it?". Every 20 s the owned bots grouped with
// their owner are looked at (out of combat, alive, near him). One need or offer per owner every 5 minutes; each kind per bot at most
// every 30 minutes; an item is offered once. The program writes the line and may open the trade (the "can" list is the owner's).
std::unordered_map<uint64, uint32> g_speak_owner_ms;      // owner guid low -> last need/offer moment
std::unordered_map<uint64, uint32> g_speak_kind_ms;       // (bot << 8 | kind) -> last time
std::unordered_set<uint64>         g_offered;             // (bot << 32 | item entry) already offered
uint32 g_next_speak_ms = 0;

enum : uint8 { kNeedBags = 1, kNeedRepair = 2, kOfferGear = 3, kOfferQuest = 4 };

bool SpeakKindReady(BotId bot, uint8 kind, uint32 now_ms)
{
    uint64 const key = (uint64(bot) << 8) | kind;
    auto it = g_speak_kind_ms.find(key);
    return it == g_speak_kind_ms.end() || now_ms - it->second >= 1800000u;
}

void SpeakUp(uint32 now_ms)
{
    if (g_speak_kind_ms.size() > 8192)
        g_speak_kind_ms.clear();
    if (g_offered.size() > 65536)
        g_offered.clear();
    for (auto const& [account, session] : sWorld->GetAllSessions())
    {
        Player* owner = session ? session->GetPlayer() : nullptr;
        if (!owner || session->IsBot() || !owner->IsInWorld() || !owner->GetGroup() || owner->IsInCombat())
            continue;
        uint64 const owner_low = owner->GetGUID().GetCounter();
        if (auto it = g_speak_owner_ms.find(owner_low); it != g_speak_owner_ms.end() && now_ms - it->second < 300000u)
            continue;
        for (GroupReference const& ref : owner->GetGroup()->GetMembers())
        {
            Player* bot = ref.GetSource();
            if (!bot || bot == owner || !bot->GetSession() || !bot->GetSession()->IsBot() || !bot->IsAlive() || bot->IsInCombat()
                || bot->GetMap() != owner->GetMap() || !bot->IsWithinDistInMap(owner, 40.0f))
                continue;
            BotId const id = bot->GetGUID().GetCounter();
            if (!Services::Registry().has(id) || !OwnsBot(owner, id))
                continue;
            uint8 kind = 0;
            uint64 offer_key = 0;
            std::string detail;
            if (SpeakKindReady(id, kNeedBags, now_ms) && bot->GetFreeInventorySlotCount() == 0)
            {
                kind = kNeedBags;
                detail = "your bags are completely full, you can't pick up any more loot; you need a vendor";
            }
            if (!kind && SpeakKindReady(id, kNeedRepair, now_ms))
                for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END && !kind; ++slot)
                    if (Item* it = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
                        if (uint32 const max = *it->m_itemData->MaxDurability; max && *it->m_itemData->Durability * 100u < max * 15u)
                        {
                            kind = kNeedRepair;
                            detail = std::string("your ") + it->GetTemplate()->GetName(DEFAULT_LOCALE) + " is nearly broken; you need a repair soon";
                        }
            if (!kind)
            {
                // Something in the bags for him: an upgrade he can wear, or an item one of his quests asks for.
                for (Item* it : TradeShop::PickGifts(bot, owner, "stuff"))
                {
                    ItemTemplate const* t = it->GetTemplate();
                    bool const quest = owner->HasQuestForItem(t->GetId());
                    bool const gear = !quest && TradeShop::IsUpgrade(owner, t) && owner->CanUseItem(t) == EQUIP_ERR_OK;
                    uint64 const offered = (uint64(id) << 32) | t->GetId();
                    if ((!quest && !gear) || g_offered.count(offered) || !SpeakKindReady(id, quest ? kOfferQuest : kOfferGear, now_ms))
                        continue;
                    offer_key = offered;
                    kind = quest ? kOfferQuest : kOfferGear;
                    detail = quest ? std::string("you have ") + t->GetName(DEFAULT_LOCALE) + " in your bags, and " + owner->GetName()
                                         + " needs it for a quest; you could trade it to him (trade, items: its name)"
                                   : std::string("you have ") + t->GetName(DEFAULT_LOCALE) + " in your bags; it would be an upgrade for "
                                         + owner->GetName() + "; you could offer it (trade, items: its name)";
                    break;
                }
            }
            if (!kind)
                continue;
            char const* const what = kind == kNeedBags || kind == kNeedRepair ? "need" : "offer";
            if (BotBridge::QueueMoment(id, what, detail, {}))
            {
                g_speak_kind_ms[(uint64(id) << 8) | kind] = now_ms;
                g_speak_owner_ms[owner_low] = now_ms;
                if (offer_key)
                    g_offered.insert(offer_key);   // offered once; he can still ask for it
                TC_LOG_INFO("playerbot.v2", "[bridge] {} speaks up to {}: {}", bot->GetName(), owner->GetName(), detail);
            }
            break;   // one bot per owner per look
        }
    }
}

} // namespace

bool BotBridge::Enabled() { return g_enabled.load(std::memory_order_relaxed); }

bool BotBridge::MomentsOn() { return g_moments_on.load(std::memory_order_relaxed); }

bool BotBridge::QueueMoment(BotId bot, std::string what, std::string detail, std::string example)
{
    if (!bot || !g_moments_on.load(std::memory_order_relaxed))
        return false;
    uint32 const now_ms = GameTime::GetGameTimeMS();
    uint32 const per_minute = Services::Config().bridge().moments_per_minute;
    std::lock_guard<std::mutex> lk(g_moment_mtx);
    while (!g_moment_minute.empty() && now_ms - g_moment_minute.front() >= 60000)
        g_moment_minute.pop_front();
    if (g_moment_minute.size() >= per_minute || g_moments.size() >= 32)
        return false;   // the model's time goes to players' chat first: this one gets the fixed line
    g_moment_minute.push_back(now_ms);
    g_moments.push_back(Moment{ bot, std::move(what), std::move(detail), std::move(example), now_ms });
    return true;
}

bool BotBridge::TalkOn()
{
    return ChatOn() && Services::Config().bridge().talk;
}

bool BotBridge::AskTalk(Player* bot, std::string const& scene, std::string const& chat, uint64 key, uint32 per_minute)
{
    if (!TalkOn() || !bot)
        return false;   // key 0: a single line nobody waits for (a greeting)
    uint32 const now_ms = GameTime::GetGameTimeMS();
    while (!g_talk_minute.empty() && now_ms - g_talk_minute.front() >= 60000)
        g_talk_minute.pop_front();
    if (g_talk_minute.size() >= per_minute)
        return false;
    // Players come first: while two of their lines wait for an answer, the bots keep quiet.
    std::size_t waiting = 0;
    for (auto const& kv : g_chat)
        if (!kv.second.talk_key && !kv.second.sender_is_bot && !kv.second.sender.empty() && now_ms - kv.second.asked_ms < 20000)
            ++waiting;
    if (waiting >= 2)
        return false;
    Event e;
    e.type = "talk";
    FillBot(e, bot, 0);
    e.fields.emplace_back("channel", "say");
    e.fields.emplace_back("scene", scene.size() > 1500 ? scene.substr(0, 1500) : scene);
    e.fields.emplace_back("talk", chat.size() > 1500 ? chat.substr(chat.size() - 1500) : chat);
    e.fields.emplace_back("locale", ListenersLocale(bot));
    if (Services::Config().bridge().swearing)
        e.numbers.emplace_back("swearing", 1);
    e.fields.emplace_back("weather", WeatherWord(bot->GetMap() ? bot->GetMap()->GetZoneWeather(bot->GetZoneId()) : WEATHER_STATE_FINE));
    e.fields.emplace_back("time_of_day", TimeOfDayWord());
    Write(e);
    if (g_chat.size() > 2048)
        g_chat.clear();
    g_chat[e.id] = ChatAsk{ bot->GetGUID().GetCounter(), std::string(), ObjectGuid::Empty, false, "say", now_ms, key ? key : kNoTalkKey };
    g_talk_minute.push_back(now_ms);
    return true;
}

bool BotBridge::QueueGreet(BotId bot, ObjectGuid player)
{
    if (!bot || player.IsEmpty() || !g_talk_on.load(std::memory_order_relaxed))
        return false;
    std::lock_guard<std::mutex> lk(g_moment_mtx);
    if (g_greets.size() >= 4)
        return false;
    g_greets.push_back(Greet{ bot, player, GameTime::GetGameTimeMS() });
    return true;
}

bool BotBridge::QueuePost(BotId bot, uint8 channel_id, std::string what, std::string detail, std::string example, std::string link_name,
                          std::string link)
{
    if (!bot || !g_talk_on.load(std::memory_order_relaxed))
        return false;
    uint32 const now_ms = GameTime::GetGameTimeMS();
    uint32 const per_minute = Services::Config().bridge().moments_per_minute;
    std::lock_guard<std::mutex> lk(g_moment_mtx);
    while (!g_post_minute.empty() && now_ms - g_post_minute.front() >= 60000)
        g_post_minute.pop_front();
    if (g_post_minute.size() >= per_minute || g_posts.size() >= 8)
        return false;
    g_post_minute.push_back(now_ms);
    g_posts.push_back(Post{ bot, channel_id == 2 ? uint8(2) : uint8(1), std::move(what), std::move(detail), std::move(example),
                            std::move(link_name), std::move(link), now_ms });
    return true;
}

std::vector<std::pair<std::string, std::string>> BotBridge::TakeTalk(uint64 key)
{
    std::vector<std::pair<std::string, std::string>> out;
    if (auto it = g_talk_said.find(key); it != g_talk_said.end())
    {
        out.swap(it->second);
        g_talk_said.erase(it);
    }
    return out;
}

bool BotBridge::ChatOn()
{
    return Enabled() && g_started && g_client_alive && Services::Config().bridge().chat;
}

uint64 BotBridge::ReportChat(Player* bot, Player* sender, std::string const& channel, std::string const& text, std::string const& prompt,
                             std::vector<BotId> const& group)
{
    if (!ChatOn() || !bot || !sender)
        return 0;
    Event e;
    e.type = "chat_in";
    FillBot(e, bot, 0);
    e.fields.emplace_back("channel", channel);
    std::vector<BotId> others;
    if (!group.empty())
    {
        // "all go to the inn": the others the order is for, by name, so the line can speak for the group ("on our way").
        std::string names;
        for (BotId g : group)
            if (Player* p = FindBot(g); p && g != bot->GetGUID().GetCounter())
            {
                others.push_back(g);
                names += (names.empty() ? "" : ", ") + p->GetName();
            }
        e.numbers.emplace_back("group_order", 1);
        e.fields.emplace_back("group", names);
    }
    e.fields.emplace_back("from", sender->GetName());
    e.fields.emplace_back("locale", ClientLocale(sender));
    e.numbers.emplace_back("from_is_bot", Services::Registry().has(sender->GetGUID().GetCounter()) ? 1 : 0);
    e.fields.emplace_back("text", text.size() > 500 ? text.substr(0, 500) : text);
    if (Services::Config().bridge().prompt_in_chat && !prompt.empty())
        e.fields.emplace_back("prompt", prompt);
    uint32 const now_ms = GameTime::GetGameTimeMS();
    BotId const bot_id = bot->GetGUID().GetCounter();
    bool const from_bot = Services::Registry().has(sender->GetGUID().GetCounter());
    if (Services::Config().bridge().actions)
    {
        // What the program needs to pick an action: who owns the bot, what the speaker has selected, and what is near (refs).
        bool const owned = Services::Owners().GetOwner(bot_id).account_id != 0;
        e.numbers.emplace_back("owned", owned ? 1 : 0);
        e.numbers.emplace_back("from_is_owner", owned && OwnsBot(sender, bot_id) ? 1 : 0);
        if (Unit* sel = sender->GetSelectedUnit(); sel && sel != bot && sel != sender && sel->IsInMap(bot))
        {
            e.fields.emplace_back("speaker_target", sel->GetName());
            e.fields.emplace_back("speaker_target_kind", sel->GetTypeId() == TYPEID_PLAYER ? "player"
                                                       : bot->IsHostileTo(sel) ? "hostile" : bot->IsFriendlyTo(sel) ? "friendly" : "neutral");
        }
        e.raw.emplace_back("near", BuildNear(bot, now_ms));
        // What this bot may do for this player right now, and the spells it may cast for him: so the program offers only real things.
        auto const& cfg = Services::Config().bridge();
        RefreshAllowed(cfg);
        e.raw.emplace_back("can", AllowedActsJson(MakeContext(cfg, bot, sender), g_allowed));
        e.fields.emplace_back("carry", CarryText(bot));
        if (cfg.actions_spells)
        {
            std::vector<SpellItem> spells;
            for (KnownSpell const& k : UsableSpells(bot))
                spells.push_back(k.item);
            if (!spells.empty())
                e.raw.emplace_back("spells", BuildSpellsJson(spells));
        }
    }
    // A player who whispers a bot about its post ("how much?", "still need a healer?") gets an answer that fits what it posted.
    if (auto lp = g_last_post.find(bot_id); lp != g_last_post.end() && now_ms - lp->second.ms < 900000u)
        e.fields.emplace_back("your_post", fmt::format("{} minutes ago you wrote in {}: {}", (now_ms - lp->second.ms) / 60000u,
                                                       lp->second.channel, lp->second.text));
    // Guild and group: "want to join my guild?", "sign my charter?", "wanna quest?" get a true answer (also without actions).
    if (!from_bot)
        e.fields.emplace_back("social", SocialText(bot, sender));
    if (Services::Config().bridge().swearing)
        e.numbers.emplace_back("swearing", 1);
    // Small talk ("what's the weather like?"): the zone's weather and the server's time of day.
    e.fields.emplace_back("weather", WeatherWord(bot->GetMap() ? bot->GetMap()->GetZoneWeather(bot->GetZoneId()) : WEATHER_STATE_FINE));
    e.fields.emplace_back("time_of_day", TimeOfDayWord());
    Write(e);
    if (g_chat.size() > 2048)
        g_chat.clear();
    g_chat[e.id] = ChatAsk{ bot_id, sender->GetName(), sender->GetGUID(), from_bot, channel, now_ms, 0, std::move(others) };
    return e.id;
}

bool BotBridge::AskGroupOrder(Player* sender, std::vector<Player*> const& group, std::string const& channel, std::string const& text)
{
    if (!ChatOn() || !Services::Config().bridge().actions || !sender || group.empty() || text.empty())
        return false;
    std::vector<BotId> ids;
    for (Player const* p : group)
        if (p)
            ids.push_back(p->GetGUID().GetCounter());
    // The one who answers: a bot the sender owns when there is one (his companion speaks for his group), else the first.
    Player* talker = group.front();
    for (Player* p : group)
        if (p && OwnsBot(sender, p->GetGUID().GetCounter()))
        {
            talker = p;
            break;
        }
    uint64 const id = ReportChat(talker, sender, channel, text, {}, ids.size() > 1 ? ids : std::vector<BotId>{});
    if (id)
        TC_LOG_INFO("playerbot.v2", "[bridge] group order from {} for {} bot(s), {} answers: \"{}\"", sender->GetName(), ids.size(),
                    talker->GetName(), text.substr(0, 80));
    return id != 0;
}

void BotBridge::ReportStuck(BotId bot, std::string detail)
{
    if (!Enabled())
        return;
    Report r;
    r.bot = bot;
    r.type = "stuck";
    r.detail = std::move(detail);
    QueueReport(std::move(r));
}

void BotBridge::ReportQuestFail(BotId bot, uint32 quest_id, std::string detail)
{
    if (!Enabled())
        return;
    Report r;
    r.bot = bot;
    r.type = "quest_fail";
    r.quest_id = quest_id;
    r.detail = std::move(detail);
    QueueReport(std::move(r));
}

void BotBridge::NoteCastFail(BotId bot, uint32 spell_id, uint32 reason)
{
    if (!Enabled() || !g_started)
        return;
    auto const& cfg = Services::Config().bridge();
    std::string const key = std::to_string(bot) + "/" + std::to_string(spell_id);
    uint32 const hits = g_casts.Hit(key, GameTime::GetGameTimeMS(), 60000);
    if (hits < cfg.spell_fail_count)
        return;
    g_casts.Reset(key);
    Report r;
    r.bot = bot;
    r.type = "spell_fail";
    r.detail = "spell failed " + std::to_string(hits) + " times in a minute";
    std::string name;
    if (SpellInfo const* info = sSpellMgr->GetSpellInfo(spell_id, DIFFICULTY_NONE))
        name = info->SpellName->Str[sWorld->GetDefaultDbcLocale()];
    r.numbers.emplace_back("spell_id", spell_id);
    r.fields.emplace_back("spell", name);
    // reason: a SpellCastResult number from the server (SharedDefines.h), or 1000+ for the bot's own pre-checks (PlayerbotAPI.h CastReject)
    r.numbers.emplace_back("reason", reason);
    QueueReport(std::move(r));
}

void BotBridge::NoteDeath(Player* bot, std::string const& killer)
{
    if (!Enabled() || !g_started || !bot)
        return;
    auto const& cfg = Services::Config().bridge();
    // 60-yard cells: deaths in one place count together.
    BotId const id = bot->GetGUID().GetCounter();
    std::string const key = std::to_string(id) + "/" + std::to_string(bot->GetMapId()) + "/" + std::to_string(int(std::floor(bot->GetPositionX() / 60.0f)))
                          + "/" + std::to_string(int(std::floor(bot->GetPositionY() / 60.0f)));
    uint32 const hits = g_deaths.Hit(key, GameTime::GetGameTimeMS(), 600000);
    if (hits < cfg.death_loop_count)
        return;
    g_deaths.Reset(key);
    Report r;
    r.bot = id;
    r.type = "death_loop";
    r.detail = "died " + std::to_string(hits) + " times in 10 minutes in the same place";
    if (!killer.empty())
        r.fields.emplace_back("killer", killer);
    QueueReport(std::move(r));
}

void BotBridge::Update(uint32 now_ms)
{
    auto const& cfg = Services::Config().bridge();
    g_enabled = cfg.enable;
    g_moments_on = cfg.enable && g_started && g_client_alive && cfg.chat && cfg.moments && cfg.moments_per_minute > 0;
    g_talk_on = cfg.enable && g_started && g_client_alive && cfg.chat && cfg.talk && cfg.moments_per_minute > 0;
    if (!cfg.enable)
    {
        if (g_started)
            Stop("bridge turned off in the config");
        return;
    }
    if (!g_started)
    {
        static uint32 s_next_try_ms = 0;   // a folder that can't be made is tried again once a minute, not every update
        if (s_next_try_ms && int32(now_ms - s_next_try_ms) < 0)
        {
            g_enabled = false;
            return;
        }
        Start(cfg, now_ms);
        if (!g_started)
        {
            s_next_try_ms = (now_ms + 60000) | 1u;
            return;
        }
        s_next_try_ms = 0;
    }
    g_gate.SetLimits(cfg.event_cooldown_sec * 1000u, cfg.max_events_per_sec);
    DrainReports(cfg, now_ms);
    if (int32(now_ms - g_next_read_ms) >= 0)
    {
        ReadReplies(cfg, now_ms);
        g_next_read_ms = now_ms + cfg.read_ms;
    }
    HandleReplies(cfg, now_ms);
    DrainMoments(cfg, now_ms);
    DrainPosts(cfg, now_ms);
    DrainGreets(now_ms);
    RunPostAnswers(cfg, now_ms);
    if (g_moments_on && int32(now_ms - g_next_speak_ms) >= 0)
    {
        SpeakUp(now_ms);
        g_next_speak_ms = now_ms + 20000;
    }
    if (!g_pending_cast.empty())
        RunPendingCasts(now_ms);
    if (int32(now_ms - g_next_leash_ms) >= 0)
    {
        if (!g_leash.empty())
            CheckLeashes(now_ms);
        g_refs.Prune(now_ms);
        g_next_leash_ms = now_ms + 1000;
    }
    if (int32(now_ms - g_next_status_ms) >= 0)
    {
        WriteStatus(now_ms);
        g_next_status_ms = now_ms + 10000;
    }
}

void BotBridge::Shutdown()
{
    Stop("clean shutdown");
    g_enabled = false;
}

} // namespace Playerbot
