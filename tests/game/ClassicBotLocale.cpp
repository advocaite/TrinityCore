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

#include "../../src/modules/PlayerbotV2/Util/BotLocaleText.h"
#include "../../src/modules/PlayerbotV2/Bot/Raid/RaidBanter.h"
#include "../../src/modules/PlayerbotV2/Bot/Quest/QuestRefusal.h"
#include <cctype>
#include <set>
#include <string>

using namespace Playerbot::Locale;

namespace
{
int Captures(std::string const& en)
{
    int n = 0;
    for (size_t i = 0; i + 1 < en.size(); ++i)
        if (en[i] == '{' && (en[i + 1] == '}' || ((en[i + 1] == '?' || en[i + 1] == '#' || en[i + 1] == '$') && i + 2 < en.size() && en[i + 2] == '}')))
            ++n;
    return n;
}

int Placeholders(std::string const& tr)
{
    int n = 0;
    for (size_t i = 0; i + 1 < tr.size(); ++i)
        if (tr[i] == '{' && (tr[i + 1] == '}' || (tr[i + 1] >= '1' && tr[i + 1] <= '9')))
            ++n;
    return n;
}
}

TEST_CASE("BotLocale: every Russian entry is well formed and used once", "[BotLocale]")
{
    Catalog const& ru = Russian();
    for (std::string const& bad : ru.Rejected())
        FAIL_CHECK("refused: " << bad);
    CHECK(ru.Rejected().empty());
    CHECK(ru.Size() == RussianEntries().size() + RussianChatterEntries().size());

    // A translation that drops a capture loses a name or a number the player needs.
    for (Catalog::Entry const& e : RussianEntries())
    {
        INFO(e.en);
        CHECK(Placeholders(e.tr) == Captures(e.en));
    }
}

TEST_CASE("BotLocale: the stock chatter keeps every placeholder", "[BotLocale]")
{
    // A placeholder the translation drops loses the item, the zone or the name; one it invents is never filled.
    auto placeholders = [](std::string const& s)
    {
        std::multiset<std::string> out;
        for (size_t i = 0; i < s.size(); ++i)
        {
            size_t j = i + 1;
            if (s[i] == '%')
                while (j < s.size() && (std::isalnum(static_cast<unsigned char>(s[j])) || s[j] == '_')) ++j;
            else if (s[i] == '<')
            {
                while (j < s.size() && s[j] >= 'a' && s[j] <= 'z') ++j;
                if (j >= s.size() || s[j] != '>') continue;
                ++j;
            }
            else
                continue;
            if (j > i + 1)
                out.insert(s.substr(i, j - i));
        }
        return out;
    };
    for (Catalog::Entry const& e : RussianChatterEntries())
    {
        INFO(e.en);
        CHECK(placeholders(e.en) == placeholders(e.tr));
    }
}

TEST_CASE("BotLocale: the stock chatter in Russian", "[BotLocale]")
{
    std::string const linen = "|cnIQ1:|Hitem:2589::::::::12::::::::::|h[Linen Cloth]|h|r";
    CHECK(Translate(Lang::Ru, "WTS " + linen + "x20 for 1g 20s.") == "Продаю " + linen + "x20 за 1з 20с.");
    CHECK(Translate(Lang::Ru, "Argent Dawn: need 3K for exalted.") == "«Серебряный Рассвет»: нужно ещё 3K до уровня «Превознесение».");
    CHECK(Translate(Lang::Ru, "Bite me, Hogger!") == "Выкуси, Hogger!");
    CHECK(Translate(Lang::Ru, "Elwynn Forest: where is everyone?") == "Elwynn Forest: где все?");
    CHECK(Translate(Lang::Ru, "hi Karrz") == "привет, Karrz");
    CHECK(Translate(Lang::Ru, "tank LFG Deadmines") == "танк ищет группу: Deadmines");
    CHECK(Translate(Lang::Ru, "Ding 23!") == "Дзынь, 23!");
    // Two lines that differ only in what fills them stay apart: a sale is no dungeon call.
    CHECK(Translate(Lang::Ru, "Anyone needs " + linen + " for 2g?") == "Кому нужно " + linen + " за 2з?");
    CHECK(Translate(Lang::Ru, "Anyone needs Protection Warrior for Deadmines?") == "Кому нужен Protection Warrior в Deadmines?");
    // A short chatter line does not take every line that starts the same way.
    CHECK(Translate(Lang::Ru, "got it, Hogger") == "got it, Hogger");
    CHECK(Translate(Lang::Ru, "hey there, i can tank") == "hey there, i can tank");
    CHECK(Translate(Lang::Ru, "WTS [Linen Cloth] x20 for 1g 20s.") == "WTS [Linen Cloth] x20 for 1g 20s.");
}

