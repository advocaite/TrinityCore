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

// Spell test module: test characters without a client, and a log of what they cast, deal, gain and proc. It is driven
// with GM commands, which also work from the console and over SOAP (contrib/docker-compose/spelltest.sh, see doc/Docker.md).
// Test characters live on the account SPELLTEST, start on GM Island at level 60, log in with full health and power, and
// do only what they are told.
//
//   .spelltest create NAME RACE CLASS [GENDER] [LEVEL]  a new test character
//   .spelltest login|logout NAME           .spelltest status
//   .spelltest info SPELL                  the spell as the server loaded it: attributes, effects, proc data, scripts
//   .spelltest dummy NAME [YARDS] [ENTRY]  a fresh target dummy in front of the character, facing it
//   .spelltest place NAME OTHER [YARDS]    moves the test character OTHER in front of NAME, facing it
//   .spelltest pvp NAME on|off             free-for-all PvP: two test characters that have it can attack each other
//   .spelltest cast NAME SPELL [TARGET]    a normal cast (cast time, cost, cooldown)
//   .spelltest trigger NAME SPELL [TARGET] a triggered cast (none of that)
//   .spelltest attack NAME [TARGET]        melee        .spelltest stop NAME   stop attacking and casting
//   .spelltest learn|unlearn NAME SPELL    .spelltest equip NAME ITEM
//   .spelltest reset NAME                  alive, cooldowns, health, power, auras that are not passive
//   .spelltest health NAME PERCENT [TARGET]
//   .spelltest auras NAME [TARGET]         .spelltest stats NAME
//   .spelltest log NAME                    prints and clears the character's log: casts, failures, misses, damage,
//                                          ticks, heals, energizes, swings, auras gained and lost, procs
//
// TARGET is dummy, self, none, or the name of another test character. Without one, cast and trigger take the dummy if
// there is one, else the character itself; attack takes the dummy; health and auras take the character itself.

#include "ScriptMgr.h"
#include "AccountMgr.h"
#include "CharacterCache.h"
#include "CharacterPackets.h"
#include "Chat.h"
#include "ChatCommand.h"
#include "CombatLogPackets.h"
#include "Creature.h"
#include "CryptoRandom.h"
#include "DB2Stores.h"
#include "GameTime.h"
#include "Item.h"
#include "Log.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "PassiveAI.h"
#include "PhasingHandler.h"
#include "Player.h"
#include "RBAC.h"
#include "SmartEnum.h"
#include "Spell.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellHistory.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "SpellPackets.h"
#include "SpellTestObserver.h"
#include "SpellTestSession.h"
#include "StringConvert.h"
#include "TemporarySummon.h"
#include "Util.h"
#include "World.h"
#include <fmt/format.h>
#include <algorithm>
#include <atomic>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <unordered_map>

using namespace Trinity::ChatCommands;

namespace
{
char const* const TEST_ACCOUNT = "SPELLTEST";
uint8 const DEFAULT_LEVEL      = 60;
uint32 const NPC_TEST_DUMMY    = 9659;          // Unkillable Test Dummy, level 60 (the vanilla world)
size_t const MAX_LOG_LINES     = 20000;

// GM Island: no player comes here, so nobody sees the test characters
WorldLocation const TEST_LOCATION(1, 16226.2f, 16257.0f, 13.2022f, 1.65007f);

std::string SpellName(uint32 spellId)
{
    if (SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE))
        if (spellInfo->SpellName)
            return (*spellInfo->SpellName)[sWorld->GetDefaultDbcLocale()];
    return "?";
}

std::string SpellText(uint32 spellId)
{
    return fmt::format("{} {}", spellId, SpellName(spellId));
}

std::string CastResultName(int32 result)
{
    if (result == SPELL_CAST_OK)
        return "SPELL_CAST_OK";    // the same value as SPELL_FAILED_SUCCESS

    try
    {
        return EnumUtils::ToConstant(SpellCastResult(result));
    }
    catch (std::out_of_range const&)
    {
        return std::to_string(result);
    }
}

char const* MissName(uint32 missInfo)
{
    static char const* const names[] = { "none", "miss", "resist", "dodge", "parry", "block", "evade", "immune", "immune",
        "deflect", "absorb", "reflect" };
    return missInfo < std::size(names) ? names[missInfo] : "?";
}

char const* RemoveModeName(uint32 removeMode)
{
    static char const* const names[] = { "none", "default", "interrupt", "cancel", "enemy spell", "expire", "death" };
    return removeMode < std::size(names) ? names[removeMode] : "?";
}

std::string NameOf(WorldObject const* context, ObjectGuid guid)
{
    if (guid.IsEmpty())
        return "-";
    if (context)
    {
        if (context->GetGUID() == guid)
            return context->GetName();
        if (WorldObject const* object = ObjectAccessor::GetWorldObject(*context, guid))
            return object->GetName();
    }
    return guid.ToString();
}

std::string DescribeAura(AuraApplication const* aurApp)
{
    Aura const* aura = aurApp->GetBase();
    std::string text = fmt::format("{}/{} ms, stacks {}, charges {}", aura->GetDuration(), aura->GetMaxDuration(),
        uint32(aura->GetStackAmount()), uint32(aura->GetCharges()));
    for (AuraEffect const* aurEff : aura->GetAuraEffects())
    {
        if (!aurEff || !aurApp->HasEffect(aurEff->GetEffIndex()))
            continue;

        text += fmt::format("; e{} aura {} amount {}", uint32(aurEff->GetEffIndex()), uint32(aurEff->GetAuraType()), aurEff->GetAmount());
        if (aurEff->GetPeriod())
            text += fmt::format(" every {} ms", aurEff->GetPeriod());
        if (aurEff->GetMiscValue())
            text += fmt::format(" misc {}", aurEff->GetMiscValue());
    }
    return text;
}

// Never attacks, never moves, never dies: a killing blow restores its health instead. The combat log still shows the
// whole hit, since it is written from the damage before the dummy takes it.
struct SpellTestDummyAI : public NullCreatureAI
{
    using NullCreatureAI::NullCreatureAI;

    void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo*/) override
    {
        if (damage >= me->GetHealth())
        {
            damage = 0;
            me->SetFullHealth();
        }
    }
};

struct TestCharacter
{
    std::string Name;
    std::unique_ptr<SpellTestSession> Session;
    bool Ready = false;                         // in the world, with what a client would have set
    ObjectGuid Dummy;
    uint32 LogStart = 0;                        // game time of the last ".spelltest log"
    std::vector<std::string> Log;
    size_t Dropped = 0;
};

