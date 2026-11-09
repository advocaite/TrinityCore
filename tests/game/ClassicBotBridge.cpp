/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "tc_catch2.h"

#ifdef TESTS_HAVE_PLAYERBOT_V2

#include "../../src/modules/PlayerbotV2/Bot/BotBridgeFormat.h"
#include <string>

using namespace Playerbot::Bridge;

TEST_CASE("BotBridge: an event line carries the bot, its place, its quest and what went wrong", "[BotBridge]")
{
    Event e;
    e.id = 1042;
    e.time = "2026-11-08T10:20:05Z";
    e.type = "stuck";
    e.bot = "Brannor";
    e.guid = 1234;
    e.cls = "warrior";
    e.race = "dwarf";
    e.level = 14;
    e.zone = "Loch Modan";
    e.area = "Thelsamar";
    e.map = 0;
    e.x = -5340.12f; e.y = -2950.3f; e.z = 323.9f;
    e.has_pos = true;
    e.quest_id = 1920;
    e.quest_name = "Blue \"Recluse\"";
    e.detail = "no progress\nfor 300 s";
    e.fields.emplace_back("killer", "Elder Mottled Boar");
    e.numbers.emplace_back("spell_id", 78);
    CHECK(BuildEventLine(e) ==
          "{\"id\":1042,\"t\":\"2026-11-08T10:20:05Z\",\"type\":\"stuck\",\"bot\":\"Brannor\",\"guid\":1234,\"class\":\"warrior\","
          "\"race\":\"dwarf\",\"level\":14,\"zone\":\"Loch Modan\",\"area\":\"Thelsamar\",\"map\":0,\"pos\":[-5340.1,-2950.3,323.9],"
          "\"quest\":{\"id\":1920,\"name\":\"Blue \\\"Recluse\\\"\"},\"detail\":\"no progress\\nfor 300 s\",\"killer\":\"Elder Mottled Boar\","
          "\"spell_id\":78}");

    Event s;
    s.id = 1;
    s.time = "t";
    s.type = "start";
    s.numbers.emplace_back("unclean_stop", 0);
    CHECK(BuildEventLine(s) == "{\"id\":1,\"t\":\"t\",\"type\":\"start\",\"unclean_stop\":0}");
}

TEST_CASE("BotBridge: every event line reads back as JSON", "[BotBridge]")
{
    Event e;
    e.id = 7;
    e.type = "chat_in";
    e.bot = "Ольга";
    e.fields.emplace_back("text", std::string("tab\there, \\ back, \x01 ctrl"));
    FlatObject o;
    std::string err;
    REQUIRE(ParseFlatObject(BuildEventLine(e), o, err));
    CHECK(o["bot"] == "Ольга");
    CHECK(o["text"] == std::string("tab\there, \\ back, \x01 ctrl"));
    CHECK(o["id"] == "7");
}

