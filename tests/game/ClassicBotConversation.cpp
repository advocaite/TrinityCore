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

#include "../../src/modules/PlayerbotV2/Bot/BotConversation.h"

#include <set>

using namespace Playerbot::V2;

namespace {

ConversationContext Ctx()
{
    ConversationContext c;
    c.self_name = "Aron";
    c.sender_name = "Karrz";
    c.class_name = "mage";
    c.zone = "Elwynn Forest";
    c.activity = "out questing";
    c.level = 23;
    c.bot_seed = 12345;
    return c;
}

std::string Lower(std::string s)
{
    for (char& ch : s)
        ch = char(std::tolower(static_cast<unsigned char>(ch)));
    return s;
}

ConvIntent Cls(char const* line, std::string* slot = nullptr)
{
    return ConvClassify(ConvNormalize(line), slot);
}

std::string Say(ConversationContext const& c, char const* line, uint32_t sel = 0, bool addressed = true)
{
    std::string out;
    REQUIRE(ConvReply(c, ConvNormalize(line), line, sel, addressed, out));
    return out;
}

} // namespace

TEST_CASE("BotConversation: lines are classified to the right intent", "[botconversation]")
{
    CHECK(Cls("how are you?") == ConvIntent::HowAreYou);
    CHECK(Cls("how's it going") == ConvIntent::HowAreYou);
    CHECK(Cls("how u doing") == ConvIntent::HowAreYou);
    CHECK(Cls("where are you from?") == ConvIntent::WhereFrom);
    CHECK(Cls("where are you") == ConvIntent::WhereAreYou);
    CHECK(Cls("what level are you") == ConvIntent::Level);
    CHECK(Cls("what class are you?") == ConvIntent::ClassSpec);
    CHECK(Cls("what are you doing") == ConvIntent::WhatDoing);
    CHECK(Cls("wyd") == ConvIntent::WhatDoing);
    CHECK(Cls("want to group?") == ConvIntent::GroupInvite);
    CHECK(Cls("lfg deadmines?") == ConvIntent::GroupInvite);
    CHECK(Cls("can you help me with a quest") == ConvIntent::QuestHelp);
    CHECK(Cls("what profession do you have") == ConvIntent::Trade);
    CHECK(Cls("are you in a guild") == ConvIntent::Guild);
    CHECK(Cls("what's your name") == ConvIntent::Name);
    CHECK(Cls("tell me a joke") == ConvIntent::Joke);
    CHECK(Cls("you suck") == ConvIntent::Insult);
    CHECK(Cls("you are awesome") == ConvIntent::Compliment);
    CHECK(Cls("ok bye, gtg") == ConvIntent::Goodbye);
    CHECK(Cls("blorp") == ConvIntent::Unknown);
    CHECK(Cls("") == ConvIntent::Unknown);
}

TEST_CASE("BotConversation: ordinary words are not mistaken for intents", "[botconversation]")
{
    // "how do you do that" is not a wellbeing question, "the quest is called" is not a name question, "levelling" is not "level".
    CHECK(Cls("how do you pull that off") == ConvIntent::Unknown);
    CHECK(Cls("whats the quest called") != ConvIntent::Name);
    CHECK(Cls("wanna trade") == ConvIntent::Unknown);
    CHECK(Cls("pull now") == ConvIntent::Unknown);
}

TEST_CASE("BotConversation: directions find the destination", "[botconversation]")
{
    std::string slot;
    CHECK(Cls("how do i get to ironforge?", &slot) == ConvIntent::Directions);
    CHECK(slot == "ironforge");
    CHECK(Cls("where is thunder bluff", &slot) == ConvIntent::Directions);
    CHECK(slot == "thunder bluff");
    CHECK(Cls("where's stormwind", &slot) == ConvIntent::Directions);
    CHECK(slot == "stormwind");
    // A place we do not know is not a direction we can give.
    CHECK(Cls("how do i get to the moon", &slot) == ConvIntent::Unknown);

    // Casual bots type in lower case, so compare without case.
    CHECK(Lower(Say(Ctx(), "how do i get to ironforge", 0)).find("dun morogh") != std::string::npos);
    CHECK(Lower(Say(Ctx(), "how do i get to darnassus", 1)).find("auberdine") != std::string::npos);
}

TEST_CASE("BotConversation: directions to the zone the bot stands in say so", "[botconversation]")
{
    ConversationContext c = Ctx();
    c.zone = "Elwynn Forest";
    std::string const r = Say(c, "where is elwynn forest", 0);
    CHECK(Lower(r).find("dun morogh") == std::string::npos);
    std::string const l = Lower(r);
    CHECK((l.find("here") != std::string::npos || l.find("zone") != std::string::npos || l.find("where") != std::string::npos));
}

