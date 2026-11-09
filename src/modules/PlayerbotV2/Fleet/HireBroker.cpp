#include "HireBroker.h"
#include "HireMgr.h"
#include "OwnerRegistry.h"
#include "../Services.h"
#include "../Bot/ClassTables.h"
#include "CharacterCache.h"
#include "Chat.h"
#include "Creature.h"
#include "GossipDef.h"
#include "Log.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "SharedDefines.h"
#include "StringFormat.h"
#include "WorldSession.h"

using namespace Playerbot::V2;
using namespace Playerbot::V2::Fleet;

namespace {

// Gossip actions: a grade's role menu, a role's class menu, a class hired. 1000 + grade * 100 + role * 20 + class stays under 2000.
constexpr uint32 kActionGrade     = 1;      // + grade (1..3): the role menu
constexpr uint32 kActionContracts = 5;      // the player's hired bots, in chat
constexpr uint32 kActionOptIn     = 6;      // `.bot hire on|off` from the gossip (Hire.Mode = 2)
constexpr uint32 kActionRole      = 10;     // + grade * 10 + role (1..3): the class menu
constexpr uint32 kActionHire      = 1000;   // + grade * 100 + role * 20 + class

// The gossip text: npc_text 3959, "Greetings, $r." (BroadcastText 6498). Not DEFAULT_GOSSIP_MESSAGE: its npc_text 16777215 points at
// BroadcastText 50429, a retail text the Classic client does not have, and the client then opens no gossip window at all (Goldshire,
// 2026-11-01: right-click on Brannock Ironhale opened nothing although his NPC flags and script were right).
constexpr uint32 kBrokerText      = 3959;

// The menu the client is shown: gossip menu 6110000 (the first broker entry), options 61100000 + their place in the menu. Every menu the
// Classic client opens carries a gossip menu id and positive option ids; a script menu left at menu 0 gets ids 0, -1, -2 ... from
// GossipMenu::AddMenuItem, the one difference left after right-click on Brannock Ironhale still opened nothing with a text the
// client has (Goldshire, 2026-11-01, dev build with npc_text 3959). The script builds the options; the menu itself must also be in the
// database (gossip_menu 6110000 -> npc_text 3959, creature_template_gossip for every broker), because the server hides the gossip NPC
// flag from a player when Player::CanSeeGossipOn finds no menu, and the client then shows no talk cursor and never sends the hello.
// The hello is logged (playerbot.v2, INFO) so Server.log shows whether a click reached the broker at all.
constexpr uint32 kBrokerMenu      = 6110000;
constexpr int32  kBrokerOptionId  = 61100000;

void StartMenu(Player* player)
{
    ClearGossipMenuFor(player);
    InitGossipMenuFor(player, kBrokerMenu);
}

void AddOption(Player* player, std::string text, uint32 action, std::string confirm = {}, uint64 money = 0)
{
    GossipMenu& menu = player->PlayerTalkClass->GetGossipMenu();
    int32 const id = kBrokerOptionId + int32(menu.GetMenuItemCount());
    menu.AddMenuItem(id, -1, GossipOptionNpc::None, std::move(text), 0, GossipOptionFlags::None, {}, 0, 0, false, money, std::move(confirm), {}, {},
                     GOSSIP_SENDER_MAIN, action);
}

struct ClassInfo { uint8 cls; char const* name; bool tank, healer, dps; };
constexpr ClassInfo kClasses[] = {
    { CLASS_WARRIOR, "Warrior", true,  false, true },
    { CLASS_PALADIN, "Paladin", true,  true,  true },
    { CLASS_HUNTER,  "Hunter",  false, false, true },
    { CLASS_ROGUE,   "Rogue",   false, false, true },
    { CLASS_PRIEST,  "Priest",  false, true,  true },
    { CLASS_SHAMAN,  "Shaman",  false, true,  true },
    { CLASS_MAGE,    "Mage",    false, false, true },
    { CLASS_WARLOCK, "Warlock", false, false, true },
    { CLASS_DRUID,   "Druid",   true,  true,  true },
};

ClassInfo const* FindClass(uint8 cls)
{
    for (ClassInfo const& c : kClasses)
        if (c.cls == cls)
            return &c;
    return nullptr;
}

bool Plays(ClassInfo const& c, Playerbot::Role role)
{
    return role == Playerbot::Role::Tank ? c.tank : role == Playerbot::Role::Healer ? c.healer : c.dps;
}

// Role index 1..3 in the gossip actions.
Playerbot::Role RoleOfIndex(uint32 i)
{
    return i == 1 ? Playerbot::Role::Tank : i == 2 ? Playerbot::Role::Healer : Playerbot::Role::Dps;
}

char const* RoleWord(Playerbot::Role role)
{
    return role == Playerbot::Role::Tank ? "tank" : role == Playerbot::Role::Healer ? "healer" : "damage dealer";
}

std::string GradeOffer(Hire::Grade grade, uint64 price)
{
    switch (grade)
    {
        case Hire::Grade::Sellsword: return Trinity::StringFormat("I need a sellsword. ({} each)", Hire::MoneyText(price));
        case Hire::Grade::Veteran:   return Trinity::StringFormat("I need a veteran raider. ({} each)", Hire::MoneyText(price));
        case Hire::Grade::Legend:    return Trinity::StringFormat("I hear you know someone... legendary. ({} each)", Hire::MoneyText(price));
        default:                     return {};
    }
}

// What the broker says when a hire goes through.
char const* HiredLine(Hire::Broker broker)
{
    switch (broker)
    {
        case Hire::Broker::BlackMarket: return "Pleasure doing business. No refunds, no questions, and you never saw me.";
        case Hire::Broker::Capital:     return "A fine choice. They have stood in the line before, they will not break.";
        default:                        return "Done. Your new sword arm will meet you outside.";
    }
}

void Tell(Player* player, std::string const& text)
{
    if (player && player->GetSession())
        ChatHandler(player->GetSession()).SendSysMessage(text.c_str());
}

void ListContracts(Player* player)
{
    uint32 const account = player->GetSession()->GetAccountId();
    Hire::Prices const prices = HireMgr::ConfigPrices();
    uint32 n = 0;
    for (Playerbot::BotId id : Playerbot::Services::Owners().BotsOwnedBy(account))
    {
        CharacterCacheEntry const* entry = sCharacterCache->GetCharacterCacheByGuid(ObjectGuid::Create<HighGuid::Player>(id));
        if (!entry)
            continue;
        HireRecord const rec = HireMgr::Instance().RecordOf(id);
        uint64 const upkeep = Hire::UpkeepFor(prices, rec.price);
        ClassInfo const* c = FindClass(entry->Class);
        Tell(player, Trinity::StringFormat("{}: level {} {} {}{}", entry->Name, uint32(entry->Level), c ? c->name : "",
                                           Hire::GradeName(rec.grade),
                                           upkeep ? " (" + Hire::MoneyText(upkeep) + " a day you call them)" : std::string()));
        ++n;
    }
    if (!n)
        Tell(player, "Nobody works for you yet.");
}

}  // namespace