TEST_CASE("BotBridge: reply lines", "[BotBridge]")
{
    Reply r;
    std::string err;
    REQUIRE(ParseReplyLine("{\"reply_to\":1042,\"bot\":\"Brannor\",\"say\":\"Going around.\",\"channel\":\"Party\"}", r, err));
    CHECK(r.reply_to == 1042);
    CHECK(r.say == "Going around.");
    CHECK(r.channel == "party");

    // A line the bot says on its own needs the bot's name.
    REQUIRE(ParseReplyLine("{\"bot\":\"Brannor\",\"say\":\"Anyone up for Deadmines?\"}", r, err));
    CHECK(r.reply_to == 0);
    CHECK(r.channel.empty());
    CHECK(r.delay_ms == -1);
    CHECK_FALSE(ParseReplyLine("{\"say\":\"who says this?\"}", r, err));

    // delay_ms: the program's typing time, at most 10 s.
    REQUIRE(ParseReplyLine("{\"reply_to\":7,\"say\":\"On it.\",\"do\":\"follow\",\"delay_ms\":1400}", r, err));
    CHECK(r.delay_ms == 1400);
    CHECK(r.action.act == Act::Follow);
    REQUIRE(ParseReplyLine("{\"reply_to\":7,\"say\":\"ok\",\"delay_ms\":0}", r, err));
    CHECK(r.delay_ms == 0);
    REQUIRE(ParseReplyLine("{\"reply_to\":7,\"say\":\"ok\",\"delay_ms\":999999}", r, err));
    CHECK(r.delay_ms == 10000);

    // Unknown fields and nested values are skipped, so a newer program still works.
    REQUIRE(ParseReplyLine(" {\"reply_to\": 5, \"say\": \"ok\", \"action\": {\"type\": \"follow\", \"list\": [1, {\"a\": \"}\"}]}, \"x\": null} ", r, err));
    CHECK(r.reply_to == 5);

    // \u escapes, a surrogate pair included.
    REQUIRE(ParseReplyLine("{\"reply_to\":1,\"say\":\"\\u041f\\u0440\\u0438\\u0432\\u0435\\u0442 \\ud83d\\ude00\"}", r, err));
    CHECK(r.say == "Привет \xF0\x9F\x98\x80");

    REQUIRE(ParseReplyLine("{\"ping\":1}", r, err));
    CHECK(r.ping);

    CHECK_FALSE(ParseReplyLine("not json", r, err));
    CHECK_FALSE(ParseReplyLine("{\"reply_to\":1,\"say\":\"broken", r, err));
    CHECK_FALSE(ParseReplyLine("{\"reply_to\":1,\"say\":\"\"}", r, err));
    CHECK_FALSE(ParseReplyLine("{\"reply_to\":1,\"say\":\"hi\",\"channel\":\"guild\"}", r, err));
    CHECK_FALSE(ParseReplyLine("{\"bot\":\"Brannor\",\"say\":\"hi\",\"channel\":\"whisper\"}", r, err));   // whisper to whom?
    REQUIRE(ParseReplyLine("{\"bot\":\"Brannor\",\"say\":\"hi\",\"channel\":\"whisper\",\"to\":\"Karrz\"}", r, err));
    CHECK(r.to == "Karrz");
    // Deep nesting is refused, not a stack overflow.
    CHECK_FALSE(ParseReplyLine("{\"reply_to\":1,\"say\":\"x\",\"a\":" + std::string(100, '[') + std::string(100, ']') + "}", r, err));
}

TEST_CASE("BotBridge: lines are split only at a newline; a half-written line waits", "[BotBridge]")
{
    std::string carry;
    auto a = SplitLines(carry, "{\"a\":1}\r\n\n{\"b\":");
    REQUIRE(a.size() == 1);
    CHECK(a[0] == "{\"a\":1}");
    CHECK(carry == "{\"b\":");
    auto b = SplitLines(carry, "2}\n");
    REQUIRE(b.size() == 1);
    CHECK(b[0] == "{\"b\":2}");
    CHECK(carry.empty());
}

TEST_CASE("BotBridge: rate limits per bot and type and for the whole server", "[BotBridge]")
{
    EventGate g(300000, 3);
    CHECK(g.Allow(1, "stuck", 1500));
    CHECK_FALSE(g.Allow(1, "stuck", 2000));          // same bot, same type, inside 5 minutes
    CHECK(g.Allow(1, "spell_fail", 2000));           // another type
    CHECK(g.Allow(2, "stuck", 2000));                // another bot
    CHECK_FALSE(g.Allow(3, "stuck", 2100));          // 3 in this second already
    CHECK(g.Allow(3, "stuck", 3100));                // a new second
    std::uint32_t repeats = 99;
    CHECK(g.Allow(1, "stuck", 301500, &repeats));    // 5 minutes later
    CHECK(repeats == 1);                             // the held-back repeat at 2000 is told with this one
    CHECK(g.TakeDropped() == 1);                     // only the whole-server limit counts as dropped
    CHECK(g.TakeDropped() == 0);
    CHECK(g.TotalRepeats() == 1);
    CHECK(g.Allow(2, "spell_fail", 302000, &repeats));
    CHECK(repeats == 0);
}

TEST_CASE("BotBridge: repeats inside a window", "[BotBridge]")
{
    RepeatCounter c;
    CHECK(c.Hit("1/78", 0, 60000) == 1);
    CHECK(c.Hit("1/78", 10000, 60000) == 2);
    CHECK(c.Hit("1/78", 65000, 60000) == 2);         // the first one left the window
    CHECK(c.Hit("2/78", 65000, 60000) == 1);
    c.Reset("1/78");
    CHECK(c.Hit("1/78", 65001, 60000) == 1);
}

