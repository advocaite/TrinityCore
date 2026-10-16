// BotTextMgr - the bots' speech lines, carried over from Karrz's ike3 playerbot build.
//
// Data: {Playerbot.SharedDatabase}.playerbot_v2_bot_text / _bot_text_chance (migration 0019). Loaded
// once by Module::Init before any bot logs in; read-only afterwards, so the bot workers and the world
// thread may read it concurrently without a lock.
//
// A line is picked at random among the rows of one key ('hello', 'goodbye', 'reply',
// 'broadcast_looting_item_rare', ...), weighted by the row's `weight` (migration 0022: ike3 listed a
// line twice to make it twice as likely), and its ike3 %placeholders are filled from the pairs the
// caller passes. Placeholders the caller cannot fill are left as they are, exactly as ike3 did.
//
// BotChatter (below) is the event side: it turns a game event (loot, quest progress, level-up, group
// join, dismissal, an emote) into one ChatIntent pushed on the bot's intent queue. It never touches a
// Player beyond reading it; the line is spoken by the executor on the world thread like any other
// bot action. Unsolicited lines are chance-gated per key, rate-limited per group and per bot, and
// switched/scaled by config:
//
//   Playerbot.Chatter.Enabled         1   0 silences everything below (greetings, goodbyes,
//                                         broadcasts, ike3 replies, emote answers)
//   Playerbot.Chatter.ChanceScale   1.0   multiplies every per-key chance (0.5 = half as chatty)
//   Playerbot.Chatter.GroupCooldownMs 20000  minimum gap between two unsolicited lines in one group
//   Playerbot.Chatter.BotCooldownMs   60000  minimum gap between two unsolicited lines from one bot
//
// SpeechTick (below) is the per-tick side, run by the bot worker from the snapshot: ike3's SayStrategy
// combat callouts for companions (critical / low health, low mana, out of ammo, the tank's taunt
// lines, the "so many mobs" line on a big pull) and SuggestWhatToDoAction's lines from world bots in
// their zone's General / Trade channel. It emits ChatIntents like any other rule.
//
//   Playerbot.Chatter.Callouts.Enabled         1   companions' combat / ammo lines
//   Playerbot.Chatter.Callouts.KeyCooldownMs  60000  one bot says the same kind of callout at most this often
//   Playerbot.Chatter.Callouts.GroupCooldownMs 8000  gap between two callouts in one group (heal / ammo
//                                                 requests are exempt: they are never drowned by banter)
//   Playerbot.Chatter.Suggest.Enabled          1   world bots' channel suggestions
//   Playerbot.Chatter.Suggest.BotCooldownMs  1800000 one world bot suggests at most once per 30 min
//   Playerbot.Chatter.Suggest.ChannelCooldownMs 180000 one line per zone General (or the city Trade) per 3 min
//   Playerbot.Chatter.Suggest.GlobalCooldownMs  10000  realm-wide gap between two channel lines
//   Playerbot.Chatter.GlobalMaxPerMinute       60   realm-wide cap on the lines above (callouts + suggestions)

#pragma once

#include "BotTypes.h"
#include "Define.h"
#include "ObjectGuid.h"

#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

class Creature;
class Player;
class Quest;
struct ItemTemplate;

namespace Playerbot {

class BotAI;
class BotIntentEmitter;
class BotSnapshotView;

using TextPlaceholders = std::vector<std::pair<std::string, std::string>>;

// ike3 ChatReplyType (reply_type column of the 'reply' rows).
enum class ReplyType : uint8
{
    NotUnderstand = 0,
    Grudge        = 1,
    Victim        = 2,
    Attacker      = 3,
    Hello         = 4,
    Name          = 5,
    AdminAbuse    = 6,
};

class BotTextMgr
{
public:
    struct Line
    {
        std::string text;
        bool        yell = false;   // say_type 1
    };

    static BotTextMgr& instance();

    // Reads config + both tables. World thread, at module init.
    void Load();

    bool   Enabled() const        { return enabled_; }
    uint32 GroupCooldownMs() const { return group_cooldown_ms_; }
    uint32 BotCooldownMs() const   { return bot_cooldown_ms_; }
    size_t Count() const           { return count_; }

    struct SpeechConfig
    {
        bool   callouts_enabled          = true;
        uint32 callout_key_cooldown_ms   = 60000;
        uint32 callout_group_cooldown_ms = 8000;
        bool   suggest_enabled           = true;
        uint32 suggest_bot_cooldown_ms   = 1800000;
        uint32 suggest_channel_cooldown_ms = 180000;
        uint32 suggest_global_cooldown_ms  = 10000;
        uint32 global_max_per_minute     = 60;
    };
    SpeechConfig const& Speech() const { return speech_; }

    bool HasKey(std::string_view key) const;

    // Weighted random line for `key` with placeholders filled. Empty when the key has no rows.
    Line        Pick(std::string_view key, TextPlaceholders const& ph = {}) const;
    std::string Get(std::string_view key, TextPlaceholders const& ph = {}) const { return Pick(key, ph).text; }

    // Random 'reply' row of the given type, %s filled with `other_name`. Empty when none.
    std::string GetReply(ReplyType type, std::string const& other_name) const;

    // Rolls the key's chance (percent, scaled by ChanceScale). A key without a chance row always passes.
    bool RollChance(std::string_view key) const;