class SpellTestRecorder final : public SpellTestObserver
{
public:
    // Recursive: game calls made under it (casting, logging in) come back through the hooks
    std::recursive_mutex Lock;
    std::unordered_map<ObjectGuid, TestCharacter> Characters;
    std::unordered_map<ObjectGuid, ObjectGuid> Units;       // a test character and its dummy -> the test character
    std::atomic<bool> Active = false;

    void OnCombatLog(WorldObject const* sender, WorldPackets::CombatLog::CombatLogServerPacket const* combatLog) override
    {
        if (!Active)
            return;

        using namespace WorldPackets::CombatLog;
        std::lock_guard lock(Lock);
        if (auto const* go = dynamic_cast<WorldPackets::Spells::SpellGo const*>(combatLog))
        {
            // Hits show up as damage, heals and auras; only the misses are logged here
            WorldPackets::Spells::SpellCastData const& cast = go->Cast;
            for (std::size_t i = 0; i < cast.MissTargets.size(); ++i)
            {
                uint32 reason = i < cast.MissStatus.size() ? cast.MissStatus[i].Reason : 0;
                Record(sender, cast.CasterUnit, cast.MissTargets[i], [&]
                {
                    return fmt::format("miss {} {} -> {}: {}", SpellText(cast.SpellID), NameOf(sender, cast.CasterUnit),
                        NameOf(sender, cast.MissTargets[i]), MissName(reason));
                });
            }
        }
        else if (auto const* damage = dynamic_cast<SpellNonMeleeDamageLog const*>(combatLog))
        {
            Record(sender, damage->CasterGUID, damage->Me, [&]
            {
                return fmt::format("dmg {} {} -> {}: {}{} (before mitigation {}, absorbed {}, resisted {}, blocked {}, school 0x{:X}{})",
                    SpellText(damage->SpellID), NameOf(sender, damage->CasterGUID), NameOf(sender, damage->Me), damage->Damage,
                    (damage->Flags & SPELL_HIT_TYPE_CRIT) ? " crit" : "", damage->OriginalDamage, damage->Absorbed, damage->Resisted,
                    damage->ShieldBlock, uint32(damage->SchoolMask), damage->Periodic ? ", periodic" : "");
            });
        }
        else if (auto const* periodic = dynamic_cast<SpellPeriodicAuraLog const*>(combatLog))
        {
            for (PeriodicAuraLogEffect const& effect : periodic->Effects)
            {
                Record(sender, periodic->CasterGUID, periodic->TargetGUID, [&]
                {
                    return fmt::format("tick {} {} -> {}: aura {} amount {}{} (before mitigation {}, over {}, absorbed {}, resisted {})",
                        SpellText(periodic->SpellID), NameOf(sender, periodic->CasterGUID), NameOf(sender, periodic->TargetGUID),
                        effect.Effect, effect.Amount, effect.Crit ? " crit" : "", effect.OriginalDamage, effect.OverHealOrKill,
                        effect.AbsorbedOrAmplitude, effect.Resisted);
                });
            }
        }
        else if (auto const* heal = dynamic_cast<SpellHealLog const*>(combatLog))
        {
            Record(sender, heal->CasterGUID, heal->TargetGUID, [&]
            {
                return fmt::format("heal {} {} -> {}: {}{} (before mitigation {}, over {}, absorbed {})", SpellText(heal->SpellID),
                    NameOf(sender, heal->CasterGUID), NameOf(sender, heal->TargetGUID), heal->Health, heal->Crit ? " crit" : "",
                    heal->OriginalHeal, heal->OverHeal, heal->Absorbed);
            });
        }
        else if (auto const* energize = dynamic_cast<SpellEnergizeLog const*>(combatLog))
        {
            Record(sender, energize->CasterGUID, energize->TargetGUID, [&]
            {
                return fmt::format("energize {} {} -> {}: {} of power {} (over {})", SpellText(energize->SpellID),
                    NameOf(sender, energize->CasterGUID), NameOf(sender, energize->TargetGUID), energize->Amount,
                    int32(energize->Type), energize->OverEnergize);
            });
        }
        else if (auto const* swing = dynamic_cast<AttackerStateUpdate const*>(combatLog))
        {
            Record(sender, swing->AttackerGUID, swing->VictimGUID, [&]
            {
                return fmt::format("swing {} -> {}: {}{} (before mitigation {}, absorbed {}, resisted {}, blocked {}, victim state {}, hit info 0x{:X}, rage {})",
                    NameOf(sender, swing->AttackerGUID), NameOf(sender, swing->VictimGUID), swing->Damage,
                    (swing->Flags & HITINFO_CRITICALHIT) ? " crit" : "", swing->OriginalDamage, swing->SubDmg ? swing->SubDmg->Absorbed : 0,
                    swing->SubDmg ? swing->SubDmg->Resisted : 0, swing->BlockAmount, uint32(swing->VictimState), swing->Flags,
                    swing->RageGained);
            });
        }
        else if (auto const* shield = dynamic_cast<SpellDamageShield const*>(combatLog))
        {
            Record(sender, shield->Attacker, shield->Defender, [&]
            {
                return fmt::format("damage shield {} {} -> {}: {}", SpellText(shield->SpellID), NameOf(sender, shield->Defender),
                    NameOf(sender, shield->Attacker), shield->TotalDamage);
            });
        }
        else if (auto const* absorb = dynamic_cast<SpellAbsorbLog const*>(combatLog))
        {
            Record(sender, absorb->Caster, absorb->Victim, [&]
            {
                return fmt::format("absorb {} of {} on {} absorbs {} of {}", SpellText(absorb->AbsorbSpellID), NameOf(sender, absorb->Caster),
                    NameOf(sender, absorb->Victim), absorb->Absorbed, SpellText(absorb->AbsorbedSpellID));
            });
        }
        else if (auto const* execute = dynamic_cast<SpellExecuteLog const*>(combatLog))
        {
            Record(sender, execute->Caster, ObjectGuid::Empty, [&]
            {
                return fmt::format("execute {} {}", SpellText(execute->SpellID), NameOf(sender, execute->Caster));
            });
        }
    }

