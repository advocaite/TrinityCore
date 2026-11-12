/*
 * `.playerbot selftest ...`: console checks for fixes that used to need a player at the keyboard. Each one drives an online
 * character (a world bot is fine) through the same server code a client would reach, and prints PASS / FAIL lines that a
 * script can grep. Nothing here changes data in the database; quests and items a check gives are taken back at the end
 * unless the check says otherwise.
 *
 * Registered by AddSC_playerbot_v2 (PlayerbotV2Scripts.cpp). The commands sit under the same `.playerbot` root as
 * cs_playerbot_v2.cpp; the command tree merges both tables.
 */

#include "ScriptMgr.h"
#include "AreaTriggerPackets.h"
#include "CharacterPackets.h"
#include "Chat.h"
#include "ChatCommand.h"
#include "Creature.h"
#include "CreatureData.h"
#include "DB2Stores.h"
#include "GossipDef.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "PathGenerator.h"
#include "Player.h"
#include "QuestDef.h"
#include "RBAC.h"
#include "SpellAuras.h"
#include "SpellMgr.h"
#include "StringFormat.h"
#include "Trainer.h"
#include "WorldSession.h"
#include "PlayerbotMovement.h"

#include <list>
#include <sstream>

using namespace Trinity::ChatCommands;

namespace
{

Player* FindOnline(ChatHandler* handler, std::string name, char const* what)
{
    Player* p = normalizePlayerName(name) ? ObjectAccessor::FindPlayerByName(name) : nullptr;
    if (!p || !p->IsInWorld())
    {
        handler->PSendSysMessage("selftest %s: no online character named '%s'", what, name.c_str());
        handler->SetSentErrorMessage(true);
        return nullptr;
    }
    return p;
}

char const* StatusName(QuestStatus s)
{
    switch (s)
    {
        case QUEST_STATUS_NONE:       return "none";
        case QUEST_STATUS_COMPLETE:   return "complete";
        case QUEST_STATUS_INCOMPLETE: return "incomplete";
        case QUEST_STATUS_FAILED:     return "failed";
        case QUEST_STATUS_REWARDED:   return "rewarded";
        default:                      return "?";
    }
}

char const* NpcOptionName(GossipOptionNpc o)
{
    switch (o)
    {
        case GossipOptionNpc::None:    return "talk";
        case GossipOptionNpc::Vendor:  return "VENDOR";
        case GossipOptionNpc::Trainer: return "TRAINER";
        case GossipOptionNpc::Binder:  return "innkeeper";
        case GossipOptionNpc::Banker:  return "banker";
        case GossipOptionNpc::Taxinode:return "flight";
        default:                       return "other";
    }
}

// Every objective of the quest with what the character has for it, one line each.
void PrintObjectives(ChatHandler* handler, Player* p, Quest const* q)
{
    uint16 const slot = p->FindQuestSlot(q->GetQuestId());
    for (QuestObjective const& o : q->GetObjectives())
    {
        int32 have = slot < MAX_QUEST_LOG_SIZE ? p->GetQuestSlotObjectiveData(slot, o) : 0;
        std::string extra;
        if (o.Type == QUEST_OBJECTIVE_ITEM)
            extra = Trinity::StringFormat(", in bags {}", p->GetItemCount(uint32(o.ObjectID)));
        bool const done = slot < MAX_QUEST_LOG_SIZE && p->IsQuestObjectiveComplete(slot, q, o);
        handler->PSendSysMessage("  objective %u type %u object %d: %d/%d%s %s", o.ID, uint32(o.Type), o.ObjectID, have, o.Amount,
            extra.c_str(), done ? "done" : "open");
    }
}

// The area trigger a quest's exploration objective (type 10) or an areatrigger_involvedrelation row points at.
AreaTriggerEntry const* QuestTrigger(Quest const* q)
{
    for (QuestObjective const& o : q->GetObjectives())
        if (o.Type == QUEST_OBJECTIVE_AREATRIGGER && o.ObjectID > 0)
            if (AreaTriggerEntry const* at = sAreaTriggerStore.LookupEntry(uint32(o.ObjectID)))
                return at;
    for (AreaTriggerEntry const* at : sAreaTriggerStore)
        if (std::unordered_set<uint32> const* quests = sObjectMgr->GetQuestsForAreaTrigger(at->ID))
            if (quests->count(q->GetQuestId()))
                return at;
    return nullptr;
}

Creature* NearestOfEntry(Player* p, uint32 entry, float range)
{
    std::list<Creature*> found;
    p->GetCreatureListWithEntryInGrid(found, entry, range);
    Creature* best = nullptr;
    for (Creature* c : found)
        if (!best || p->GetDistance(c) < p->GetDistance(best))
            best = c;
    return best;
}

CreatureData const* FirstSpawn(uint32 entry)
{
    for (auto const& [guid, data] : sObjectMgr->GetAllCreatureData())
        if (data.id == entry)
            return &data;
    return nullptr;
}

} // namespace