TEST_CASE("BotBridge: reply text becomes one safe chat line", "[BotBridge]")
{
    CHECK(CleanSayText("  hello |cffff0000there|r  \nsecond line", 200) == "hello cffff0000therer");
    CHECK(CleanSayText("\n\nfirst", 200) == "first");
    CHECK(CleanSayText("   ", 200).empty());
    CHECK(CleanSayText("one two three four", 10) == "one two");
    // Never cut inside a UTF-8 character.
    std::string const ru = "Привет мир";   // 2 bytes a letter
    std::string const cut = CleanSayText(ru, 7);
    CHECK(cut == "При");
}

TEST_CASE("BotBridge: file names and times are UTC", "[BotBridge]")
{
    // 2026-11-08 10:20:05 UTC
    CHECK(IsoTime(1794133205) == "2026-11-08T10:20:05Z");
    CHECK(EventsFileName(1794133205) == "events-20261108-1000.jsonl");
}

TEST_CASE("BotBridge: a reply may carry one action from the fixed list", "[BotBridge]")
{
    Reply r;
    std::string err;
    REQUIRE(ParseReplyLine("{\"reply_to\":9,\"say\":\"Right behind you.\",\"do\":\"Follow\"}", r, err));
    CHECK(r.action.act == Act::Follow);
    CHECK(r.action_error.empty());

    // An action alone, without text.
    REQUIRE(ParseReplyLine("{\"bot\":\"Brannor\",\"do\":\"go_to\",\"x\":-5340.1,\"y\":-2950.3}", r, err));
    CHECK(r.action.act == Act::GoTo);
    CHECK(r.action.has_point);
    CHECK_FALSE(r.action.has_z);
    CHECK(r.action.x == Catch::Approx(-5340.1f));
    REQUIRE(ParseReplyLine("{\"reply_to\":9,\"do\":\"attack\",\"ref\":3}", r, err));
    CHECK(r.action.act == Act::Attack);
    CHECK(r.action.ref == 3);
    REQUIRE(ParseReplyLine("{\"reply_to\":9,\"do\":\"accept_quest\",\"quest\":1920}", r, err));
    CHECK(r.action.quest == 1920);
    REQUIRE(ParseReplyLine("{\"reply_to\":9,\"do\":\"emote\",\"emote\":\"Wave\"}", r, err));
    CHECK(r.action.emote == "wave");
    CHECK(EmoteId(r.action.emote) == 3);

    // Anything off the list is not an action: the text is still said, and the program is told why.
    REQUIRE(ParseReplyLine("{\"reply_to\":9,\"say\":\"sure\",\"do\":\"gm_teleport\"}", r, err));
    CHECK(r.action.act == Act::None);
    CHECK_FALSE(r.action_error.empty());
    REQUIRE(ParseReplyLine("{\"reply_to\":9,\"do\":\"mail\"}", r, err));   // no text: still read, so the refusal can be reported
    CHECK(r.action.act == Act::None);
    CHECK_FALSE(r.action_error.empty());
    for (char const* bad : { "\"do\":\"accept_quest\"", "\"do\":\"emote\",\"emote\":\"spit\"", "\"do\":\"go_to\"",
                             "\"do\":\"follow\",\"target\":\"Karrz\"", "\"do\":\"attack\",\"target\":\"Karrz\"",
                             "\"do\":\"go_to\",\"x\":\"nan\",\"y\":1" })
    {
        REQUIRE(ParseReplyLine(std::string("{\"reply_to\":9,") + bad + "}", r, err));
        CHECK(r.action.act == Act::None);
        CHECK_FALSE(r.action_error.empty());
    }
    // A command line in "do" is just an unknown word.
    REQUIRE(ParseReplyLine("{\"bot\":\"Brannor\",\"do\":\".gm on\"}", r, err));
    CHECK(r.action.act == Act::None);

    CHECK(ParseAct("goto") == Act::GoTo);
    CHECK(ParseAct("stop") == Act::StopAttack);
    CHECK(ParseAct("delete") == Act::None);
    CHECK(std::string(ActName(Act::StopAttack)) == "stop_attack");
}