    void OnSpellGo(Spell const* spell) override
    {
        if (!Active)
            return;

        WorldObject const* caster = spell->GetCaster();
        Unit const* target = spell->m_targets.GetUnitTarget();
        std::lock_guard lock(Lock);
        Record(caster, caster->GetGUID(), target ? target->GetGUID() : ObjectGuid::Empty, [&]
        {
            std::string text = fmt::format("cast {} {} -> {}", SpellText(spell->m_spellInfo->Id), caster->GetName(), target ? target->GetName() : "-");
            if (spell->IsTriggered())
                text += " (triggered)";
            if (SpellInfo const* auraSpell = spell->GetTriggeredByAuraSpell())
                text += fmt::format(" by aura {}", SpellText(auraSpell->Id));
            return text;
        });
    }

    void OnCastFailed(Spell const* spell, int32 result) override
    {
        if (!Active)
            return;

        WorldObject const* caster = spell->GetCaster();
        Unit const* target = spell->m_targets.GetUnitTarget();
        std::lock_guard lock(Lock);
        Record(caster, caster->GetGUID(), target ? target->GetGUID() : ObjectGuid::Empty, [&]
        {
            return fmt::format("fail {} {}: {}", SpellText(spell->m_spellInfo->Id), caster->GetName(), CastResultName(result));
        });
    }

    void OnAuraApplied(AuraApplication const* aurApp) override
    {
        if (!Active)
            return;

        Unit const* target = aurApp->GetTarget();
        Aura const* aura = aurApp->GetBase();
        std::lock_guard lock(Lock);
        Record(target, target->GetGUID(), aura->GetCasterGUID(), [&]
        {
            return fmt::format("aura+ {} on {} from {}: {}", SpellText(aura->GetId()), target->GetName(), NameOf(target, aura->GetCasterGUID()),
                DescribeAura(aurApp));
        });
    }

    void OnAuraRemoved(AuraApplication const* aurApp, uint32 removeMode) override
    {
        if (!Active)
            return;

        Unit const* target = aurApp->GetTarget();
        Aura const* aura = aurApp->GetBase();
        std::lock_guard lock(Lock);
        Record(target, target->GetGUID(), aura->GetCasterGUID(), [&]
        {
            return fmt::format("aura- {} on {} from {}: {}", SpellText(aura->GetId()), target->GetName(), NameOf(target, aura->GetCasterGUID()),
                RemoveModeName(removeMode));
        });
    }

    void OnAuraProc(AuraApplication const* aurApp, uint32 procEffectMask, ProcEventInfo const& eventInfo) override
    {
        if (!Active)
            return;

        Unit const* owner = aurApp->GetTarget();
        Unit const* actionTarget = eventInfo.GetActionTarget();
        std::lock_guard lock(Lock);
        Record(owner, owner->GetGUID(), actionTarget ? actionTarget->GetGUID() : ObjectGuid::Empty, [&]
        {
            SpellInfo const* from = eventInfo.GetSpellInfo();
            ProcFlagsInit typeMask = eventInfo.GetTypeMask();
            return fmt::format("proc {} on {} from {} ({} -> {}): type 0x{:X}:0x{:X}, spell type 0x{:X}, phase 0x{:X}, hit 0x{:X}, effects 0x{:X}",
                SpellText(aurApp->GetBase()->GetId()), owner->GetName(), from ? SpellText(from->Id) : std::string("a swing"),
                eventInfo.GetActor() ? eventInfo.GetActor()->GetName() : std::string("-"),
                actionTarget ? actionTarget->GetName() : std::string("-"), uint32(typeMask[0]), uint32(typeMask[1]),
                uint32(eventInfo.GetSpellTypeMask()), uint32(eventInfo.GetSpellPhaseMask()), uint32(eventInfo.GetHitMask()), procEffectMask);
        });
    }

private:
    // The test character an event belongs to: the character itself, its dummy, or something it owns (pet, totem, guardian)
    TestCharacter* Owner(WorldObject const* context, ObjectGuid guid)
    {
        if (guid.IsEmpty())
            return nullptr;

        auto unit = Units.find(guid);
        if (unit == Units.end() && !guid.IsPlayer() && context)
        {
            if (Unit const* object = ObjectAccessor::GetUnit(*context, guid))
            {
                ObjectGuid owner = object->GetCharmerOrOwnerGUID();
                if (owner.IsEmpty())
                    owner = object->GetCreatorGUID();
                if (!owner.IsEmpty())
                    unit = Units.find(owner);
            }
        }
        if (unit == Units.end())
            return nullptr;

        auto character = Characters.find(unit->second);
        return character != Characters.end() ? &character->second : nullptr;
    }

    template<typename Line>
    void Record(WorldObject const* context, ObjectGuid source, ObjectGuid target, Line const& line)
    {
        TestCharacter* first = Owner(context, source);
        TestCharacter* second = Owner(context, target);
        if (!first && !second)
            return;

        std::string text = line();
        if (first)
            Add(*first, text);
        if (second && second != first)
            Add(*second, text);
    }

    static void Add(TestCharacter& character, std::string const& text)
    {
        if (character.Log.size() >= MAX_LOG_LINES)
        {
            ++character.Dropped;
            return;
        }
        character.Log.push_back(fmt::format("{:8.3f} {}", (GameTime::GetGameTimeMS() - character.LogStart) / 1000.0, text));
    }
};

SpellTestRecorder Recorder;

bool Fail(ChatHandler* handler, std::string const& text)
{
    handler->SendSysMessage(text);
    handler->SetSentErrorMessage(true);
    return false;
}

// The account the test characters live on. It is made on first use, with a random password: nobody logs in with it.
uint32 TestAccountId(bool create)
{
    if (uint32 accountId = AccountMgr::GetId(TEST_ACCOUNT))
        return accountId;
    if (!create)
        return 0;

    std::string password = ByteArrayToHexStr(Trinity::Crypto::GetRandomBytes<8>());
    if (sAccountMgr->CreateAccount(TEST_ACCOUNT, password) != AccountOpResult::AOR_OK)
        return 0;
    return AccountMgr::GetId(TEST_ACCOUNT);
}

// The test character's Player, or nullptr after telling the caller why not
Player* TestPlayer(ChatHandler* handler, std::string name, TestCharacter** character = nullptr)
{
    if (!normalizePlayerName(name))
    {
        Fail(handler, "Bad character name");
        return nullptr;
    }

    std::lock_guard lock(Recorder.Lock);
    auto itr = Recorder.Characters.find(sCharacterCache->GetCharacterGuidByName(name));
    if (itr == Recorder.Characters.end())
    {
        Fail(handler, fmt::format("{} is not logged in as a test character: .spelltest login {}", name, name));
        return nullptr;
    }

    Player* player = itr->second.Session->GetPlayer();
    if (!itr->second.Ready || !player || !player->IsInWorld())
    {
        Fail(handler, fmt::format("{} is still logging in", name));
        return nullptr;
    }

    if (character)
        *character = &itr->second;
    return player;
}