struct npc_fb_hire_broker : public ScriptedAI
{
    explicit npc_fb_hire_broker(Creature* creature) : ScriptedAI(creature) { }

    Hire::Broker BrokerKind() const { return Hire::BrokerOfEntry(me->GetEntry()); }

    void MainMenu(Player* player)
    {
        StartMenu(player);
        Hire::Broker const broker = BrokerKind();
        for (Hire::Grade g : { Hire::Grade::Sellsword, Hire::Grade::Veteran, Hire::Grade::Legend })
        {
            if (!Hire::BrokerSells(HireMgr::ConfigRules(), broker, g))
                continue;
            if (uint64 const price = HireMgr::PriceHere(player, g))
                AddOption(player, GradeOffer(g, price), kActionGrade + uint32(g) - 1);
            else
                AddOption(player, Trinity::StringFormat("({} only sign on with level {} heroes.)", g == Hire::Grade::Legend ? "Legends" : "Veterans",
                                                        uint32(Playerbot::MaxPlayerLevel())), 0);
        }
        AddOption(player, "Who works for me?", kActionContracts);
        if (HireMgr::ConfigMode() == Hire::Mode::PlayerChoice)
            AddOption(player,
                      HireMgr::Instance().OptedIn(player->GetSession()->GetAccountId())
                          ? "I'd rather gather my own company again. (Bots are free again.)"
                          : "From now on I only fight beside those I pay. (Hired bots only.)",
                      kActionOptIn);
        TC_LOG_INFO("playerbot.v2", "[hire] {} talks to broker {} ({}): menu {} with {} options", player->GetName(), me->GetName(), me->GetEntry(),
                    kBrokerMenu, player->PlayerTalkClass->GetGossipMenu().GetMenuItemCount());
        SendGossipMenuFor(player, kBrokerText, me->GetGUID());
    }