TEST_CASE("BotBridge: who may make a bot act, and where", "[BotBridge]")
{
    ActionContext world;   // a world bot on its own
    CHECK(CheckActionContext(Act::Follow, world) == nullptr);
    CHECK(CheckActionContext(Act::Attack, world) == nullptr);

    // A player's companion: only when its owner asked.
    ActionContext comp;
    comp.owned = true;
    comp.has_speaker = true;
    CHECK(CheckActionContext(Act::Follow, comp) != nullptr);        // a stranger asked
    CHECK(CheckActionContext(Act::Emote, comp) != nullptr);
    comp.speaker_is_owner = comp.speaker_controls = true;
    CHECK(CheckActionContext(Act::Follow, comp) == nullptr);
    ActionContext self = comp;
    self.has_speaker = self.speaker_is_owner = self.speaker_controls = false;
    CHECK(CheckActionContext(Act::Stay, self) != nullptr);          // the program on its own
    self.owned_bots_allowed = true;
    CHECK(CheckActionContext(Act::Stay, self) == nullptr);

    // A world bot in a real player's group: only that group's leader, except harmless emotes.
    ActionContext grp;
    grp.grouped_with_human = true;
    grp.has_speaker = true;
    CHECK(CheckActionContext(Act::GoTo, grp) != nullptr);
    CHECK(CheckActionContext(Act::Emote, grp) == nullptr);
    grp.speaker_controls = true;
    CHECK(CheckActionContext(Act::GoTo, grp) == nullptr);

    // Dungeons and fights.
    ActionContext inst;
    inst.in_instance = true;
    CHECK(CheckActionContext(Act::Follow, inst) == nullptr);
    CHECK(CheckActionContext(Act::Attack, inst) != nullptr);
    CHECK(CheckActionContext(Act::AcceptQuest, inst) != nullptr);
    ActionContext fight;
    fight.in_combat = true;
    CHECK(CheckActionContext(Act::Flee, fight) == nullptr);
    CHECK(CheckActionContext(Act::Mount, fight) != nullptr);
    CHECK(CheckActionContext(Act::Emote, fight) != nullptr);
}

TEST_CASE("BotBridge: the allowed actions come from the config", "[BotBridge]")
{
    auto all = ParseAllowedActions("all");
    CHECK(all[std::size_t(Act::Emote)]);
    CHECK_FALSE(all[std::size_t(Act::None)]);
    auto some = ParseAllowedActions(" Follow, stay ;emote nonsense");
    CHECK(some[std::size_t(Act::Follow)]);
    CHECK(some[std::size_t(Act::Stay)]);
    CHECK(some[std::size_t(Act::Emote)]);
    CHECK_FALSE(some[std::size_t(Act::Attack)]);
    CHECK_FALSE(ParseAllowedActions("")[std::size_t(Act::Follow)]);
}

TEST_CASE("BotBridge: action rate limits", "[BotBridge]")
{
    ActionGate g;
    g.SetLimits(5000, 3);
    CHECK(g.Allow(1, Act::Follow, 1000) == nullptr);
    CHECK(g.Allow(1, Act::Stay, 3000) != nullptr);          // 5 s per bot
    CHECK(g.Allow(1, Act::Stay, 6000) == nullptr);
    CHECK(g.Allow(2, Act::Emote, 6000) == nullptr);
    CHECK(g.Allow(3, Act::Emote, 6100) == nullptr);
    CHECK(g.Allow(4, Act::Emote, 6200) != nullptr);         // 3 in this second
    CHECK(g.Allow(2, Act::Emote, 12000) != nullptr);        // one emote per 10 s
    CHECK(g.Allow(2, Act::Emote, 16100) == nullptr);
    CHECK(g.Allow(5, Act::Unstuck, 20000) == nullptr);
    CHECK(g.Allow(5, Act::Unstuck, 30000) != nullptr);      // one unstuck per 2 minutes

    // 10 refusals in a minute: no actions for 5 minutes.
    for (int i = 0; i < 10; ++i)
        g.NoteRefused(7, 40000 + i * 1000);
    CHECK(g.Allow(7, Act::Follow, 60000) != nullptr);
    CHECK(g.Allow(7, Act::Follow, 349001) == nullptr);
}