// TARGET of the commands: dummy, self, none, or another test character
Unit* Target(ChatHandler* handler, Player* player, TestCharacter const* character, std::string const& which, bool& none)
{
    none = StringEqualI(which, "none");
    if (none)
        return nullptr;
    if (StringEqualI(which, "self"))
        return player;
    if (StringEqualI(which, "dummy"))
    {
        Creature* dummy = ObjectAccessor::GetCreature(*player, character->Dummy);
        if (!dummy)
            Fail(handler, fmt::format("{} has no dummy: .spelltest dummy {}", player->GetName(), player->GetName()));
        return dummy;
    }
    return TestPlayer(handler, which);
}

void Face(Player* player, WorldObject const* target)
{
    if (target != player)
    {
        player->SetOrientation(player->GetAbsoluteAngle(target));
        player->SetSelection(target->GetGUID());
    }
}

void FillHealthAndPower(Player* player)
{
    player->SetFullHealth();
    for (int32 power = 0; power < MAX_POWERS; ++power)
        if (player->GetMaxPower(Powers(power)) > 0)
            player->SetFullPower(Powers(power));
}

// Leveling up raises only the maximum of a weapon skill (the weapons, Defense, Unarmed): raise the skills themselves
void MaxWeaponSkills(Player* player)
{
    for (SkillLineEntry const* skillLine : sSkillLineStore)
    {
        if (skillLine->CategoryID != SKILL_CATEGORY_WEAPON || !player->HasSkill(skillLine->ID))
            continue;

        uint16 maxValue = player->GetPureMaxSkillValue(skillLine->ID);
        player->SetSkill(skillLine->ID, player->GetSkillStep(skillLine->ID), maxValue, maxValue);
    }
}

void LogOut(TestCharacter& character)
{
    if (Player* player = character.Session->GetPlayer())
    {
        if (Creature* dummy = ObjectAccessor::GetCreature(*player, character.Dummy))
            dummy->DespawnOrUnsummon();
        character.Session->LogoutPlayer(true);
    }
}
}

class spell_test_commandscript : public CommandScript
{
public:
    spell_test_commandscript() : CommandScript("spell_test_commandscript") { }

    std::span<ChatCommandBuilder const> GetCommands() const override
    {
        static ChatCommandTable spellTestCommandTable =
        {
            { "create",  HandleCreate,  rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "login",   HandleLogin,   rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "logout",  HandleLogout,  rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "status",  HandleStatus,  rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "info",    HandleInfo,    rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "dummy",   HandleDummy,   rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "place",   HandlePlace,   rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "pvp",     HandlePvp,     rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "cast",    HandleCast,    rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "trigger", HandleTrigger, rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "attack",  HandleAttack,  rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "stop",    HandleStop,    rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "learn",   HandleLearn,   rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "unlearn", HandleUnlearn, rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "equip",   HandleEquip,   rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "reset",   HandleReset,   rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "health",  HandleHealth,  rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "auras",   HandleAuras,   rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "stats",   HandleStats,   rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "log",     HandleLog,     rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
        };
        static ChatCommandTable commandTable =
        {
            { "spelltest", spellTestCommandTable },
        };
        return commandTable;
    }

    static uint8 ParseId(std::string const& text, std::initializer_list<std::pair<char const*, uint8>> names)
    {
        if (Optional<uint8> id = Trinity::StringTo<uint8>(text))
            return *id;
        for (auto const& [name, id] : names)
            if (StringEqualI(text, name))
                return id;
        return 0;
    }

    static bool HandleCreate(ChatHandler* handler, std::string name, std::string race, std::string playerClass, Optional<uint8> gender,
        Optional<uint8> level)
    {
        if (!normalizePlayerName(name))
            return Fail(handler, "Bad character name");
        if (ObjectMgr::CheckPlayerName(name, LOCALE_enUS, true) != CHAR_NAME_SUCCESS || sObjectMgr->IsReservedName(name))
            return Fail(handler, fmt::format("{} is not allowed as a character name", name));
        if (sCharacterCache->GetCharacterCacheByName(name))
            return Fail(handler, fmt::format("{} is taken", name));

        uint8 raceId = ParseId(race, { { "human", RACE_HUMAN }, { "orc", RACE_ORC }, { "dwarf", RACE_DWARF }, { "nightelf", RACE_NIGHTELF },
            { "undead", RACE_UNDEAD_PLAYER }, { "tauren", RACE_TAUREN }, { "gnome", RACE_GNOME }, { "troll", RACE_TROLL } });
        uint8 classId = ParseId(playerClass, { { "warrior", CLASS_WARRIOR }, { "paladin", CLASS_PALADIN }, { "hunter", CLASS_HUNTER },
            { "rogue", CLASS_ROGUE }, { "priest", CLASS_PRIEST }, { "shaman", CLASS_SHAMAN }, { "mage", CLASS_MAGE }, { "warlock", CLASS_WARLOCK },
            { "druid", CLASS_DRUID } });
        if (!raceId || !classId)
            return Fail(handler, "Race and class: a number or a vanilla name (undead, mage...)");
        if (!sObjectMgr->GetPlayerInfo(raceId, classId))
            return Fail(handler, fmt::format("There is no {} {}", race, playerClass));

        uint8 sex = gender.value_or(GENDER_MALE);
        if (sex != GENDER_MALE && sex != GENDER_FEMALE)
            return Fail(handler, "Gender: 0 (male) or 1 (female)");

        uint8 newLevel = std::clamp<uint8>(level.value_or(DEFAULT_LEVEL), 1, uint8(sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL)));

        uint32 accountId = TestAccountId(true);
        if (!accountId)
            return Fail(handler, fmt::format("The account {} could not be made, see the server log", TEST_ACCOUNT));

        WorldPackets::Character::CharacterCreateInfo createInfo;
        createInfo.Race = raceId;
        createInfo.Class = classId;
        createInfo.Sex = sex;
        createInfo.Name = name;

        // Player::Create takes the account from the session; this one only lives while the character is made
        SpellTestSession session(accountId, TEST_ACCOUNT);
        std::unique_ptr<Player, void(*)(Player*)> newChar(new Player(&session), [](Player* player)
        {
            player->CleanupsBeforeDelete();
            delete player;
        });
        newChar->GetMotionMaster()->Initialize();

        ObjectGuid::LowType guidLow = sObjectMgr->GetGenerator<HighGuid::Player>().Generate();
        if (!newChar->Create(guidLow, &createInfo))
            return Fail(handler, "Player::Create refused it, see the server log");

        // Away from players, at the level of the tests: the login works out the rest (stats, talent points, skill maximums)
        newChar->WorldRelocate(TEST_LOCATION);
        newChar->SetLevel(newLevel);
        newChar->SetAtLoginFlag(AT_LOGIN_FIRST);
        newChar->SaveToDB(true);

        sScriptMgr->OnPlayerCreate(newChar.get());
        sCharacterCache->AddCharacterCacheEntry(newChar->GetGUID(), accountId, name, sex, raceId, classId, newLevel, false);

        handler->SendSysMessage(fmt::format("Created {} ({}), level {} race {} class {}: .spelltest login {}", name, newChar->GetGUID().ToString(),
            uint32(newLevel), uint32(raceId), uint32(classId), name));
        return true;
    }