    static void Fill(std::string& text, TextPlaceholders const& ph);

private:
    struct Row
    {
        std::string text;
        uint8       say_type = 0;
        uint8       reply_type = 0;
        uint32      weight = 1;
    };

    // Weighted pick among `rows` (those passing `accept`); null when none qualifies.
    template <typename Accept>
    static Row const* PickWeighted(std::vector<Row> const& rows, Accept&& accept);

    std::unordered_map<std::string, std::vector<Row>> texts_;
    std::unordered_map<std::string, float>            chance_;
    size_t count_ = 0;
    bool   enabled_ = true;
    float  chance_scale_ = 1.0f;
    uint32 group_cooldown_ms_ = 20000;
    uint32 bot_cooldown_ms_ = 60000;
    SpeechConfig speech_;
};

namespace BotChatter {

// ike3 ChatReplyAction::GenerateReplyMessage: an answer to a line the bot could not parse as a
// command or a social keyword - a question gets a question answer, the bot's name a "yes?" line,
// anything else a "what?" line. Empty when the text table is not loaded.
std::string GenerateReply(std::string const& bot_name, std::string const& msg, std::string const& sender_name);

// %my_class / %my_race / %my_level / %zone_name / %area_name for a bot (DB2 lookups only, any thread).
void AddSelfPlaceholders(TextPlaceholders& ph, uint8 cls, uint8 race, uint8 level, uint32 zone_id, uint32 area_id);

// Rate limiter for unsolicited lines. Greetings use a short per-group window so a `.bot call all`
// of ten companions produces one or two hellos, not ten; broadcasts use the configured cooldowns.
// Level-up lines share the short per-group window too: a group that dings together says it once.
enum class Slot : uint8 { Greet, LevelUp, Broadcast };
bool TakeSlot(Slot slot, uint64 group_low, BotId bot, uint32 now_ms);

// ---- lines for the bot worker (snapshot data only; the caller emits them as party chat) ----

// ike3 'hello' (or 'hello_follow' for a companion joining its owner). Empty when chatter is off or
// another bot of the group greeted moments ago.
std::string GreetLine(bool follow_owner, uint64 group_low, BotId bot, uint32 now_ms);
// ike3 broadcast_levelup_generic / _10x / _max_level, chance-gated. Empty when nothing is to be said.
std::string LevelUpLine(uint8 level, uint8 cls, uint8 race, uint32 zone_id, uint32 area_id,
                        uint64 group_low, BotId bot, uint32 now_ms);

// ---- event entry points (world/map thread; each pushes at most one ChatIntent) ----

// A companion (bot grouped with a real player) stored an item from loot.
void OnItemLooted(Player* bot, uint32 item_id);
// Quest accepted / all objectives complete / failed / turned in.
void OnQuestStatusChange(Player* bot, uint32 quest_id);
// The bot got the killing blow credit on a creature.
void OnCreatureKilled(Player* bot, Creature* killed);
// The bot killed an enemy player (ike3 broadcast_killed_player).
void OnPlayerKilled(Player* bot, Player* killed);
// `.bot dismiss`: the companion whispers its owner goodbye. The line is pushed undeferred and is
// flushed by Module::OnPlayerLogout (FlushPendingChat) before the bot leaves the world.
void SayGoodbye(Player* bot, Player* owner);

// ---- party banter (Playerbot.Behavior.PartyBanter) and social emotes (Playerbot.Behavior.SocialEmotes) ----
//
// Low-frequency, human-sounding remarks from companions (a bot grouped with its owner): "ready" on a ready
// check it passes, "ouch" when it dies, "my bad" / "wp" on a dungeon wipe, "ty" when resurrected, "gg" on a boss
// kill. Every line goes through one limiter: chatter on, a Normal+ personality, a group of at most 15, a roll
// (about 1 trigger in 3), the group gap (Playerbot.Chatter.GroupCooldownMs, shared with the ike3 broadcasts) and
// a 90 s gap per bot. The text is a small built-in pool varied by class, role and personality.
enum class Banter : uint8 { Ready, Wipe, Thanks, Ouch, BossKill };

// bot worker: the line to say now for `kind`, or empty (gates / roll / cooldown said no). The caller emits it.
std::string BanterLine(Banter kind, BotAI& ai, BotSnapshotView const& s);
// A boss (dungeon boss or world boss) died to `killer` or a group member. Called by the kill listener for any
// killer. One nearby companion of the killer's group may say "gg" and one may bow (the killer is a real player,
// "the owner finished the boss") or cheer.
void OnBossKilled(Player* killer, Creature* boss);

// ---- per-tick speech (bot worker; snapshot data only; emits at most one ChatIntent per tick) ----

// ike3 SayStrategy + SuggestWhatToDoAction, see the header comment. Called from
// BotAI::dispatch_layers every tick, whatever the bot's state.
void SpeechTick(BotAI& ai, BotSnapshotView const& s, BotIntentEmitter& em);

// Executes the bot's queued ChatIntents now and drops everything else. Called only while the bot
// is logging out (world thread), so a goodbye pushed by the dismissal is still heard. Defined in
// BotIntentExecutor.cpp next to the intent visitor.
size_t FlushPendingChat(Player* bot);

} // namespace BotChatter

} // namespace Playerbot