TEST_CASE("BotBridge: near refs point at one thing for a minute", "[BotBridge]")
{
    RefTable<int> t(60000);
    std::uint32_t const a = t.Add(1, 111, 3, 1000);
    std::uint32_t const b = t.Add(1, 222, 1, 1000);
    std::uint32_t const c = t.Add(2, 333, 3, 1000);
    CHECK(a != b);
    REQUIRE(t.Find(1, a, 30000) != nullptr);
    CHECK(t.Find(1, a, 30000)->id == 111);
    CHECK(t.Find(1, b, 30000)->kind == 1);
    CHECK((t.Find(1, c, 30000) == nullptr || t.Find(1, c, 30000)->id != 333));   // refs are per bot
    CHECK(t.Find(2, c, 30000)->id == 333);
    CHECK(t.Find(1, 99, 30000) == nullptr);
    CHECK(t.Find(1, a, 61001) == nullptr);                  // older than a minute
    t.Prune(200000);
    CHECK(t.Bots() == 0);

    NearItem n;
    n.ref = 4;
    n.kind = "quest_giver";
    n.name = "Mountaineer Kadrell";
    n.dist = 12.4f;
    n.quests.emplace_back(416, "Rat Catching");
    CHECK(BuildNearJson({ n }) ==
          "[{\"ref\":4,\"kind\":\"quest_giver\",\"name\":\"Mountaineer Kadrell\",\"dist\":12,\"quests\":[{\"id\":416,\"name\":\"Rat Catching\"}]}]");
    CHECK(BuildNearJson({}).empty());

    Event e;
    e.id = 1;
    e.time = "t";
    e.type = "stuck";
    e.raw.emplace_back("near", BuildNearJson({ n }));
    FlatObject o;
    std::string err;
    CHECK(ParseFlatObject(BuildEventLine(e), o, err));   // still one valid JSON object
}

TEST_CASE("BotBridge: cast replies name a spell and a target", "[BotBridge]")
{
    Reply r;
    std::string err;
    REQUIRE(ParseReplyLine(R"({"reply_to":7,"say":"watch this","do":"cast","spell":"Fireball","ref":2})", r, err));
    CHECK(r.action.act == Act::Cast);
    CHECK(r.action.spell == "Fireball");
    CHECK(r.action.ref == 2);
    CHECK(r.action_error.empty());

    REQUIRE(ParseReplyLine(R"({"reply_to":7,"say":"there you go","do":"cast_spell","spell":"Power Word: Fortitude","target":"speaker"})", r, err));
    CHECK(r.action.act == Act::Cast);
    CHECK(r.action.target == "speaker");

    REQUIRE(ParseReplyLine(R"({"reply_to":7,"say":"hm","do":"cast"})", r, err));   // no spell: the text is still said
    CHECK(r.action.act == Act::None);
    CHECK_FALSE(r.action_error.empty());

    REQUIRE(ParseReplyLine(R"({"reply_to":7,"say":"hm","do":"cast","spell":"Heal","target":"everyone"})", r, err));
    CHECK(r.action.act == Act::None);
    CHECK_FALSE(r.action_error.empty());

    CHECK(std::string(ActName(Act::Cast)) == "cast");
    CHECK(ParseAllowedActions("cast")[std::size_t(Act::Cast)]);
}

TEST_CASE("BotBridge: spell names compare without case, marks and ranks", "[BotBridge]")
{
    CHECK(SpellNameKey("Power Word: Fortitude") == "power word fortitude");
    CHECK(SpellNameKey("power word fortitude (Rank 3)") == "power word fortitude");
    CHECK(SpellNameKey("Fireball Rank 11") == "fireball");
    CHECK(SpellNameKey("  Flash   Heal ") == "flash heal");
    CHECK(SpellNameKey("Mark of the Wild") != SpellNameKey("Gift of the Wild"));
    CHECK(SpellNameKey("Rank 2").empty() == false);   // a name that is only "rank N" is kept as it is
    CHECK(SpellNameKey("!!!").empty());

    CHECK(BuildSpellsJson({ SpellItem{ "Fireball", "damage" }, SpellItem{ "Arcane Intellect", "buff" } }) ==
          "[{\"name\":\"Fireball\",\"kind\":\"damage\"},{\"name\":\"Arcane Intellect\",\"kind\":\"buff\"}]");
    CHECK(BuildSpellsJson({}) == "[]");
}