    static bool HandleLogin(ChatHandler* handler, std::string name)
    {
        if (!normalizePlayerName(name))
            return Fail(handler, "Bad character name");

        ObjectGuid guid = sCharacterCache->GetCharacterGuidByName(name);
        if (guid.IsEmpty())
            return Fail(handler, fmt::format("No character named {}", name));
        if (ObjectAccessor::FindConnectedPlayer(guid))
            return Fail(handler, fmt::format("{} is already in the world", name));

        // Only characters made for the tests: logging in anyone else's would take their character away from them
        uint32 accountId = sCharacterCache->GetCharacterAccountIdByGuid(guid);
        if (!accountId || accountId != TestAccountId(false))
            return Fail(handler, fmt::format("{} is not a test character (they are on the account {}: .spelltest create)", name, TEST_ACCOUNT));

        std::lock_guard lock(Recorder.Lock);
        if (Recorder.Characters.contains(guid))
            return Fail(handler, fmt::format("{} is already logging in", name));

        auto session = std::make_unique<SpellTestSession>(accountId, TEST_ACCOUNT);
        session->InitializeSession();
        if (!session->LoginWithoutClient(guid))
            return Fail(handler, "The login query could not start, see the server log");

        TestCharacter& character = Recorder.Characters[guid];
        character.Name = name;
        character.Session = std::move(session);
        character.LogStart = GameTime::GetGameTimeMS();
        Recorder.Units[guid] = guid;
        Recorder.Active = true;
        handler->SendSysMessage(fmt::format("{} is logging in", name));
        return true;
    }

    static bool HandleLogout(ChatHandler* handler, std::string name)
    {
        if (!normalizePlayerName(name))
            return Fail(handler, "Bad character name");

        std::lock_guard lock(Recorder.Lock);
        ObjectGuid guid = sCharacterCache->GetCharacterGuidByName(name);
        auto itr = Recorder.Characters.find(guid);
        if (itr == Recorder.Characters.end())
            return Fail(handler, fmt::format("{} is not logged in as a test character", name));

        LogOut(itr->second);
        Recorder.Units.erase(itr->second.Dummy);
        Recorder.Units.erase(guid);
        Recorder.Characters.erase(itr);
        Recorder.Active = !Recorder.Characters.empty();
        handler->SendSysMessage(fmt::format("{} logged out", name));
        return true;
    }

    static bool HandleStatus(ChatHandler* handler)
    {
        std::lock_guard lock(Recorder.Lock);
        if (Recorder.Characters.empty())
            handler->SendSysMessage("No test characters are logged in");

        for (auto const& [guid, character] : Recorder.Characters)
        {
            Player const* player = character.Session->GetPlayer();
            if (!character.Ready || !player)
            {
                handler->SendSysMessage(fmt::format("{}: logging in", character.Name));
                continue;
            }
            handler->SendSysMessage(fmt::format("{}: level {} race {} class {}, map {} zone {} ({:.1f}, {:.1f}, {:.1f}), {}{} log lines, dummy {}",
                character.Name, uint32(player->GetLevel()), uint32(player->GetRace()), uint32(player->GetClass()), player->GetMapId(),
                player->GetZoneId(), player->GetPositionX(), player->GetPositionY(), player->GetPositionZ(), player->IsFFAPvP() ? "free-for-all PvP, " : "",
                character.Log.size(), ObjectAccessor::GetCreature(*player, character.Dummy) ? "yes" : "no"));
        }
        return true;
    }