class playerbot_v2_selftest_commandscript : public CommandScript
{
public:
    playerbot_v2_selftest_commandscript() : CommandScript("playerbot_v2_selftest_commandscript") {}

    std::span<ChatCommandBuilder const> GetCommands() const override
    {
        static ChatCommandTable selftestTable =
        {
            { "quest",       HandleQuest,       rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "explore",     HandleExplore,     rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "explorelist", HandleExploreList, rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "itemobjective", HandleItemObjective, rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "npc",         HandleNpc,         rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "path",        HandlePath,        rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "namegen",     HandleNameGen,     rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "cast",        HandleCast,        rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "auras",       HandleAuras,       rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
        };
        static ChatCommandTable playerbotTable =
        {
            { "selftest", selftestTable },
        };
        static ChatCommandTable root =
        {
            { "playerbot", playerbotTable },
        };
        return root;
    }

    // .playerbot selftest quest <name> <quest> [accept <creature entry> | add | drop]
    // Can the character take the quest, and if not, which rule says no (level, race, class, previous quest, exclusive group,
    // repeatable, conditions). "add" gives it (start items included, as a turn-in NPC would), "accept <entry>" gives it from
    // the nearest creature of that entry within 30 yd so the NPC's own accept script runs (escorts), "drop" takes it off and
    // forgets that it was done. Always prints the status and every objective after.
    static bool HandleQuest(ChatHandler* handler, std::string name, uint32 questId, Optional<std::string> verb, Optional<uint32> entry)
    {
        Player* p = FindOnline(handler, name, "quest");
        Quest const* q = sObjectMgr->GetQuestTemplate(questId);
        if (!p)
            return false;
        if (!q)
        {
            handler->PSendSysMessage("selftest quest: quest %u does not exist -> FAIL", questId);
            return false;
        }
        handler->PSendSysMessage("selftest quest: %s (level %u, race %u, class %u) quest %u '%s' status %s, repeatable %s, rewarded before %s",
            p->GetName().c_str(), uint32(p->GetLevel()), uint32(p->GetRace()), uint32(p->GetClass()), questId, q->GetLogTitle().c_str(),
            StatusName(p->GetQuestStatus(questId)), q->IsRepeatable() ? "yes" : "no", p->IsQuestRewarded(questId) ? "yes" : "no");

        struct Rule { char const* name; bool ok; };
        Rule const rules[] =
        {
            { "status",          p->SatisfyQuestStatus(q, false) },
            { "min level",       p->SatisfyQuestMinLevel(q, false) },
            { "max level",       p->SatisfyQuestMaxLevel(q, false) },
            { "race",            p->SatisfyQuestRace(q, false) },
            { "class",           p->SatisfyQuestClass(q, false) },
            { "skill",           p->SatisfyQuestSkill(q, false) },
            { "reputation",      p->SatisfyQuestReputation(q, false) },
            { "previous quest",  p->SatisfyQuestPreviousQuest(q, false) },
            { "dependent quests",p->SatisfyQuestDependentQuests(q, false) },
            { "breadcrumb",      p->SatisfyQuestBreadcrumbQuest(q, false) },
            { "exclusive group", p->SatisfyQuestExclusiveGroup(q, false) },
            { "conditions",      p->SatisfyQuestConditions(q, false) },
            { "daily/weekly",    p->SatisfyQuestDay(q, false) && p->SatisfyQuestWeek(q, false) && p->SatisfyQuestMonth(q, false) && p->SatisfyQuestSeasonal(q, false) },
            { "expansion",       p->SatisfyQuestExpansion(q, false) },
        };
        std::string refused;
        for (Rule const& r : rules)
            if (!r.ok)
                refused += std::string(refused.empty() ? "" : ", ") + r.name;
        bool const canTake = p->CanTakeQuest(q, false);
        handler->PSendSysMessage("  can take: %s%s%s", canTake ? "yes" : "no", refused.empty() ? "" : "; refused by: ", refused.c_str());

        std::string const v = verb.value_or("");
        if (v == "drop")
        {
            if (p->GetQuestStatus(questId) != QUEST_STATUS_NONE)
                p->RemoveActiveQuest(questId, true);
            p->RemoveRewardedQuest(questId, true);
            handler->PSendSysMessage("  dropped: status now %s", StatusName(p->GetQuestStatus(questId)));
            return true;
        }
        if (v == "add" || v == "accept")
        {
            Object* giver = nullptr;
            if (v == "accept")
            {
                Creature* c = entry ? NearestOfEntry(p, *entry, 30.0f) : nullptr;
                if (!c)
                {
                    handler->PSendSysMessage("  accept: no creature %u within 30 yd of %s -> FAIL", entry.value_or(0), p->GetName().c_str());
                    return false;
                }
                giver = c;
            }
            if (p->GetQuestStatus(questId) == QUEST_STATUS_NONE || (q->IsRepeatable() && p->GetQuestStatus(questId) == QUEST_STATUS_REWARDED))
            {
                if (!p->CanAddQuest(q, false))
                {
                    handler->PSendSysMessage("  add: the quest log or bags refuse it -> FAIL");
                    return false;
                }
                p->AddQuestAndCheckCompletion(q, giver);
            }
            handler->PSendSysMessage("  %s: status now %s; start item %u in bags %u", v.c_str(), StatusName(p->GetQuestStatus(questId)),
                q->GetSrcItemId(), q->GetSrcItemId() ? p->GetItemCount(q->GetSrcItemId()) : 0);
        }
        PrintObjectives(handler, p, q);
        return true;
    }

    // .playerbot selftest explore <name> <quest>
    // An exploration quest end to end, the way a client walks into it. First call: gives the quest if the character lacks it
    // and moves the character to the middle of the quest's area trigger (client AreaTrigger.db2 position); "run again".
    // Second call (character inside the trigger now): sends the same CMSG_AREA_TRIGGER the client sends on entering and
    // prints whether the objective was credited, then takes the quest back off. The "inside" line is the server's own
    // shape test (sphere, box, polygon at the trigger's orientation). With "check" it never moves the character: when it is
    // not inside it prints where it stands against the trigger (3D and flat distance, height of the trigger's middle above
    // the floor), which tells a world bot that walked off from a trigger a player on foot can't reach.
    static bool HandleExplore(ChatHandler* handler, std::string name, uint32 questId, Optional<std::string> verb)
    {
        Player* p = FindOnline(handler, name, "explore");
        Quest const* q = sObjectMgr->GetQuestTemplate(questId);
        if (!p)
            return false;
        if (!q)
        {
            handler->PSendSysMessage("selftest explore: quest %u does not exist -> FAIL", questId);
            return false;
        }
        AreaTriggerEntry const* at = QuestTrigger(q);
        if (!at)
        {
            handler->PSendSysMessage("selftest explore: quest %u '%s' has no area trigger (no type 10 objective in the client store, no areatrigger_involvedrelation row) -> FAIL",
                questId, q->GetLogTitle().c_str());
            return false;
        }
        if (p->GetQuestStatus(questId) == QUEST_STATUS_NONE)
        {
            p->RemoveRewardedQuest(questId, true);
            p->AddQuestAndCheckCompletion(q, nullptr);
        }
        bool const inside = p->GetMapId() == at->ContinentID && p->IsInAreaTrigger(at);
        if (!inside && verb && *verb == "check")
        {
            Position const mid(at->Pos.X, at->Pos.Y, at->Pos.Z);
            float floor = INVALID_HEIGHT;
            if (p->GetMapId() == at->ContinentID)
                floor = p->GetMap()->GetHeight(p->GetPhaseShift(), at->Pos.X, at->Pos.Y, at->Pos.Z + 5.0f, true, 100.0f);
            handler->PSendSysMessage("selftest explore: %s quest %u trigger %u NOT inside: map %u vs %u, 3D %.1f / flat %.1f yd from the middle (radius %.1f), "
                "middle z %.1f, floor under it %.1f, %s at z %.1f -> %s", p->GetName().c_str(), questId, at->ID, p->GetMapId(), uint32(at->ContinentID),
                p->GetMapId() == at->ContinentID ? p->GetExactDist(&mid) : -1.0f, p->GetMapId() == at->ContinentID ? p->GetExactDist2d(&mid) : -1.0f,
                at->Radius, at->Pos.Z, floor > INVALID_HEIGHT ? floor : 0.0f, p->GetName().c_str(), p->GetPositionZ(),
                (floor > INVALID_HEIGHT && at->GetShapeType() == AreaTriggerShapeType::Sphere && at->Pos.Z - floor > at->Radius)
                    ? "FAIL (the middle is higher above the floor than the radius: nobody on foot gets in)" : "NOT RUN (moved away; run again)");
            p->RemoveActiveQuest(questId, true);
            p->RemoveRewardedQuest(questId, true);
            return true;
        }
        if (!inside)
        {
            float x = at->Pos.X, y = at->Pos.Y, z = at->Pos.Z;
            if (p->GetMapId() == at->ContinentID)
            {
                float const floor = p->GetMap()->GetHeight(p->GetPhaseShift(), x, y, z + 5.0f, true, 50.0f);
                // stand on the floor when a player standing there is inside the trigger; otherwise drop in at the middle
                if (floor > INVALID_HEIGHT && (at->GetShapeType() != AreaTriggerShapeType::Sphere || z - floor < at->Radius - 1.0f))
                    z = floor + 0.5f;
            }
            bool const ok = Playerbot::BotMovement::SafeTeleport(p, at->ContinentID, x, y, z, p->GetOrientation(), TELE_TO_GM_MODE);
            handler->PSendSysMessage("selftest explore: %s quest %u '%s' trigger %u (map %u, shape %d, radius %.1f, box %.1fx%.1f yaw %.2f): moved %s to its middle, run again in 3 s",
                p->GetName().c_str(), questId, q->GetLogTitle().c_str(), at->ID, uint32(at->ContinentID), int32(at->ShapeType), at->Radius,
                at->BoxLength, at->BoxWidth, at->BoxYaw, ok ? "" : "(TELEPORT REFUSED)");
            return ok;
        }
        WorldPackets::AreaTrigger::AreaTrigger packet{ WorldPacket(CMSG_AREA_TRIGGER) };
        packet.AreaTriggerID = int32(at->ID);
        packet.Entered = true;
        packet.FromClient = true;
        p->GetSession()->HandleAreaTriggerOpcode(packet);
        QuestStatus const st = p->GetQuestStatus(questId);
        // Most of these quests have other steps too (kill, collect): the explore step itself is what must tick. A quest with
        // only an areatrigger_involvedrelation row (no type 10 objective) must turn complete.
        uint16 const slot = p->FindQuestSlot(questId);
        uint32 exploreSteps = 0, exploreDone = 0;
        for (QuestObjective const& o : q->GetObjectives())
            if (o.Type == QUEST_OBJECTIVE_AREATRIGGER)
            {
                ++exploreSteps;
                if (slot < MAX_QUEST_LOG_SIZE && p->IsQuestObjectiveComplete(slot, q, o))
                    ++exploreDone;
            }
        bool const pass = exploreSteps ? exploreDone == exploreSteps : (st == QUEST_STATUS_COMPLETE || st == QUEST_STATUS_REWARDED);
        handler->PSendSysMessage("selftest explore: %s quest %u '%s' trigger %u: inside yes; after entering status %s, explore steps %u/%u done -> %s",
            p->GetName().c_str(), questId, q->GetLogTitle().c_str(), at->ID, StatusName(st), exploreDone, exploreSteps, pass ? "PASS" : "FAIL");
        PrintObjectives(handler, p, q);
        p->RemoveActiveQuest(questId, true);
        p->RemoveRewardedQuest(questId, true);
        return true;
    }

    // .playerbot selftest explorelist
    // Read-only: every quest with an exploration objective or an areatrigger_involvedrelation row, and whether its trigger
    // exists in the client's AreaTrigger store (a missing one can never be entered). One line per quest, then a count.
    static bool HandleExploreList(ChatHandler* handler)
    {
        uint32 quests = 0, missing = 0;
        for (auto const& [id, quest] : sObjectMgr->GetQuestTemplates())
        {
            Quest const* q = quest.get();
            bool wants = false;
            for (QuestObjective const& o : q->GetObjectives())
                if (o.Type == QUEST_OBJECTIVE_AREATRIGGER)
                    wants = true;
            AreaTriggerEntry const* at = QuestTrigger(q);
            if (!wants && !at)
                continue;
            ++quests;
            if (!at)
                ++missing;
            handler->PSendSysMessage("explorelist: quest %u '%s' trigger %u map %d -> %s", id, q->GetLogTitle().c_str(), at ? at->ID : 0,
                at ? int32(at->ContinentID) : -1, at ? "PASS" : "FAIL (no trigger)");
        }
        handler->PSendSysMessage("explorelist: %u quests, %u without a usable trigger", quests, missing);
        return true;
    }

    // .playerbot selftest itemobjective <name> <quest> <item> [count]
    // Do collected quest items follow the bags (Zephras ears, test-0136 0053)? Gives the quest, puts `count` items in the bags,
    // reads the objective, destroys them, reads it again, then takes quest and leftovers back. PASS when the objective drops
    // back to what is left in the bags.
    static bool HandleItemObjective(ChatHandler* handler, std::string name, uint32 questId, uint32 itemId, Optional<uint32> countArg)
    {
        Player* p = FindOnline(handler, name, "itemobjective");
        Quest const* q = sObjectMgr->GetQuestTemplate(questId);
        if (!p || !q || !sObjectMgr->GetItemTemplate(itemId))
        {
            if (p)
                handler->PSendSysMessage("selftest itemobjective: unknown quest %u or item %u -> FAIL", questId, itemId);
            return false;
        }
        QuestObjective const* obj = nullptr;
        for (QuestObjective const& o : q->GetObjectives())
            if (o.Type == QUEST_OBJECTIVE_ITEM && uint32(o.ObjectID) == itemId)
                obj = &o;
        if (!obj)
        {
            handler->PSendSysMessage("selftest itemobjective: quest %u has no item objective for %u -> FAIL", questId, itemId);
            return false;
        }
        uint32 const count = std::max<uint32>(1, countArg.value_or(uint32(std::max(1, obj->Amount - 1))));
        if (p->GetQuestStatus(questId) != QUEST_STATUS_NONE)
            p->RemoveActiveQuest(questId, true);
        p->RemoveRewardedQuest(questId, true);
        p->DestroyItemCount(itemId, p->GetItemCount(itemId, true), true);
        p->AddQuestAndCheckCompletion(q, nullptr);

        bool const stored = p->StoreNewItemInBestSlots(itemId, count, ItemContext::NONE);
        uint16 const slot = p->FindQuestSlot(questId);
        int32 const after_add = slot < MAX_QUEST_LOG_SIZE ? p->GetQuestSlotObjectiveData(slot, *obj) : -1;
        p->DestroyItemCount(itemId, count, true);
        int32 const after_destroy = slot < MAX_QUEST_LOG_SIZE ? p->GetQuestSlotObjectiveData(slot, *obj) : -1;
        uint32 const left = p->GetItemCount(itemId, true);
        bool const pass = stored && after_add == int32(std::min<uint32>(count, uint32(obj->Amount))) && after_destroy == int32(left);
        handler->PSendSysMessage("selftest itemobjective: %s quest %u item %u: %u stored %s, objective %d after adding, %d after destroying (bags %u) -> %s",
            p->GetName().c_str(), questId, itemId, count, stored ? "ok" : "REFUSED", after_add, after_destroy, left, pass ? "PASS" : "FAIL");
        p->RemoveActiveQuest(questId, true);
        return true;
    }

    // .playerbot selftest npc <name> <creature entry> [go | show | learn <spell>]
    // go: moves the character next to the creature's first spawn ("run show in 3 s"). show: the creature's npc flags, the gossip
    // menu the character gets (each option with its kind: TRAINER / VENDOR / talk, and the trainer an option opens), the number
    // of goods and of trainer spells. learn: the trainer teaches the spell as the trainer window's "Train" button does, and the
    // line says whether the character knows it after.
    static bool HandleNpc(ChatHandler* handler, std::string name, uint32 entry, Optional<std::string> verb, Optional<uint32> spellId)
    {
        Player* p = FindOnline(handler, name, "npc");
        if (!p)
            return false;
        CreatureTemplate const* tmpl = sObjectMgr->GetCreatureTemplate(entry);
        if (!tmpl)
        {
            handler->PSendSysMessage("selftest npc: creature %u does not exist -> FAIL", entry);
            return false;
        }
        std::string const v = verb.value_or("show");
        if (v == "go")
        {
            CreatureData const* d = FirstSpawn(entry);
            if (!d)
            {
                handler->PSendSysMessage("selftest npc: %u '%s' has no spawn -> FAIL", entry, tmpl->Name.c_str());
                return false;
            }
            Position const& pos = d->spawnPoint;
            float const o = pos.GetOrientation();
            float const x = pos.GetPositionX() + 2.0f * std::cos(o), y = pos.GetPositionY() + 2.0f * std::sin(o);
            bool const ok = Playerbot::BotMovement::SafeTeleport(p, d->mapId, x, y, pos.GetPositionZ() + 0.5f, o + float(M_PI), TELE_TO_GM_MODE);
            handler->PSendSysMessage("selftest npc: %s moved to %u '%s' (map %u %.1f %.1f %.1f)%s, run show in 3 s", p->GetName().c_str(), entry,
                tmpl->Name.c_str(), uint32(d->mapId), pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(), ok ? "" : " TELEPORT REFUSED");
            return ok;
        }
        Creature* c = NearestOfEntry(p, entry, 40.0f);
        if (!c)
        {
            handler->PSendSysMessage("selftest npc: no %u '%s' within 40 yd of %s (run go first) -> FAIL", entry, tmpl->Name.c_str(), p->GetName().c_str());
            return false;
        }
        uint32 const trainerId = sObjectMgr->GetCreatureDefaultTrainer(entry);
        Trainer::Trainer const* trainer = trainerId ? sObjectMgr->GetTrainer(trainerId) : nullptr;
        if (v == "learn")
        {
            if (!trainer || !spellId)
            {
                handler->PSendSysMessage("selftest npc learn: %s", !trainer ? "the creature has no trainer -> FAIL" : "give a spell id");
                return false;
            }
            bool const before = p->HasSpell(*spellId);
            trainer->TeachSpell(c, p, *spellId);
            bool const after = p->HasSpell(*spellId);
            handler->PSendSysMessage("selftest npc learn: %s spell %u from %u '%s' (trainer %u): known before %s, after %s -> %s", p->GetName().c_str(),
                *spellId, entry, tmpl->Name.c_str(), trainerId, before ? "yes" : "no", after ? "yes" : "no", after ? "PASS" : "FAIL");
            return true;
        }
        uint32 const menuId = c->GetGossipMenuId();
        p->PrepareGossipMenu(c, menuId, true);
        GossipMenuItemContainer const& items = p->PlayerTalkClass->GetGossipMenu().GetMenuItems();
        VendorItemData const* goods = sObjectMgr->GetNpcVendorItemList(entry);
        handler->PSendSysMessage("selftest npc: %u '%s' npcflag 0x%llx, gossip menu %u with %u options; goods %u; default trainer %u with %u spells",
            entry, tmpl->Name.c_str(), (unsigned long long)c->GetNpcFlags(), menuId, uint32(items.size()), goods ? goods->GetItemCount() : 0,
            trainerId, trainer ? uint32(trainer->GetSpells().size()) : 0);
        for (GossipMenuItem const& item : items)
        {
            uint32 const optTrainer = item.OptionNpc == GossipOptionNpc::Trainer
                ? sObjectMgr->GetCreatureTrainerForGossipOption(entry, menuId, item.OrderIndex) : 0;
            handler->PSendSysMessage("  option %u (id %d) %s '%s'%s", item.OrderIndex, item.GossipOptionID, NpcOptionName(item.OptionNpc),
                item.OptionText.c_str(), optTrainer ? Trinity::StringFormat(" -> trainer {}", optTrainer).c_str() : "");
        }
        p->PlayerTalkClass->SendCloseGossip();
        return true;
    }

    // .playerbot selftest path <name> <creature entry> <x> <y> <z>
    // The path the nearest creature of that entry (within 100 yd of the character) would take to x y z, point by point, with
    // the floor and water surface under each point. For land creatures that cannot swim (crabs) the points under water must
    // sit on the floor ("bed"), not on the surface ("SURFACE").
    static bool HandlePath(ChatHandler* handler, std::string name, uint32 entry, float x, float y, float z)
    {
        Player* p = FindOnline(handler, name, "path");
        if (!p)
            return false;
        Creature* c = NearestOfEntry(p, entry, 100.0f);
        if (!c)
        {
            handler->PSendSysMessage("selftest path: no creature %u within 100 yd of %s -> FAIL", entry, p->GetName().c_str());
            return false;
        }
        PathGenerator path(c);
        bool const found = path.CalculatePath(x, y, z, false);
        Movement::PointsArray const& pts = path.GetPath();
        uint32 underwater = 0, onBed = 0, atSurface = 0;
        Map* map = c->GetMap();
        for (size_t i = 0; i < pts.size(); ++i)
        {
            G3D::Vector3 const& v = pts[i];
            float const floor = map->GetHeight(c->GetPhaseShift(), v.x, v.y, v.z + 2.0f, true, 50.0f);
            float const water = map->GetWaterLevel(c->GetPhaseShift(), v.x, v.y);
            bool const wet = water > INVALID_HEIGHT && floor > INVALID_HEIGHT && water > floor + 0.5f;
            char const* where = "land";
            if (wet)
            {
                ++underwater;
                if (std::fabs(v.z - floor) < 1.0f) { ++onBed; where = "bed"; }
                else if (std::fabs(v.z - water) < 1.0f) { ++atSurface; where = "SURFACE"; }
                else where = "MIDWATER";
            }
            handler->PSendSysMessage("  point %u: %.1f %.1f %.1f floor %.1f water %.1f %s", uint32(i), v.x, v.y, v.z, floor,
                water > INVALID_HEIGHT ? water : 0.0f, where);
        }
        bool const pass = found && underwater > 0 && onBed == underwater;
        handler->PSendSysMessage("selftest path: %u '%s' (swims %s) %u points, path type %u, %u under water: %u on the bed, %u at the surface -> %s",
            entry, c->GetName().c_str(), c->CanSwim() ? "yes" : "no", uint32(pts.size()), uint32(path.GetPathType()), underwater, onBed, atSurface,
            underwater == 0 ? "NO WATER ON THIS PATH (pick another point)" : pass ? "PASS" : "FAIL");
        return true;
    }

    // .playerbot selftest namegen <race> <sex 0|1> [count]
    // What the character-creation dice gets from the server for that race and sex (the same call the opcode handler makes).
    static bool HandleNameGen(ChatHandler* handler, uint8 race, uint8 sex, Optional<uint32> count)
    {
        uint32 const n = std::min<uint32>(20, count.value_or(5));
        uint32 two = 0;
        for (uint32 i = 0; i < n; ++i)
        {
            std::string const nm = sDB2Manager.GetNameGenEntry(race, sex);
            bool const hasSurname = nm.find(' ') != std::string::npos;
            two += hasSurname ? 1 : 0;
            handler->PSendSysMessage("  namegen race %u sex %u: '%s'%s", uint32(race), uint32(sex), nm.c_str(), hasSurname ? " (name + surname)" : "");
        }
        handler->PSendSysMessage("selftest namegen: %u of %u with a surname", two, n);
        return true;
    }

    // .playerbot selftest cast <name> <spell> [enemy]: the character casts the spell on itself (or with "enemy" on its target or the
    // nearest enemy within 30 yd), triggered (no cost, no cast time, no cooldown), as
    // using the item or toy would; for objects and area triggers a spell puts down (sleeping bag, campfire, bear trap).
    static bool HandleCast(ChatHandler* handler, std::string name, uint32 spellId, Optional<std::string> at)
    {
        Player* p = FindOnline(handler, name, "cast");
        if (!p)
            return false;
        if (!sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE))
        {
            handler->PSendSysMessage("selftest cast: spell %u does not exist -> FAIL", spellId);
            return false;
        }
        Unit* target = p;
        if (at && *at == "enemy")
        {
            target = p->GetVictim() ? p->GetVictim() : p->SelectNearbyTarget(nullptr, 30.0f);
            if (!target)
            {
                handler->PSendSysMessage("selftest cast: no enemy within 30 yd of %s (move it next to mobs first) -> NOT RUN", p->GetName().c_str());
                return false;
            }
        }
        SpellCastResult const r = p->CastSpell(target, spellId, true);
        handler->PSendSysMessage("selftest cast: %s cast %u on %s -> result %u%s", p->GetName().c_str(), spellId, target->GetName().c_str(), uint32(r),
            r == SPELL_CAST_OK ? " (ok)" : "");
        return r == SPELL_CAST_OK;
    }

    // .playerbot selftest auras <name> [spell]: whether the character is resting, and its auras (or only whether it has `spell`).
    static bool HandleAuras(ChatHandler* handler, std::string name, Optional<uint32> spellId)
    {
        Player* p = FindOnline(handler, name, "auras");
        if (!p)
            return false;
        bool const resting = p->HasPlayerFlag(PLAYER_FLAGS_RESTING);
        if (spellId)
        {
            Aura const* aura = p->GetAura(*spellId);
            handler->PSendSysMessage("selftest auras: %s resting %s, aura %u %s (stacks %u) -> %s", p->GetName().c_str(), resting ? "yes" : "no", *spellId,
                aura ? "on" : "off", aura ? uint32(aura->GetStackAmount()) : 0, aura ? "PASS" : "FAIL");
            return true;
        }
        std::ostringstream ss;
        for (auto const& [id, app] : p->GetAppliedAuras())
            ss << ' ' << id;
        handler->PSendSysMessage("selftest auras: %s resting %s, auras:%s", p->GetName().c_str(), resting ? "yes" : "no", ss.str().c_str());
        return true;
    }
};

void AddSC_playerbot_v2_selftest_commandscript()
{
    new playerbot_v2_selftest_commandscript();
}
