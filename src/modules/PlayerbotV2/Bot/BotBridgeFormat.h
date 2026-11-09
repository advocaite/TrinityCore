// BotBridgeFormat - the text side of the optional bot bridge (Playerbot.Bridge.*, see BotBridge.h).
//
// Builds the JSON lines the server writes to the events file and reads the JSON lines an outside program writes to the replies
// file. Holds no game state and includes no game headers, so it is unit tested on its own (tests/game/ClassicBotBridge.cpp).
//
// The format is one JSON object per line ("JSON lines"). Events are flat objects with string and number values plus a nested "pos"
// array, a nested "quest" object and (chat_in, stuck) a "near" array. Replies are flat objects with string and number values only;
// anything else in a reply line (nested objects, arrays) is skipped, so a later version can add fields without breaking an older
// server.
//
// A reply may carry one action ("do", Playerbot.Bridge.Actions.*): a word from a fixed list (Act below) with a few plain fields.
// This file only reads it and holds the rules that need no game state (which action may run where, the rate limits, the "near"
// reference numbers); BotBridge.cpp checks the game side and runs it.

#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Playerbot::Bridge {

// Escapes `s` for a JSON string value (quotes, backslash, control characters). UTF-8 passes through as it is.
std::string JsonEscape(std::string const& s);

struct Event
{
    std::uint64_t id = 0;
    std::string   time;          // UTC, "2026-11-08T10:20:05Z"
    std::string   type;          // chat_in, stuck, quest_fail, spell_fail, death_loop, start, stop, dropped
    std::string   bot;           // empty for server events (start, stop, dropped)
    std::uint64_t guid = 0;
    std::string   cls;           // "warrior"
    std::string   race;          // "dwarf"
    std::uint32_t level = 0;
    std::string   zone;
    std::string   area;
    std::uint32_t map = 0;
    float         x = 0, y = 0, z = 0;
    bool          has_pos = false;
    std::uint32_t quest_id = 0;  // 0: no "quest" object
    std::string   quest_name;
    std::string   detail;        // what went wrong, in words
    // Extra string fields in the order given (chat_in: channel, from, text, prompt; spell_fail: spell, reason; start: last_crash ...).
    std::vector<std::pair<std::string, std::string>> fields;
    // Extra number fields.
    std::vector<std::pair<std::string, std::int64_t>> numbers;
    // Extra fields whose value is already JSON (an array or object, written as it is): "near".
    std::vector<std::pair<std::string, std::string>> raw;
};

// One JSON line, no trailing newline.
std::string BuildEventLine(Event const& e);

// A flat JSON object: string values as they are (unescaped), numbers and true/false/null as their text.
using FlatObject = std::map<std::string, std::string>;

// Parses one line holding one JSON object. Nested objects and arrays are skipped (not an error). False with `error` set when the
// line is not a JSON object.
bool ParseFlatObject(std::string const& line, FlatObject& out, std::string& error);

// The actions a reply can ask for. Each is something a player can already order a bot to do; nothing else exists.
enum class Act : std::uint8_t
{
    None = 0,
    Follow, Stay, Resume, Attack, StopAttack, Flee, GoTo,
    AcceptQuest, TurnIn, DropQuest, ShareQuest,
    Unstuck, Sit, Stand, Mount, Dismount, Emote,
    Cast,
    Order,      // "order": one of the safe words of the bot command parser (SafeOrder), run as if the asking player typed it
    Trade,      // open a trade with the asking player and put in things from the bot's bags ("items": what he asked for, or empty)
    Step,       // move a few yards and wait there: "dir" back / forward / left / right (as the asking player faces) or away / closer
                // (from him), "yards" 1-20 (default 5). "step back", "move left", "get out of the fire".
    SignCharter, // sign the guild charter the asking player carries ("can you sign my charter?"), as his Request Signature would
    Count
};

// "follow", "stop_attack", ... ("" for None).
char const* ActName(Act a);
// The action for a word (case-insensitive), None when the word is not on the list.
Act ParseAct(std::string const& word);

// The emote words a reply may use and the animation they play (EMOTE_ONESHOT_* / EMOTE_STATE_DANCE ids). 0: not on the list.
std::uint32_t EmoteId(std::string const& word);