TEST_CASE("BotBridge: cast may run in a fight, not in a dungeon", "[BotBridge]")
{
    ActionContext ctx;
    ctx.has_speaker = true;
    CHECK(CheckActionContext(Act::Cast, ctx) == nullptr);
    ctx.in_combat = true;
    CHECK(CheckActionContext(Act::Cast, ctx) == nullptr);   // "heal me!" mid-fight
    ctx.in_combat = false;
    ctx.in_instance = true;
    CHECK(CheckActionContext(Act::Cast, ctx) != nullptr);
}

TEST_CASE("BotBridge: the can list holds what this player may ask for now", "[BotBridge]")
{
    std::vector<bool> const all = ParseAllowedActions("all");
    ActionContext ctx;
    ctx.has_speaker = true;
    std::string const free = AllowedActsJson(ctx, all);
    CHECK(free.find("\"attack\"") != std::string::npos);
    CHECK(free.find("\"cast\"") != std::string::npos);
    CHECK(free.front() == '[');
    CHECK(free.back() == ']');

    ctx.in_instance = true;
    std::string const inside = AllowedActsJson(ctx, all);
    CHECK(inside.find("\"attack\"") == std::string::npos);
    CHECK(inside.find("\"follow\"") != std::string::npos);

    ActionContext stranger;                 // someone else's companion: only its owner can ask
    stranger.owned = true;
    stranger.has_speaker = true;
    CHECK(AllowedActsJson(stranger, all) == "[]");

    CHECK(AllowedActsJson(ActionContext{ false, true }, ParseAllowedActions("emote")) == "[\"emote\"]");

    // A world bot grouped with a player who does not lead: harmless things only.
    ActionContext member;
    member.has_speaker = true;
    member.grouped_with_human = true;
    CHECK(AllowedActsJson(member, all) == "[\"sit\",\"stand\",\"emote\"]");

    FlatObject o;
    std::string err;
    Event e;
    e.id = 1;
    e.time = "t";
    e.type = "chat_in";
    e.raw.emplace_back("can", free);
    CHECK(ParseFlatObject(BuildEventLine(e), o, err));
}

TEST_CASE("BotBridge: near says how many of a hostile are around", "[BotBridge]")
{
    NearItem n;
    n.ref = 2;
    n.kind = "hostile";
    n.name = "Timber Wolf";
    n.level = 6;
    n.dist = 34.6f;
    n.count = 3;
    CHECK(BuildNearJson({ n }) == "[{\"ref\":2,\"kind\":\"hostile\",\"name\":\"Timber Wolf\",\"level\":6,\"dist\":35,\"count\":3}]");
    n.count = 1;
    CHECK(BuildNearJson({ n }) == "[{\"ref\":2,\"kind\":\"hostile\",\"name\":\"Timber Wolf\",\"level\":6,\"dist\":35}]");
}

TEST_CASE("BotBridge: an order is one of the safe parser words", "[BotBridge]")
{
    std::string norm, why;
    CHECK(SafeOrder("  Mount! ", norm, why));
    CHECK(norm == "mount");
    CHECK(SafeOrder("/drop quest  Wolves", norm, why));
    CHECK(norm == "drop quest wolves");
    CHECK(SafeOrder("use altar", norm, why));
    CHECK_FALSE(SafeOrder("use", norm, why));
    CHECK(SafeOrder("Craft water 20", norm, why));
    CHECK(norm == "craft water 20");
    CHECK_FALSE(SafeOrder("craft", norm, why));
    for (char const* w : { "pull", "Pull back!", "fall back", "rally here", "spread out", "stack", "max dps", "save mana" })
        CHECK(SafeOrder(w, norm, why));
    CHECK(SafeOrder("formation circle", norm, why));
    CHECK(SafeOrder("distance 10", norm, why));
    CHECK_FALSE(SafeOrder("formation", norm, why));
    CHECK_FALSE(SafeOrder("pull everything", norm, why));
    CHECK_FALSE(SafeOrder("tele stormwind", norm, why));
    CHECK_FALSE(SafeOrder("gold 100", norm, why));
    CHECK_FALSE(SafeOrder("mail Karrz all", norm, why));
    CHECK_FALSE(why.empty());

    Reply r;
    std::string err;
    REQUIRE(ParseReplyLine("{\"reply_to\":5,\"say\":\"ok\",\"do\":\"order\",\"order\":\"Dismount\"}", r, err));
    CHECK(r.action.act == Act::Order);
    CHECK(r.action.order == "dismount");
    REQUIRE(ParseReplyLine("{\"reply_to\":5,\"say\":\"no\",\"do\":\"order\",\"order\":\"teleport\"}", r, err));
    CHECK(r.action.act == Act::None);
    CHECK_FALSE(r.action_error.empty());
}