TEST_CASE("BotConversation: replies carry the bot's real state", "[botconversation]")
{
    ConversationContext c = Ctx();
    c.length = ConvLength::Normal;
    // Over many selectors every phrasing for level names the level, every one for the zone names the zone.
    for (uint32_t sel = 0; sel < 40; ++sel)
    {
        CHECK(Say(c, "what level are you", sel).find("23") != std::string::npos);
    }
    std::string zone_reply;
    for (uint32_t sel = 0; sel < 40; ++sel)
    {
        zone_reply = Say(c, "where are you", sel);
        bool const has = zone_reply.find("lwynn") != std::string::npos;   // survives lower-casing and capitalisation
        CHECK(has);
    }
}

TEST_CASE("BotConversation: group answer follows whether the bot is grouped", "[botconversation]")
{
    ConversationContext c = Ctx();
    c.in_group = false;
    std::set<std::string> free_set, busy_set;
    for (uint32_t sel = 0; sel < 40; ++sel)
        free_set.insert(Say(c, "want to group?", sel));
    c.in_group = true;
    for (uint32_t sel = 0; sel < 40; ++sel)
        busy_set.insert(Say(c, "want to group?", sel));
    for (auto const& f : free_set)
        for (auto const& b : busy_set)
            CHECK(f != b);
    CHECK(free_set.size() > 1);   // several phrasings, not one canned line
}

TEST_CASE("BotConversation: quest, profession and guild answers use the data when there is some", "[botconversation]")
{
    ConversationContext c = Ctx();
    c.tone = ConvTone::Polite;
    c.quest_title = "The Fargodeep Mine";
    c.professions = { "mining", "blacksmithing" };
    c.guild_name = "Night Owls";
    bool quest_named = false, prof_named = false, guild_named = false;
    for (uint32_t sel = 0; sel < 40; ++sel)
    {
        quest_named |= Say(c, "can you help with a quest", sel).find("argodeep") != std::string::npos;
        prof_named |= Say(c, "what professions do you have", sel).find("mining") != std::string::npos;
        guild_named |= Say(c, "are you in a guild", sel).find("ight") != std::string::npos;
    }
    CHECK(quest_named);
    CHECK(prof_named);
    CHECK(guild_named);

    // Without data the reply must not contain a hole or an unreplaced placeholder.
    ConversationContext n = Ctx();
    for (uint32_t sel = 0; sel < 40; ++sel)
        for (char const* line : { "can you help with a quest", "what professions do you have", "are you in a guild", "how are you",
                                  "where are you from", "what are you doing", "whats your name", "tell me a joke", "you suck",
                                  "bye", "you are awesome", "what class are you", "how do i get to org" })
        {
            std::string const r = Say(n, line, sel);
            CHECK(r.find('{') == std::string::npos);
            CHECK(r.find("  ") == std::string::npos);
            CHECK(r.size() > 0);
        }
}

TEST_CASE("BotConversation: tone and length shape the reply", "[botconversation]")
{
    ConversationContext rude = Ctx(), polite = Ctx();
    rude.tone = ConvTone::Rude;
    polite.tone = ConvTone::Polite;
    std::set<std::string> a, b;
    for (uint32_t sel = 0; sel < 60; ++sel)
    {
        a.insert(Say(rude, "how are you", sel));
        b.insert(Say(polite, "how are you", sel));
    }
    for (auto const& x : a)
        CHECK(b.count(x) == 0);

    ConversationContext terse = Ctx(), chatty = Ctx();
    terse.length = ConvLength::Terse;
    chatty.length = ConvLength::Chatty;
    std::set<std::string> t, ch;
    for (uint32_t sel = 0; sel < 200; ++sel)
    {
        t.insert(Say(terse, "how are you", sel));
        ch.insert(Say(chatty, "how are you", sel));
    }
    CHECK(t.size() < ch.size());
}

TEST_CASE("BotConversation: two bots asked alike do not always answer alike, one bot is stable", "[botconversation]")
{
    ConversationContext a = Ctx(), b = Ctx();
    a.bot_seed = 1;
    b.bot_seed = 2;
    int differ = 0;
    for (uint32_t sel = 0; sel < 50; ++sel)
        if (Say(a, "how are you", sel) != Say(b, "how are you", sel))
            ++differ;
    CHECK(differ > 10);
    CHECK(Say(a, "what level are you", 7) == Say(a, "what level are you", 7));
}