struct Action
{
    Act           act = Act::None;
    std::string   target;        // follow / flee / go_to: speaker (default) or owner; attack: speaker_target (default)
    std::uint32_t ref = 0;       // a "ref" number from an event's "near" list
    std::uint32_t quest = 0;     // accept_quest / drop_quest / turn_in / share_quest
    std::string   emote;         // emote: the word ("wave")
    std::string   spell;         // cast: the spell's name as the chat_in's "spells" list gives it ("Fireball")
    bool          has_point = false;   // go_to: "x" and "y" (and maybe "z") given
    bool          has_z = false;
    float         x = 0, y = 0, z = 0;
    std::string   place;         // go_to: a place in the zone by name ("inn", "mailbox", "flight master", "Goldshire")
    std::string   order;         // order: the parser words, already checked and normalized by SafeOrder
    std::string   items;         // trade: what the player asked for ("linen cloth", "food"); empty: what helps him
    std::string   dir;           // step: back, forward, left, right, away or closer (normalized by ReadAction)
    float         yards = 5.0f;  // step: 1-20
};

// An order a reply may give through the bot command parser: exactly what a player could type to his bot, from a short list of
// harmless ones (moving, mounting, sitting, using an object, buffs and heals, quests, loot, repair, selling grey, gear). Never
// teleports, gold, mail, the auction house, guild or group changes, or anything that deletes. True with the order in lower case
// and single spaces in `norm`; false with the reason in `why`.
bool SafeOrder(std::string const& raw, std::string& norm, std::string& why);

// What kind of place a go_to "place" names: a kind of NPC or object found by its role, or Name (an NPC, an object or an area of
// the zone found by its name). `rest` gets the words left once the kind and the small words are taken out ("the inn at
// goldshire" -> Innkeeper, "goldshire"; "goldshire" -> Name, "goldshire").
enum class PlaceKind : std::uint8_t
{
    Name, Innkeeper, Mailbox, FlightMaster, Vendor, Repair, Banker, Trainer, StableMaster, Auctioneer, SpiritHealer
};
PlaceKind ParsePlace(std::string const& place, std::string& rest);

struct Reply
{
    std::uint64_t reply_to = 0;  // the chat_in event id this answers; 0: a line the bot says on its own
    std::string   bot;           // the bot's name (needed when reply_to is 0)
    std::string   say;           // the text
    std::string   channel;       // say, party, raid, whisper, yell (empty: the channel of the chat_in, or say)
    std::string   to;            // whisper target when channel is whisper and reply_to is 0
    bool          ping = false;  // {"ping":1}: the program is alive, nothing to say
    Action        action;        // "do" and its fields; act None when there is no "do"
    std::string   action_error;  // "do" was there but can't be used (unknown word, a field missing): the text is still said
    std::int32_t  delay_ms = -1; // "delay_ms": how long the bot waits before the text shows (the program's typing time); the
                                 // action never waits. -1: not given, the server's own short typing time is used. At most 10000.
};

// Reads a reply line. False with `error` set when it is not a usable reply (not JSON, no text and no "do", no bot and no reply_to).
bool ParseReplyLine(std::string const& line, Reply& out, std::string& error);

// Splits newly read file bytes into complete lines. `carry` keeps a last line that has no newline yet (the program is still writing
// it) for the next read. Empty lines and a trailing '\r' are dropped.
std::vector<std::string> SplitLines(std::string& carry, std::string const& chunk);

// Rate limits for events. Not thread safe (the bridge calls it from the world thread only).
class EventGate
{
public:
    // Same bot and same event type: at most once every `per_bot_type_ms`. Whole server: at most `per_second` events in any second.
    EventGate(std::uint32_t per_bot_type_ms, std::uint32_t per_second) : per_bot_type_ms_(per_bot_type_ms), per_second_(per_second) { }
    void SetLimits(std::uint32_t per_bot_type_ms, std::uint32_t per_second) { per_bot_type_ms_ = per_bot_type_ms; per_second_ = per_second; }