TEST_CASE("BotLocale: chatter placeholders", "[BotLocale]")
{
    Catalog c;
    CHECK(c.AddChatter("%a sells %item_link to %b for %cost_gold", "%b buys %item_link from %a, %cost_gold"));
    CHECK_FALSE(c.AddChatter("hi %s", "hello %other"));   // a placeholder the English line has not got
    CHECK_FALSE(c.AddChatter("hi {}", "x"));              // a brace of its own
    CHECK(c.Rejected().size() == 2);
    CHECK(c.Translate("Ann sells |Hitem:1|h[Axe]|h to Bob for 3g") == "Bob buys |Hitem:1|h[Axe]|h from Ann, 3g");
    CHECK(c.Translate("Ann sells an axe to Bob for 3g") == "Ann sells an axe to Bob for 3g");   // no link, no match
}

TEST_CASE("BotLocale: English readers get the line unchanged", "[BotLocale]")
{
    CHECK(Translate(Lang::En, "Rento: following you") == "Rento: following you");
    CHECK(Translate(Lang::En, "anything at all") == "anything at all");
}

TEST_CASE("BotLocale: per-bot confirmations keep the bot's name", "[BotLocale]")
{
    CHECK(Translate(Lang::Ru, "Rento: following you") == "Rento: следую за вами");
    CHECK(Translate(Lang::Ru, "Rento: following at 5 yards") == "Rento: следую на расстоянии 5 ярд.");
    CHECK(Translate(Lang::Ru, "Zelah: attacking Kobold Miner") == "Zelah: атакую: Kobold Miner");
    CHECK(Translate(Lang::Ru, "Zelah: tank on star") == "Zelah: танкую: звезда");
    CHECK(Translate(Lang::Ru, "Zelah: mount ok") == "Zelah: mount — ок");
}

TEST_CASE("BotLocale: captured words are translated, names and numbers are not", "[BotLocale]")
{
    CHECK(Translate(Lang::Ru, "Pull back: pulling Hogger to the rally point.") == "Пул назад: тяну Hogger к точке сбора.");
    CHECK(Translate(Lang::Ru, "Pull back: pulling Hogger to you.") == "Пул назад: тяну Hogger к вам.");
    CHECK(Translate(Lang::Ru, "Rento: level 20 Mage (Fire), dps - following you") ==
          "Rento: уровень 20, Маг (Огонь), боец — следую за вами");
    CHECK(Translate(Lang::Ru, "Assign: skull cleared.") == "Назначение: череп — снято.");
}