    static bool HandleInfo(ChatHandler* handler, uint32 spellId)
    {
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
        if (!spellInfo)
            return Fail(handler, fmt::format("No spell {}", spellId));

        handler->SendSysMessage(fmt::format("{}: school 0x{:X}, cast {} ms, duration {}/{} ms, cooldown {} / category {} / global {} ms, range {:.0f}-{:.0f}, "
            "family {} 0x{:X} 0x{:X} 0x{:X} 0x{:X}, levels {}/{}/{}", SpellText(spellId), spellInfo->SchoolMask,
            spellInfo->CastTimeEntry ? spellInfo->CastTimeEntry->Base : 0, spellInfo->GetDuration(), spellInfo->GetMaxDuration(), spellInfo->RecoveryTime,
            spellInfo->CategoryRecoveryTime, spellInfo->StartRecoveryTime, spellInfo->GetMinRange(false), spellInfo->GetMaxRange(false),
            spellInfo->SpellFamilyName, spellInfo->SpellFamilyFlags[0], spellInfo->SpellFamilyFlags[1], spellInfo->SpellFamilyFlags[2],
            spellInfo->SpellFamilyFlags[3], spellInfo->BaseLevel, spellInfo->SpellLevel, spellInfo->MaxLevel));

        handler->SendSysMessage(fmt::format("attributes 0x{:X} 0x{:X} 0x{:X} 0x{:X} 0x{:X} 0x{:X} 0x{:X} 0x{:X} 0x{:X} 0x{:X} 0x{:X} 0x{:X} 0x{:X} 0x{:X} "
            "0x{:X} 0x{:X} 0x{:X}, custom 0x{:X}", spellInfo->Attributes, spellInfo->AttributesEx, spellInfo->AttributesEx2, spellInfo->AttributesEx3,
            spellInfo->AttributesEx4, spellInfo->AttributesEx5, spellInfo->AttributesEx6, spellInfo->AttributesEx7, spellInfo->AttributesEx8,
            spellInfo->AttributesEx9, spellInfo->AttributesEx10, spellInfo->AttributesEx11, spellInfo->AttributesEx12, spellInfo->AttributesEx13,
            spellInfo->AttributesEx14, spellInfo->AttributesEx15, spellInfo->AttributesEx16, spellInfo->AttributesCu));

        for (SpellPowerEntry const* power : spellInfo->PowerCosts)
            if (power)
                handler->SendSysMessage(fmt::format("cost: power {} {} + {:.1f}% (per level {}, per second {})", int32(power->PowerType), power->ManaCost,
                    power->PowerCostPct, power->ManaCostPerLevel, power->ManaPerSecond));

        handler->SendSysMessage(fmt::format("spell proc data: flags 0x{:X}:0x{:X}, chance {}, charges {}, cooldown {} ms, ppm {:.2f}",
            uint32(spellInfo->ProcFlags[0]), uint32(spellInfo->ProcFlags[1]), spellInfo->ProcChance, spellInfo->ProcCharges, spellInfo->ProcCooldown,
            spellInfo->ProcBasePPM));
        if (SpellProcEntry const* proc = sSpellMgr->GetSpellProcEntry(spellInfo))
            handler->SendSysMessage(fmt::format("proc entry (used): flags 0x{:X}:0x{:X}, school 0x{:X}, family {} 0x{:X} 0x{:X} 0x{:X} 0x{:X}, spell type 0x{:X}, "
                "phase 0x{:X}, hit 0x{:X}, attributes 0x{:X}, disabled effects 0x{:X}, ppm {:.2f}, chance {:.1f}, cooldown {} ms, charges {}",
                uint32(proc->ProcFlags[0]), uint32(proc->ProcFlags[1]), proc->SchoolMask, proc->SpellFamilyName, proc->SpellFamilyMask[0],
                proc->SpellFamilyMask[1], proc->SpellFamilyMask[2], proc->SpellFamilyMask[3], uint32(proc->SpellTypeMask), uint32(proc->SpellPhaseMask),
                uint32(proc->HitMask), uint32(proc->AttributesMask), proc->DisableEffectsMask, proc->ProcsPerMinute, proc->Chance,
                proc->Cooldown.count(), proc->Charges));
        else
            handler->SendSysMessage("no proc entry");

        std::string scripts;
        SpellScriptsBounds bounds = sObjectMgr->GetSpellScriptsBounds(spellId);
        for (auto itr = bounds.first; itr != bounds.second; ++itr)
            scripts += fmt::format("{}{}{}", scripts.empty() ? "" : ", ", sObjectMgr->GetScriptName(itr->second.first), itr->second.second ? "" : " (disabled)");
        handler->SendSysMessage(fmt::format("scripts: {}", scripts.empty() ? "none" : scripts));

        for (SpellEffectInfo const& effect : spellInfo->GetEffects())
        {
            if (!effect.Effect)
                continue;

            handler->SendSysMessage(fmt::format("e{}: effect {} aura {}, base {} (per level {}, variance {}), bonus {} ap {}, amplitude {}, period {} ms, "
                "misc {} {}, targets {} {}, radius entry {}, trigger {}, class mask 0x{:X} 0x{:X} 0x{:X} 0x{:X}, mechanic {}, chain {}",
                uint32(effect.EffectIndex), uint32(effect.Effect), uint32(effect.ApplyAuraName), effect.BasePoints, effect.RealPointsPerLevel,
                effect.Scaling.Variance, effect.BonusCoefficient, effect.BonusCoefficientFromAP, effect.Amplitude, effect.ApplyAuraPeriod,
                effect.MiscValue, effect.MiscValueB, uint32(effect.TargetA.GetTarget()), uint32(effect.TargetB.GetTarget()),
                effect.TargetARadiusEntry ? effect.TargetARadiusEntry->ID : 0, effect.TriggerSpell, effect.SpellClassMask[0], effect.SpellClassMask[1],
                effect.SpellClassMask[2], effect.SpellClassMask[3], uint32(effect.Mechanic), effect.ChainTargets));
        }
        return true;
    }

    static bool HandleDummy(ChatHandler* handler, std::string name, Optional<float> yards, Optional<uint32> entry)
    {
        TestCharacter* character = nullptr;
        Player* player = TestPlayer(handler, name, &character);
        if (!player)
            return false;

        std::lock_guard lock(Recorder.Lock);
        if (Creature* old = ObjectAccessor::GetCreature(*player, character->Dummy))
        {
            Recorder.Units.erase(old->GetGUID());
            old->DespawnOrUnsummon();
        }

        Position position = player->GetPosition();
        player->MovePosition(position, yards.value_or(5.0f), 0.0f);
        TempSummon* dummy = player->SummonCreature(entry.value_or(NPC_TEST_DUMMY), position, TEMPSUMMON_TIMED_DESPAWN, 2h);
        if (!dummy)
            return Fail(handler, "The dummy could not be summoned");

        // A creature AI needs a script id: the dummy has no script, and its AI is a NullCreatureAI
        dummy->AIM_Initialize(new SpellTestDummyAI(dummy, sObjectMgr->GetScriptId("NullCreatureAI", false)));
        dummy->SetReactState(REACT_PASSIVE);
        dummy->SetRegenerateHealth(false);
        dummy->SetFacingToObject(player);
        dummy->SetControlled(true, UNIT_STATE_ROOT);
        dummy->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
        Face(player, dummy);
        character->Dummy = dummy->GetGUID();
        Recorder.Units[dummy->GetGUID()] = player->GetGUID();
        handler->SendSysMessage(fmt::format("{}: {} (level {}, health {}, armor {}) at {:.1f} yards", player->GetName(), dummy->GetName(),
            uint32(dummy->GetLevel()), dummy->GetMaxHealth(), dummy->GetArmor(), player->GetExactDist(dummy)));
        return true;
    }

    static bool HandlePlace(ChatHandler* handler, std::string name, std::string otherName, Optional<float> yards)
    {
        Player* player = TestPlayer(handler, name);
        if (!player)
            return false;
        Player* other = TestPlayer(handler, otherName);
        if (!other)
            return false;
        if (other == player)
            return Fail(handler, "Two different test characters, please");
        if (other->GetMap() != player->GetMap())
            return Fail(handler, fmt::format("{} is on another map", other->GetName()));

        // No client to tell: the server moves it
        Position position = player->GetPosition();
        player->MovePosition(position, yards.value_or(5.0f), 0.0f);
        position.SetOrientation(position.GetAbsoluteAngle(player));
        other->UpdatePosition(position, true);
        Face(player, other);
        Face(other, player);
        handler->SendSysMessage(fmt::format("{} stands {:.1f} yards in front of {}", other->GetName(), player->GetExactDist(other), player->GetName()));
        return true;
    }