    // True when the event may be written now; counts it, and sets `repeats` (when given) to how many events of this bot and type
    // the per-bot limit held back since the last one written. False: either a repeat inside the per-bot limit (counted for the
    // next allowed event and in TotalRepeats, not as dropped) or over the whole-server limit (counted in TakeDropped).
    bool Allow(std::uint64_t bot, std::string const& type, std::uint64_t now_ms, std::uint32_t* repeats = nullptr);
    // The events refused by the whole-server limit since the last call.
    std::uint32_t TakeDropped() { std::uint32_t d = dropped_; dropped_ = 0; return d; }
    // Repeats held back by the per-bot limit since the start.
    std::uint64_t TotalRepeats() const { return total_repeats_; }

private:
    std::uint32_t per_bot_type_ms_;
    std::uint32_t per_second_;
    struct Last { std::uint64_t ms = 0; std::uint32_t repeats = 0; };
    std::unordered_map<std::string, Last> last_;             // "bot/type" -> when it was last allowed, repeats held back since
    std::deque<std::uint64_t> second_;                       // allowed events in the last 1000 ms
    std::uint32_t dropped_ = 0;
    std::uint64_t total_repeats_ = 0;
};

// Counts repeats of the same thing (a spell failing, a bot dying in one place) inside a time window.
// Not thread safe (world thread only).
class RepeatCounter
{
public:
    // Notes one hit for `key` at `now_ms` and returns how many hits `key` had in the last `window_ms` (this one included).
    std::uint32_t Hit(std::string const& key, std::uint64_t now_ms, std::uint64_t window_ms);
    void Reset(std::string const& key) { hits_.erase(key); }
    std::size_t Size() const { return hits_.size(); }

private:
    std::unordered_map<std::string, std::deque<std::uint64_t>> hits_;
};

// Turns a reply's text into one chat line: first line only, '|' removed (the client's escape character), control characters removed,
// cut to `max_bytes` on a word boundary without splitting a UTF-8 character. Empty when nothing is left.
std::string CleanSayText(std::string const& raw, std::size_t max_bytes);

// ---- actions: the rules that need no game state ----

// What the game side found out about the bot and the one asking. Filled by BotBridge.cpp, read by CheckActionContext.
struct ActionContext
{
    bool owned = false;              // the bot has an owner (a player's companion)
    bool has_speaker = false;        // the action answers a chat_in, so a player asked for it
    bool speaker_is_owner = false;   // that player owns the bot
    bool speaker_controls = false;   // that player may give the bot group orders (its owner, or the human leading its group)
    bool grouped_with_human = false; // the bot is in a group with a real player
    bool in_instance = false;        // dungeon, raid, battleground or arena
    bool in_combat = false;
    bool owned_bots_allowed = false; // Playerbot.Bridge.Actions.OwnedBots: the program may act for a companion on its own
};

// Nullptr when `a` may run in this context, else why not (in words, for the action_result event). The rules:
//  - a companion acts only when its owner asked (or, with OwnedBots, when the program acts on its own);
//  - a bot grouped with a real player takes anything but emote / sit / stand only from the player leading that group;
//  - an order, a trade, and a step in a dungeon or a fight, only from the player who owns the bot or leads its group;
//  - in a dungeon, raid, battleground or arena only follow, stay, resume, emote, sit, stand, order, trade and step (the bots' own
//    logic leads there);
//  - in combat only attack, stop_attack, flee, follow, stay, cast, order and step.
char const* CheckActionContext(Act a, ActionContext const& ctx);

// The actions `ctx` lets run, as a JSON array of words (["follow","stay",...]) for the chat_in's "can" field, so the program
// knows before it answers what this bot may do for this player right now. `allowed` is ParseAllowedActions' result.
std::string AllowedActsJson(ActionContext const& ctx, std::vector<bool> const& allowed);

// The list of allowed actions from the config ("follow,stay,..." or "all"); unknown words are ignored.
std::vector<bool> ParseAllowedActions(std::string const& list);

// Rate limits for actions. Not thread safe (world thread only).
class ActionGate
{
public:
    void SetLimits(std::uint32_t bot_gap_ms, std::uint32_t per_second) { bot_gap_ms_ = bot_gap_ms; per_second_ = per_second; }