TEST_CASE("BotBridge: trade and go_to a place read their words", "[BotBridge]")
{
    Reply r;
    std::string err;
    REQUIRE(ParseReplyLine("{\"reply_to\":5,\"say\":\"here\",\"do\":\"trade\",\"items\":\"Linen Cloth\"}", r, err));
    CHECK(r.action.act == Act::Trade);
    CHECK(r.action.items == "linen cloth");
    REQUIRE(ParseReplyLine("{\"reply_to\":5,\"do\":\"give\"}", r, err));
    CHECK(r.action.act == Act::Trade);
    REQUIRE(ParseReplyLine("{\"reply_to\":5,\"do\":\"go_to\",\"place\":\"Inn\"}", r, err));
    CHECK(r.action.act == Act::GoTo);
    CHECK(r.action.place == "inn");
}

TEST_CASE("BotBridge: a place by name is a kind of NPC and the words left", "[BotBridge]")
{
    std::string rest;
    CHECK(ParsePlace("the inn at Goldshire", rest) == PlaceKind::Innkeeper);
    CHECK(rest == "goldshire");
    CHECK(ParsePlace("inn keeper", rest) == PlaceKind::Innkeeper);
    CHECK(rest.empty());
    CHECK(ParsePlace("nearest mail box", rest) == PlaceKind::Mailbox);
    CHECK(rest.empty());
    CHECK(ParsePlace("Flight Master", rest) == PlaceKind::FlightMaster);
    CHECK(ParsePlace("the auction house", rest) == PlaceKind::Auctioneer);
    CHECK(ParsePlace("stable master in Goldshire", rest) == PlaceKind::StableMaster);
    CHECK(rest == "goldshire");
    CHECK(ParsePlace("Goldshire", rest) == PlaceKind::Name);
    CHECK(rest == "goldshire");
    CHECK(ParsePlace("go to Smith Argus", rest) == PlaceKind::Name);
    CHECK(rest == "smith argus");
}

TEST_CASE("BotBridge: a channel post's answer may name General or Trade", "[BotBridge]")
{
    Reply r;
    std::string err;
    REQUIRE(ParseReplyLine("{\"reply_to\":9,\"say\":\"WTS Linen Cloth x20, 2g\",\"channel\":\"Trade\"}", r, err));
    CHECK(r.channel == "trade");
    REQUIRE(ParseReplyLine("{\"reply_to\":9,\"say\":\"any1 around?\",\"channel\":\"general\"}", r, err));
    CHECK(r.channel == "general");
    CHECK_FALSE(ParseReplyLine("{\"reply_to\":9,\"say\":\"x\",\"channel\":\"world\"}", r, err));
}