TEST_CASE("BotLocale: optional tails and number runs", "[BotLocale]")
{
    CHECK(Translate(Lang::Ru, "Rento: health 100/200 (50%), mana 40/90, item level 23, armor 300, str 10 agi 11 sta 12 int 13, crit 5.5%") ==
          "Rento: здоровье 100/200 (50%), мана 40/90, уровень предметов 23, броня 300, сил 10 лов 11 вын 12 инт 13, крит 5.5%");
    CHECK(Translate(Lang::Ru, "Rento: health 100/200 (50%), item level 23, armor 300, str 10 agi 11 sta 12 int 13, crit 5.5%") ==
          "Rento: здоровье 100/200 (50%), уровень предметов 23, броня 300, сил 10 лов 11 вын 12 инт 13, крит 5.5%");
    CHECK(Translate(Lang::Ru, "Rento: 12 pieces worn, avg item level 45, best: [Sword], [Shield]; 3 free bag slots; 1g 2s 3c") ==
          "Rento: надето вещей: 12, средний уровень предметов 45, лучшее: [Sword], [Shield]; свободно ячеек в сумках: 3; 1з 2с 3м");
}

TEST_CASE("BotLocale: money gets the Russian letters, other text is not taken for money", "[BotLocale]")
{
    CHECK(Translate(Lang::Ru, "[Linen Cloth] x5 is 1g 20s, open a trade and put that in") ==
          "[Linen Cloth] x5 стоит 1з 20с, откройте обмен и положите эту сумму");
    CHECK(Translate(Lang::Ru, "i'll give you 35s 2c for that") == "дам за это 35с 2м");
    // "X is Y" with no money in it is no price line.
    CHECK(Translate(Lang::Ru, "Stormwind is far") == "Stormwind is far");
}

TEST_CASE("BotLocale: comma lists of known words inside a line", "[BotLocale]")
{
    CHECK(Translate(Lang::Ru, "Prefer: Blessing of Might, pet off.") == "Выбор: Благословение могущества, питомец выключен.");
    // One unknown part: the list stays as it was, the frame is still translated.
    CHECK(Translate(Lang::Ru, "Prefer: Blessing of Might, Unknown Thing.") == "Выбор: Blessing of Might, Unknown Thing.");
}

TEST_CASE("BotLocale: the add-on's structured whispers are never translated", "[BotLocale]")
{
    CHECK(NeverTranslate("ASSIGN|8|Rento|tank"));
    CHECK(Translate(Lang::Ru, "ASSIGN|END|3") == "ASSIGN|END|3");
}

TEST_CASE("BotLocale: raid callouts", "[BotLocale]")
{
    CHECK(Translate(Lang::Ru, "Living Bomb on me, running out") == "Living Bomb на мне, отбегаю");
    CHECK(Translate(Lang::Ru, "taunting Flamegor") == "провоцирую Flamegor");
    CHECK(Translate(Lang::Ru, "Polymorph on Flamewaker Healer, don't break it") == "Polymorph на Flamewaker Healer, не сбивайте");
    CHECK(Translate(Lang::Ru, "spread out") == "расходимся");
    CHECK(Translate(Lang::Ru, "wipe, run back to the entrance") == "вайп, бегите ко входу");
}

TEST_CASE("BotLocale: raid leader banter", "[BotLocale]")
{
    using namespace Playerbot::Raid;
    CHECK(Translate(Lang::Ru, "Karrz GET OUT OF IT") == "Karrz, ВЫЙДИ ОТТУДА");
    CHECK(Translate(Lang::Ru, "NOBODY KICKED Shadow Bolt??") == "НИКТО НЕ СБИЛ Shadow Bolt??");
    CHECK(Translate(Lang::Ru, "WIPE. Brok died first. again") == "ВАЙП. Brok - опять первый труп");
    CHECK(Translate(Lang::Ru, "that was Brok's fault") == "это вина Brok");
    CHECK(Translate(Lang::Ru, "i was rooted ok") == "меня зарутило, ок");

    // Every fixed line the leader or a raider can say has its Russian form.
    Catalog const& ru = Russian();
    for (uint8_t m = 0; m < uint8_t(Mishap::Count); ++m)
        for (BanterLevel l : { BanterLevel::Mild, BanterLevel::Harsh })
            for (uint32_t pick = 0; pick < 3; ++pick)
                for (char const* name : { "", "Brok" })
                {
                    std::string const s = LeaderLine(Mishap(m), l, name, pick);
                    INFO(s);
                    CHECK((s.empty() || ru.Knows(s)));
                }
    for (BanterLevel l : { BanterLevel::Mild, BanterLevel::Harsh })
        for (uint32_t pick = 0; pick < 8; ++pick)
            for (char const* blame : { "", "Brok" })
                for (bool self : { false, true })
                {
                    std::string const s = WipeReply(l, blame, self, pick);
                    INFO(s);
                    CHECK_FALSE(s.empty());
                    CHECK(ru.Knows(s));
                }
}