    // Nullptr when the action may run now (it is counted), else why not. Per bot: one action every bot_gap_ms, one emote every
    // 10 s, one unstuck every 2 minutes; a bot with 10 refused actions in a minute takes none for 5 minutes. Whole server:
    // per_second actions in any second.
    char const* Allow(std::uint64_t bot, Act a, std::uint64_t now_ms);
    // An action of this bot was refused (by the checks, or by Allow): counts towards the 5 minute pause.
    void NoteRefused(std::uint64_t bot, std::uint64_t now_ms);

private:
    struct PerBot
    {
        std::uint64_t last_ms = 0, last_emote_ms = 0, last_unstuck_ms = 0, paused_until_ms = 0;
        std::deque<std::uint64_t> refused;
    };
    std::uint32_t bot_gap_ms_ = 5000;
    std::uint32_t per_second_ = 20;
    std::unordered_map<std::uint64_t, PerBot> bots_;
    std::deque<std::uint64_t> second_;
};

// The "ref" numbers of the "near" lists: each thing an event names near a bot gets a number that stays valid for `ttl_ms`, so a
// reply can point at it ("ref":3) without ever sending a game id. Numbers are per bot and only grow. Not thread safe.
template <class Id>
class RefTable
{
public:
    explicit RefTable(std::uint64_t ttl_ms = 60000) : ttl_ms_(ttl_ms) { }

    struct Entry { std::uint32_t ref = 0; Id id{}; std::uint8_t kind = 0; std::uint64_t made_ms = 0; };

    std::uint32_t Add(std::uint64_t bot, Id const& id, std::uint8_t kind, std::uint64_t now_ms)
    {
        Bot& b = bots_[bot];
        while (!b.entries.empty() && (now_ms - b.entries.front().made_ms > ttl_ms_ || b.entries.size() >= kMaxPerBot))
            b.entries.pop_front();
        Entry e;
        e.ref = ++b.next;
        e.id = id;
        e.kind = kind;
        e.made_ms = now_ms;
        b.entries.push_back(e);
        return e.ref;
    }

    // The entry for `ref` of this bot while it is valid, else nullptr.
    Entry const* Find(std::uint64_t bot, std::uint32_t ref, std::uint64_t now_ms) const
    {
        auto it = bots_.find(bot);
        if (it == bots_.end() || ref == 0)
            return nullptr;
        for (Entry const& e : it->second.entries)
            if (e.ref == ref)
                return now_ms - e.made_ms <= ttl_ms_ ? &e : nullptr;
        return nullptr;
    }

    // Forgets bots whose newest entry has run out (keeps the table small).
    void Prune(std::uint64_t now_ms)
    {
        std::erase_if(bots_, [&](auto const& kv) { return kv.second.entries.empty() || now_ms - kv.second.entries.back().made_ms > ttl_ms_; });
    }
    std::size_t Bots() const { return bots_.size(); }

private:
    static constexpr std::size_t kMaxPerBot = 64;
    struct Bot { std::uint32_t next = 0; std::deque<Entry> entries; };
    std::uint64_t ttl_ms_;
    std::unordered_map<std::uint64_t, Bot> bots_;
};

// One thing in an event's "near" list.
struct NearItem
{
    std::uint32_t ref = 0;
    std::string   kind;     // quest_giver, npc, hostile
    std::string   name;
    std::uint32_t level = 0;
    float         dist = 0;
    std::uint32_t count = 1;  // hostile: how many of that name are in range (the nearest one is listed)
    std::vector<std::pair<std::uint32_t, std::string>> quests;   // quest_giver: what it offers this bot (id, title)
};

// The JSON array for "near".
std::string BuildNearJson(std::vector<NearItem> const& items);

// One spell in a chat_in's "spells" list: what the bot can cast for a player who asks ("show me Fireball", "heal me").
struct SpellItem
{
    std::string name;
    std::string kind;   // damage, heal, buff, self, resurrect
};

// The JSON array for "spells": [{"name":"Fireball","kind":"damage"},...].
std::string BuildSpellsJson(std::vector<SpellItem> const& spells);

// The key two spell names are compared by: lower case, letters, digits and spaces only, single spaces, a trailing "(rank N)" or
// "rank N" dropped. "Power Word: Fortitude (Rank 2)" and "power word fortitude" give the same key.
std::string SpellNameKey(std::string const& name);

// "events-20261108-1000.jsonl" for the hour holding `utc_seconds`.
std::string EventsFileName(std::int64_t utc_seconds);

// "2026-11-08T10:20:05Z".
std::string IsoTime(std::int64_t utc_seconds);

} // namespace Playerbot::Bridge