    bool OnGossipHello(Player* player) override
    {
        MainMenu(player);
        return true;
    }

    bool OnGossipSelect(Player* player, uint32 /*menuId*/, uint32 gossipListId) override
    {
        uint32 const action = GetGossipActionFor(player, gossipListId);
        StartMenu(player);
        Hire::Broker const broker = BrokerKind();

        if (action == kActionContracts)
        {
            CloseGossipMenuFor(player);
            ListContracts(player);
            return true;
        }
        if (action == kActionOptIn)
        {
            uint32 const account = player->GetSession()->GetAccountId();
            bool const on = !HireMgr::Instance().OptedIn(account);
            HireMgr::Instance().SetOptedIn(account, on);
            CloseGossipMenuFor(player);
            Tell(player, on ? "Hired bots only: your companions now come from the brokers." : "Hired bots off: you can bring bots for free again.");
            return true;
        }
        if (action >= kActionGrade && action < kActionGrade + 3)
        {
            Hire::Grade const grade = Hire::Grade(action - kActionGrade + 1);
            if (!Hire::BrokerSells(HireMgr::ConfigRules(), broker, grade))
                return MainMenuAgain(player);
            for (uint32 r = 1; r <= 3; ++r)
                AddOption(player, Trinity::StringFormat("A {}.", RoleWord(RoleOfIndex(r))), kActionRole + uint32(grade) * 10 + r);
            SendGossipMenuFor(player, kBrokerText, me->GetGUID());
            return true;
        }
        if (action >= kActionRole + 10 && action < kActionRole + 40)
        {
            Hire::Grade const grade = Hire::Grade((action - kActionRole) / 10);
            uint32 const roleIndex = (action - kActionRole) % 10;
            Playerbot::Role const role = RoleOfIndex(roleIndex);
            uint64 const price = HireMgr::PriceHere(player, grade);
            if (!Hire::BrokerSells(HireMgr::ConfigRules(), broker, grade) || !price || roleIndex < 1 || roleIndex > 3)
                return MainMenuAgain(player);
            for (ClassInfo const& c : kClasses)
                if (Plays(c, role) && CanHaveCompanionClass(player, c.cls))
                    AddOption(player, c.name, kActionHire + uint32(grade) * 100 + roleIndex * 20 + c.cls,
                              Trinity::StringFormat("Hire a {} {} ({})?", c.name, Hire::GradeName(grade), RoleWord(role)), price);
            SendGossipMenuFor(player, kBrokerText, me->GetGUID());
            return true;
        }
        if (action >= kActionHire + 100 && action < kActionHire + 400)
        {
            uint32 const rest = action - kActionHire;
            Hire::Grade const grade = Hire::Grade(rest / 100);
            Playerbot::Role const role = RoleOfIndex((rest % 100) / 20);
            ClassInfo const* c = FindClass(uint8(rest % 20));
            CloseGossipMenuFor(player);
            if (!c || !Hire::BrokerSells(HireMgr::ConfigRules(), broker, grade) || !Plays(*c, role))
                return true;
            std::string msg;
            bool const ok = HireCompanionForOwner(player, c->cls, role, grade, c->name, msg);
            if (!msg.empty())
                Tell(player, msg);
            if (ok)
                me->Say(HiredLine(broker), LANG_UNIVERSAL, player);
            return true;
        }
        CloseGossipMenuFor(player);
        return true;
    }

private:
    bool MainMenuAgain(Player* player)
    {
        MainMenu(player);
        return true;
    }
};

void AddSC_playerbot_v2_hire_broker()
{
    RegisterCreatureAI(npc_fb_hire_broker);
}