TEST_CASE("BotLocale: boss tactics answers", "[BotLocale]")
{
    CHECK(Translate(Lang::Ru, "Tactics for Lucifron: adds first, no AoE, tank at your spot. Your bots use them every time you fight Lucifron.")
          == "Тактика на Lucifron: сначала адды, без АоЕ, танк на вашем месте. Ваши боты применяют её в каждом бою с Lucifron.");
    CHECK(Translate(Lang::Ru, "Tactics for Baron Geddon: boss first.") == "Тактика на Baron Geddon: сначала босс.");
    CHECK(Translate(Lang::Ru, "Tactics for Lucifron cleared.") == "Тактика на Lucifron удалена.");
    CHECK(Translate(Lang::Ru, "Bosses here: Lucifron, Magmadar.") == "Боссы здесь: Lucifron, Magmadar.");
}

TEST_CASE("BotLocale: why a quest accept was refused", "[BotLocale]")
{
    using Playerbot::QuestRefusal;
    using Playerbot::QuestRefusalLine;
    CHECK(QuestRefusalLine(QuestRefusal::None, "Wolves") == "");
    CHECK(QuestRefusalLine(QuestRefusal::LowLevel, "Wolves Across the Border") == "I can't take Wolves Across the Border: my level is too low.");
    CHECK(QuestRefusalLine(QuestRefusal::Done, "") == "I already did that quest.");
    CHECK(Translate(Lang::Ru, QuestRefusalLine(QuestRefusal::LogFull, "Wolves")) == "Я не могу взять задание Wolves: мой журнал заданий полон.");
    CHECK(Translate(Lang::Ru, QuestRefusalLine(QuestRefusal::NotAgainYet, "Wolves")) == "Задание Wolves пока нельзя взять снова.");

    // Every reason has a line with the quest in it, and every line has its Russian form.
    Catalog const& ru = Russian();
    for (uint8_t r = uint8_t(QuestRefusal::Other); r <= uint8_t(QuestRefusal::Unfinishable); ++r)
    {
        std::string const s = QuestRefusalLine(QuestRefusal(r), "Wolves");
        INFO(s);
        CHECK(s.find("Wolves") != std::string::npos);
        CHECK(ru.Knows(s));
    }
    CHECK(ru.Knows("The quest share ran out before I could take it."));
}

TEST_CASE("BotLocale: unknown lines come back in English", "[BotLocale]")
{
    CHECK(Translate(Lang::Ru, "Perf: ticks=5 emit=3") == "Perf: ticks=5 emit=3");
    CHECK(Translate(Lang::Ru, "") == "");
}

TEST_CASE("BotLocale: template syntax", "[BotLocale]")
{
    Catalog c({
        { "{} gives {} to {}", "{3} получает {2} от {1}" },
        { "a {}{} b", "x" },          // two captures in a row: refused
        { "{}", "x" },                // no fixed text: refused
        { "level {#}{?}!", "ур. {}{}!" },
        { "bad {}", "{2}" },          // uses a capture the template has not got: refused
    });
    CHECK(c.Rejected().size() == 3);
    CHECK(c.Translate("A gives B to C") == "C получает B от A");
    CHECK(c.Translate("level 45, best!") == "ур. 45, best!");
    CHECK(c.Translate("level 45!") == "ур. 45!");
    CHECK(c.Translate("level x!") == "level x!");   // {#} takes a number only
}

#endif