TEST_CASE("BotConversation: writing style is a stable per-bot trait", "[botconversation]")
{
    // Casual bots never capitalise or use apostrophes; the others always capitalise the first letter.
    int casual = 0, proper = 0;
    for (uint32_t seed = 0; seed < 200; ++seed)
    {
        ConversationContext c = Ctx();
        c.bot_seed = seed;
        std::string const first = Say(c, "how are you", 0);
        bool const upper = std::isupper(static_cast<unsigned char>(first[0])) != 0;
        for (uint32_t sel = 1; sel < 6; ++sel)
            CHECK((std::isupper(static_cast<unsigned char>(Say(c, "how are you", sel)[0])) != 0) == upper);
        (upper ? proper : casual)++;
    }
    CHECK(casual > 0);
    CHECK(proper > 0);
    CHECK(casual > proper);
}

TEST_CASE("BotConversation: only unknown lines reach a provider, and only when addressed", "[botconversation]")
{
    struct Echo : ReplyProvider
    {
        int calls = 0;
        bool TryReply(ConversationContext const&, std::string const& m, std::string& out) override
        {
            ++calls;
            out = "model says: " + m;
            return true;
        }
    };
    auto p = std::make_shared<Echo>();
    ConvSetProvider(p);

    std::string out;
    CHECK(ConvReply(Ctx(), ConvNormalize("what do you think of the weather"), "what do you think of the weather", 0, true, out));
    CHECK(out == "model says: what do you think of the weather");
    CHECK(p->calls == 1);

    // Not addressed: the provider stays out of party chatter.
    CHECK_FALSE(ConvReply(Ctx(), ConvNormalize("what do you think of the weather"), "what do you think of the weather", 0, false, out));
    // A known intent never reaches the provider.
    CHECK(ConvReply(Ctx(), ConvNormalize("how are you"), "how are you", 0, true, out));
    CHECK(out.find("model says") == std::string::npos);
    CHECK(p->calls == 1);

    // No provider: unknown lines fall through to the caller's existing fallback.
    ConvSetProvider(nullptr);
    CHECK_FALSE(ConvReply(Ctx(), ConvNormalize("what do you think of the weather"), "what do you think of the weather", 0, true, out));
}

TEST_CASE("BotConversation: a repeated line gets a different answer, never silence", "[botconversation]")
{
    ConversationContext c = Ctx();
    for (uint32_t repeat = 1; repeat < 6; ++repeat)
        for (uint32_t sel = 0; sel < 20; ++sel)
            CHECK_FALSE(ConvRepeatReply(c, sel, repeat).empty());
    std::set<std::string> seen;
    for (uint32_t sel = 0; sel < 60; ++sel)
        seen.insert(ConvRepeatReply(c, sel, 1));
    CHECK(seen.size() > 1);
    // a third repeat uses the stronger pool
    std::set<std::string> second, later;
    for (uint32_t sel = 0; sel < 60; ++sel)
    {
        second.insert(Lower(ConvRepeatReply(c, sel, 1)));
        later.insert(Lower(ConvRepeatReply(c, sel, 3)));
    }
    for (auto const& x : second)
        CHECK(later.count(x) == 0);
}

TEST_CASE("BotConversation: intent names for the log are stable", "[botconversation]")
{
    CHECK(std::string(ConvIntentName(ConvIntent::HowAreYou)) == "how_are_you");
    CHECK(std::string(ConvIntentName(ConvIntent::Unknown)) == "unknown");
    CHECK(std::string(ConvIntentName(Cls("how do i get to org") )) == "directions");
}

TEST_CASE("BotConversation: a hello is answered here only for someone the bot knows", "[botconversation]")
{
    CHECK(Cls("hi") == ConvIntent::Greeting);
    CHECK(Cls("hey Aron") == ConvIntent::Greeting);
    CHECK(Cls("hi how are you") == ConvIntent::HowAreYou);   // a real question wins over the opener
    CHECK(Cls("hello there my friend how goes it") == ConvIntent::Unknown);   // too long to be just a hello

    ConversationContext stranger = Ctx();
    std::string out;
    CHECK_FALSE(ConvReply(stranger, ConvNormalize("hi"), "hi", 0, true, out));   // falls through to the ike3 hello pool

    ConversationContext owner = Ctx();
    owner.knows_sender = true;
    std::set<std::string> seen;
    for (uint32_t sel = 0; sel < 40; ++sel)
    {
        std::string const r = Say(owner, "hi", sel);
        CHECK(Lower(r).find("know you") == std::string::npos);
        seen.insert(r);
    }
    CHECK(seen.size() > 1);
}