    static bool HandlePvp(ChatHandler* handler, std::string name, std::string state)
    {
        Player* player = TestPlayer(handler, name);
        if (!player)
            return false;

        if (StringEqualI(state, "on"))
            player->SetPvpFlag(UNIT_BYTE2_FLAG_FFA_PVP);
        else if (StringEqualI(state, "off"))
            player->RemovePvpFlag(UNIT_BYTE2_FLAG_FFA_PVP);
        else
            return Fail(handler, "on or off");

        handler->SendSysMessage(fmt::format("{}: free-for-all PvP {}", player->GetName(), player->IsFFAPvP() ? "on" : "off"));
        return true;
    }

    static bool Cast(ChatHandler* handler, std::string const& name, uint32 spellId, Optional<std::string> const& which, TriggerCastFlags flags)
    {
        TestCharacter* character = nullptr;
        Player* player = TestPlayer(handler, name, &character);
        if (!player)
            return false;
        if (!sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE))
            return Fail(handler, fmt::format("No spell {}", spellId));

        bool none = false;
        Unit* target = nullptr;
        if (which)
        {
            target = Target(handler, player, character, *which, none);
            if (!target && !none)
                return false;
        }
        else if (!(target = ObjectAccessor::GetCreature(*player, character->Dummy)))
            target = player;

        SpellCastResult result;
        if (target)
        {
            Face(player, target);
            result = player->CastSpell(target, spellId, CastSpellExtraArgs(flags));
        }
        else
            result = player->CastSpell(nullptr, spellId, CastSpellExtraArgs(flags));

        handler->SendSysMessage(fmt::format("{} casts {} on {}: {}", player->GetName(), SpellText(spellId), target ? target->GetName() : "-",
            CastResultName(result)));
        if (result != SPELL_CAST_OK)
        {
            handler->SetSentErrorMessage(true);    // a failed cast, not a wrong command: no usage text
            return false;
        }
        return true;
    }

    static bool HandleCast(ChatHandler* handler, std::string name, uint32 spellId, Optional<std::string> target)
    {
        return Cast(handler, name, spellId, target, TRIGGERED_NONE);
    }

    static bool HandleTrigger(ChatHandler* handler, std::string name, uint32 spellId, Optional<std::string> target)
    {
        return Cast(handler, name, spellId, target, TRIGGERED_FULL_MASK);
    }

    static bool HandleAttack(ChatHandler* handler, std::string name, Optional<std::string> which)
    {
        TestCharacter* character = nullptr;
        Player* player = TestPlayer(handler, name, &character);
        if (!player)
            return false;

        bool none = false;
        Unit* target = Target(handler, player, character, which.value_or("dummy"), none);
        if (!target)
            return none ? Fail(handler, "attack needs a target") : false;
        if (target == player)
            return Fail(handler, fmt::format("{} can't attack itself", player->GetName()));

        Face(player, target);
        if (!player->Attack(target, true))
            return Fail(handler, fmt::format("{} can't attack {}", player->GetName(), target->GetName()));

        handler->SendSysMessage(fmt::format("{} attacks {}", player->GetName(), target->GetName()));
        return true;
    }

    static bool HandleStop(ChatHandler* handler, std::string name)
    {
        Player* player = TestPlayer(handler, name);
        if (!player)
            return false;

        player->AttackStop();
        player->InterruptNonMeleeSpells(true);
        handler->SendSysMessage(fmt::format("{} stops", player->GetName()));
        return true;
    }

    static bool HandleLearn(ChatHandler* handler, std::string name, uint32 spellId)
    {
        Player* player = TestPlayer(handler, name);
        if (!player)
            return false;
        if (!sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE))
            return Fail(handler, fmt::format("No spell {}", spellId));

        player->LearnSpell(spellId, false);
        handler->SendSysMessage(fmt::format("{} {} {}", player->GetName(), player->HasSpell(spellId) ? "knows" : "could not learn", SpellText(spellId)));
        return true;
    }

    static bool HandleUnlearn(ChatHandler* handler, std::string name, uint32 spellId)
    {
        Player* player = TestPlayer(handler, name);
        if (!player)
            return false;

        player->RemoveSpell(spellId, false, false);
        handler->SendSysMessage(fmt::format("{} {} {}", player->GetName(), player->HasSpell(spellId) ? "still knows" : "does not know", SpellText(spellId)));
        return true;
    }

    static bool HandleEquip(ChatHandler* handler, std::string name, uint32 itemId)
    {
        Player* player = TestPlayer(handler, name);
        if (!player)
            return false;
        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
        if (!proto)
            return Fail(handler, fmt::format("No item {}", itemId));

        // Equips it if the character can, else puts it in a bag
        if (!player->StoreNewItemInBestSlots(itemId, 1, ItemContext::NONE))
            return Fail(handler, fmt::format("{} can't take {} {}", player->GetName(), itemId, proto->GetDefaultLocaleName()));

        Item const* item = player->GetItemByEntry(itemId);
        handler->SendSysMessage(fmt::format("{} {} {} {}", player->GetName(), item && item->IsEquipped() ? "equips" : "carries", itemId,
            proto->GetDefaultLocaleName()));
        return true;
    }

    static bool HandleReset(ChatHandler* handler, std::string name)
    {
        Player* player = TestPlayer(handler, name);
        if (!player)
            return false;

        if (!player->IsAlive())
        {
            player->ResurrectPlayer(1.0f);
            player->SpawnCorpseBones();
        }
        player->AttackStop();
        player->InterruptNonMeleeSpells(true);
        player->CombatStop(true);
        player->RemoveAllAurasOnDeath();
        player->GetSpellHistory()->ResetAllCooldowns();
        player->GetSpellHistory()->ResetAllCharges();
        FillHealthAndPower(player);

        handler->SendSysMessage(fmt::format("{}: alive, cooldowns, health, power and auras reset", player->GetName()));
        return true;
    }

    static bool HandleHealth(ChatHandler* handler, std::string name, float percent, Optional<std::string> which)
    {
        TestCharacter* character = nullptr;
        Player* player = TestPlayer(handler, name, &character);
        if (!player)
            return false;

        bool none = false;
        Unit* unit = which ? Target(handler, player, character, *which, none) : player;
        if (!unit)
            return none ? Fail(handler, "health needs a target") : false;

        unit->SetHealth(std::max<uint64>(1, unit->CountPctFromMaxHealth(percent)));
        handler->SendSysMessage(fmt::format("{}: health {}/{}", unit->GetName(), unit->GetHealth(), unit->GetMaxHealth()));
        return true;
    }

    static bool HandleAuras(ChatHandler* handler, std::string name, Optional<std::string> which)
    {
        TestCharacter* character = nullptr;
        Player* player = TestPlayer(handler, name, &character);
        if (!player)
            return false;

        bool none = false;
        Unit* unit = which ? Target(handler, player, character, *which, none) : player;
        if (!unit)
            return none ? Fail(handler, "auras needs a target") : false;

        for (auto const& [spellId, aurApp] : unit->GetAppliedAuras())
            handler->SendSysMessage(fmt::format("{} from {}: {}", SpellText(spellId), NameOf(unit, aurApp->GetBase()->GetCasterGUID()), DescribeAura(aurApp)));
        return true;
    }

    static bool HandleStats(ChatHandler* handler, std::string name)
    {
        Player* player = TestPlayer(handler, name);
        if (!player)
            return false;

        handler->SendSysMessage(fmt::format("{}: level {} race {} class {}, health {}/{}, armor {}", player->GetName(), uint32(player->GetLevel()),
            uint32(player->GetRace()), uint32(player->GetClass()), player->GetHealth(), player->GetMaxHealth(), player->GetArmor()));
        for (int32 power = 0; power < MAX_POWERS; ++power)
            if (player->GetMaxPower(Powers(power)) > 0)
                handler->SendSysMessage(fmt::format("power {}: {}/{}", power, player->GetPower(Powers(power)), player->GetMaxPower(Powers(power))));

        handler->SendSysMessage(fmt::format("attack power {:.0f}, ranged {:.0f}; main hand {:.0f}-{:.0f} every {} ms, ranged {:.0f}-{:.0f} every {} ms",
            player->GetTotalAttackPowerValue(BASE_ATTACK), player->GetTotalAttackPowerValue(RANGED_ATTACK),
            player->GetWeaponDamageRange(BASE_ATTACK, MINDAMAGE), player->GetWeaponDamageRange(BASE_ATTACK, MAXDAMAGE), player->GetBaseAttackTime(BASE_ATTACK),
            player->GetWeaponDamageRange(RANGED_ATTACK, MINDAMAGE), player->GetWeaponDamageRange(RANGED_ATTACK, MAXDAMAGE), player->GetBaseAttackTime(RANGED_ATTACK)));

        std::string spellPower = "spell power";
        for (uint32 school = SPELL_SCHOOL_HOLY; school < MAX_SPELL_SCHOOL; ++school)
            spellPower += fmt::format(" {}:{}", school, player->SpellBaseDamageBonusDone(SpellSchoolMask(1 << school)));
        handler->SendSysMessage(fmt::format("{}, healing {}", spellPower, player->SpellBaseHealingBonusDone(SPELL_SCHOOL_MASK_ALL)));

        handler->SendSysMessage(fmt::format("crit: melee {:.2f}%, ranged {:.2f}%, spell {:.2f}%", float(player->m_activePlayerData->CritPercentage),
            float(player->m_activePlayerData->RangedCritPercentage), float(player->m_activePlayerData->SpellCritPercentage)));
        return true;
    }

    static bool HandleLog(ChatHandler* handler, std::string name)
    {
        if (!normalizePlayerName(name))
            return Fail(handler, "Bad character name");

        std::lock_guard lock(Recorder.Lock);
        auto itr = Recorder.Characters.find(sCharacterCache->GetCharacterGuidByName(name));
        if (itr == Recorder.Characters.end())
            return Fail(handler, fmt::format("{} is not logged in as a test character", name));

        TestCharacter& character = itr->second;
        for (std::string const& line : character.Log)
            handler->SendSysMessage(line);
        if (character.Dropped)
            handler->SendSysMessage(fmt::format("({} more lines were dropped)", character.Dropped));

        character.Log.clear();
        character.Dropped = 0;
        character.LogStart = GameTime::GetGameTimeMS();
        return true;
    }
};