TEST_CASE("BotBridge: step reads a way and a few yards", "[BotBridge]")
{
    Reply r;
    std::string err;
    REQUIRE(ParseReplyLine("{\"reply_to\":5,\"say\":\"ok\",\"do\":\"step\",\"dir\":\"Backwards\",\"yards\":8}", r, err));
    CHECK(r.action.act == Act::Step);
    CHECK(r.action_error.empty());
    CHECK(r.action.dir == "back");
    CHECK(r.action.yards == 8.0f);
    REQUIRE(ParseReplyLine("{\"reply_to\":5,\"do\":\"step\",\"dir\":\"left\",\"yards\":500}", r, err));
    CHECK(r.action.dir == "left");
    CHECK(r.action.yards == 20.0f);         // at most 20
    REQUIRE(ParseReplyLine("{\"reply_to\":5,\"do\":\"back_up\"}", r, err));
    CHECK(r.action.act == Act::Step);
    CHECK(r.action.dir == "back");
    CHECK(r.action.yards == 5.0f);          // the default
    REQUIRE(ParseReplyLine("{\"reply_to\":5,\"do\":\"step\",\"dir\":\"to me\"}", r, err));
    CHECK(r.action.dir == "closer");
    REQUIRE(ParseReplyLine("{\"reply_to\":5,\"say\":\"hm\",\"do\":\"step\",\"dir\":\"up\"}", r, err));
    CHECK(r.action.act == Act::None);
    CHECK_FALSE(r.action_error.empty());
    REQUIRE(ParseReplyLine("{\"reply_to\":5,\"say\":\"hm\",\"do\":\"step\"}", r, err));
    CHECK(r.action.act == Act::None);       // no way given

    ActionContext leader;                   // in a dungeon fight, the leader may move a bot out of the fire
    leader.has_speaker = true;
    leader.grouped_with_human = true;
    leader.speaker_controls = true;
    leader.in_instance = true;
    leader.in_combat = true;
    CHECK(CheckActionContext(Act::Step, leader) == nullptr);
    ActionContext stranger;                 // a passer-by: out of a fight yes, in one no
    stranger.has_speaker = true;
    CHECK(CheckActionContext(Act::Step, stranger) == nullptr);
    stranger.in_combat = true;
    CHECK(CheckActionContext(Act::Step, stranger) != nullptr);
}

TEST_CASE("BotBridge: sign_charter needs nothing more and is not for a stranger's companion", "[BotBridge]")
{
    Reply r;
    std::string err;
    REQUIRE(ParseReplyLine("{\"reply_to\":5,\"say\":\"np, signed\",\"do\":\"sign\"}", r, err));
    CHECK(r.action.act == Act::SignCharter);
    CHECK(r.action_error.empty());
    CHECK(std::string(ActName(Act::SignCharter)) == "sign_charter");
    ActionContext world;                    // a free world bot, any player asking
    world.has_speaker = true;
    CHECK(CheckActionContext(Act::SignCharter, world) == nullptr);
    world.in_combat = true;
    CHECK(CheckActionContext(Act::SignCharter, world) != nullptr);
    ActionContext comp;                     // someone's companion, a stranger asking
    comp.owned = true;
    comp.has_speaker = true;
    CHECK(CheckActionContext(Act::SignCharter, comp) != nullptr);
}

TEST_CASE("BotBridge: orders and trades are only the owner's or the group leader's", "[BotBridge]")
{
    ActionContext owner;
    owner.owned = true;
    owner.has_speaker = true;
    owner.speaker_is_owner = true;
    CHECK(CheckActionContext(Act::Order, owner) == nullptr);
    CHECK(CheckActionContext(Act::Trade, owner) == nullptr);
    owner.in_combat = true;
    CHECK(CheckActionContext(Act::Order, owner) == nullptr);   // what he could type himself in a fight
    CHECK(CheckActionContext(Act::Trade, owner) != nullptr);

    ActionContext stranger;                 // a free world bot, a passer-by asking
    stranger.has_speaker = true;
    CHECK(CheckActionContext(Act::Follow, stranger) == nullptr);
    CHECK(CheckActionContext(Act::Order, stranger) != nullptr);
    CHECK(CheckActionContext(Act::Trade, stranger) != nullptr);

    ActionContext leader;                   // a world bot in a dungeon group the player leads
    leader.has_speaker = true;
    leader.grouped_with_human = true;
    leader.speaker_controls = true;
    leader.in_instance = true;
    CHECK(CheckActionContext(Act::Order, leader) == nullptr);
    CHECK(CheckActionContext(Act::Trade, leader) == nullptr);

    ActionContext self;                     // the program on its own: never
    self.owned = true;
    self.owned_bots_allowed = true;
    CHECK(CheckActionContext(Act::Order, self) != nullptr);
}

#endif // TESTS_HAVE_PLAYERBOT_V2