// Runs the test characters' sessions, which the world doesn't know about
class spell_test_worldscript : public WorldScript
{
public:
    spell_test_worldscript() : WorldScript("spell_test_worldscript") { }

    void OnUpdate(uint32 /*diff*/) override
    {
        std::lock_guard lock(Recorder.Lock);
        for (auto itr = Recorder.Characters.begin(); itr != Recorder.Characters.end();)
        {
            TestCharacter& character = itr->second;
            character.Session->UpdateWithoutClient();
            if (!character.Ready)
            {
                Player* player = character.Session->GetPlayer();
                if (player && player->IsInWorld())
                {
                    // What the client's packets would set: without this flag the character sees no creature, and every
                    // spell on one fails with bad targets
                    player->SetPlayerLocalFlag(PLAYER_LOCAL_FLAG_OVERRIDE_TRANSPORT_SERVER_TIME);
                    uint32 zoneId, areaId;
                    player->GetZoneAndAreaId(zoneId, areaId);
                    player->UpdateZone(zoneId, areaId);
                    PhasingHandler::OnMapChange(player);
                    MaxWeaponSkills(player);
                    // Every test starts from the same state, whatever the character had when it logged out
                    FillHealthAndPower(player);
                    character.Ready = true;
                }
                else if (!player && !character.Session->PlayerLoading())
                {
                    TC_LOG_ERROR("scripts", "spelltest: {} could not log in (Player::LoadFromDB failed, see above)", character.Name);
                    Recorder.Units.erase(itr->first);
                    itr = Recorder.Characters.erase(itr);
                    continue;
                }
            }
            ++itr;
        }
        Recorder.Active = !Recorder.Characters.empty();
    }

    void OnShutdown() override
    {
        std::lock_guard lock(Recorder.Lock);
        for (auto& [guid, character] : Recorder.Characters)
            LogOut(character);

        Recorder.Characters.clear();
        Recorder.Units.clear();
        Recorder.Active = false;
    }
};

void AddSC_spell_test()
{
    SpellTestObserver::Instance = &Recorder;
    new spell_test_commandscript();
    new spell_test_worldscript();
}
