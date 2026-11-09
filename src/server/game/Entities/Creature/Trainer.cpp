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

#include "Trainer.h"
#include "BattlePetMgr.h"
#include "Chat.h"
#include "ConditionMgr.h"
#include "Creature.h"
#include "DB2Stores.h"
#include "Log.h"
#include "NPCPackets.h"
#include "Pet.h"
#include "Player.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "StringFormat.h"
#include "WorldSession.h"

namespace Trainer
{
    // Classic 1.60 pet trainers teach the hunter's pet (Pet::GetTrainerTaughtPetSpell): such a spell is known, and meets a
    // ReqAbility, when the current pet knows its pet spell or a higher rank of it.
    static Pet* GetTrainablePet(Player const* player)
    {
        Pet* pet = player->GetPet();
        return pet && pet->getPetType() == HUNTER_PET && pet->IsAlive() ? pet : nullptr;
    }

    static bool HasTrainerAbility(Player const* player, uint32 spellId)
    {
        if (uint32 petSpell = Pet::GetTrainerTaughtPetSpell(spellId))
        {
            Pet const* pet = GetTrainablePet(player);
            return pet && pet->KnowsSpellOrHigherRank(petSpell);
        }
        return player->HasSpell(spellId);
    }

    // A pet trainer refusal used to be silent: the client offers every rank whose level the hunter has (it cannot see the pet's
    // level or spells), and TRAINER_BUY_FAILED shows nothing, so "Learn" did nothing. Say why, and log it for bug bundles.
    static void ExplainPetTrainerRefusal(Player* player, ::Trainer::Spell const* trainerSpell, uint32 petSpell)
    {
        bool const ru = player->GetSession()->GetSessionDbLocaleIndex() == LOCALE_ruRU;
        Pet const* pet = GetTrainablePet(player);
        std::string reason;
        std::string text;
        if (!pet)
        {
            reason = "no live hunter pet";
            text = ru ? "Сначала призовите (или воскресите) своего питомца." : "Summon (or revive) your pet first.";
        }
        else if (pet->KnowsSpellOrHigherRank(petSpell))
        {
            reason = "pet knows it or a higher rank";
            text = ru ? "Ваш питомец уже знает эту способность." : "Your pet already knows this ability.";
        }
        else if (std::any_of(trainerSpell->ReqAbility.begin(), trainerSpell->ReqAbility.end(),
                     [player](int32 req) { return req && !HasTrainerAbility(player, req); }))
        {
            reason = "pet lacks the lower rank";
            text = ru ? "Сначала питомец должен изучить предыдущий ранг." : "Your pet must learn the previous rank first.";
        }
        else if (player->GetLevel() < trainerSpell->ReqLevel || pet->GetLevel() < trainerSpell->ReqLevel)
        {
            reason = Trinity::StringFormat("needs level {} (hunter {}, pet {})", trainerSpell->ReqLevel, player->GetLevel(), pet->GetLevel());
            text = ru ? Trinity::StringFormat("Ваш питомец должен быть {}-го уровня.", trainerSpell->ReqLevel)
                      : Trinity::StringFormat("Your pet must be level {}.", trainerSpell->ReqLevel);
        }
        else
            reason = "other";

        TC_LOG_INFO("server.trainer", "Pet trainer refused spell {} (pet spell {}) for {} ({}): {}", trainerSpell->SpellId, petSpell,
            player->GetName(), player->GetGUID().ToString(), reason);
        if (!text.empty())
            ChatHandler(player->GetSession()).SendSysMessage(text);
    }

    bool Spell::IsCastable() const
    {
        return sSpellMgr->AssertSpellInfo(SpellId, DIFFICULTY_NONE)->HasEffect(SPELL_EFFECT_LEARN_SPELL);
    }

    Trainer::Trainer(uint32 id, Type type, std::string_view greeting, std::vector<Spell> spells) : _id(id), _type(type), _spells(std::move(spells))
    {
        _greeting[DEFAULT_LOCALE] = greeting;
    }

    void Trainer::SendSpells(Creature const* npc, Player* player, LocaleConstant locale) const
    {
        float reputationDiscount = player->GetReputationPriceDiscount(npc);

        WorldPackets::NPC::TrainerList trainerList;
        trainerList.TrainerGUID = npc->GetGUID();
        trainerList.TrainerType = AsUnderlyingType(_type);
        trainerList.TrainerID = _id;
        trainerList.Greeting = GetGreeting(locale);
        trainerList.Spells.reserve(_spells.size());
        for (Spell const& trainerSpell : _spells)
        {
            if (!player->IsSpellFitByClassAndRace(trainerSpell.SpellId))
                continue;

            if (!sConditionMgr->IsObjectMeetingTrainerSpellConditions(_id, trainerSpell.SpellId, player))
            {
                TC_LOG_DEBUG("condition", "SendSpells: conditions not met for trainer id {} spell {} player '{}' ({})", _id, trainerSpell.SpellId, player->GetName(), player->GetGUID().ToString());
                continue;
            }

            // the client marks a rank as learned only when the hunter knows it, so a rank the pet already has would stay "available"
            if (Pet::GetTrainerTaughtPetSpell(trainerSpell.SpellId) && GetSpellState(player, &trainerSpell) == SpellState::Known)
                continue;

            trainerList.Spells.emplace_back();
            WorldPackets::NPC::TrainerListSpell& trainerListSpell = trainerList.Spells.back();
            trainerListSpell.SpellID = trainerSpell.SpellId;
            trainerListSpell.MoneyCost = int32(trainerSpell.MoneyCost * reputationDiscount);
            trainerListSpell.ReqSkillLine = trainerSpell.ReqSkillLine;
            trainerListSpell.ReqSkillRank = trainerSpell.ReqSkillRank;
            std::copy(trainerSpell.ReqAbility.begin(), trainerSpell.ReqAbility.end(), trainerListSpell.ReqAbility.begin());
            for (int32& reqAbility : trainerListSpell.ReqAbility)
                if (reqAbility && Pet::GetTrainerTaughtPetSpell(reqAbility) && HasTrainerAbility(player, reqAbility))
                    reqAbility = 0;
            trainerListSpell.Usable = AsUnderlyingType(GetSpellState(player, &trainerSpell));
            trainerListSpell.ReqLevel = trainerSpell.ReqLevel;
        }

        player->SendDirectMessage(trainerList.Write());
    }

    void Trainer::TeachSpell(Creature const* npc, Player* player, uint32 spellId) const
    {
        Spell const* trainerSpell = GetSpell(spellId);
        if (!trainerSpell || !CanTeachSpell(player, trainerSpell))
        {
            if (trainerSpell)
                if (uint32 petSpell = Pet::GetTrainerTaughtPetSpell(trainerSpell->SpellId))
                    ExplainPetTrainerRefusal(player, trainerSpell, petSpell);
            SendTeachFailure(npc, player, spellId, FailReason::Unavailable);
            return;
        }

        bool sendSpellVisual = true;
        BattlePetSpeciesEntry const* speciesEntry = BattlePets::BattlePetMgr::GetBattlePetSpeciesBySpell(trainerSpell->SpellId);
        if (speciesEntry)
        {
            if (player->GetSession()->GetBattlePetMgr()->HasMaxPetCount(speciesEntry, player->GetGUID()))
            {
                // Don't send any error to client (intended)
                return;
            }

            sendSpellVisual = false;
        }

        float reputationDiscount = player->GetReputationPriceDiscount(npc);
        int64 moneyCost = int64(trainerSpell->MoneyCost * reputationDiscount);
        if (!player->HasEnoughMoney(moneyCost))
        {
            SendTeachFailure(npc, player, spellId, FailReason::NotEnoughMoney);
            return;
        }

        player->ModifyMoney(-moneyCost);

        if (sendSpellVisual)
        {
            npc->SendPlaySpellVisualKit(179, 0, 0);     // 53 SpellCastDirected
            player->SendPlaySpellVisualKit(362, 1, 0);  // 113 EmoteSalute
        }

        // learn explicitly or cast explicitly
        if (trainerSpell->IsCastable())
        {
            player->CastSpell(player, trainerSpell->SpellId, true);
            if (Pet::GetTrainerTaughtPetSpell(trainerSpell->SpellId))
                SendSpells(npc, player, player->GetSession()->GetSessionDbLocaleIndex());
        }
        else
        {
            bool dependent = false;

            if (speciesEntry)
            {
                player->GetSession()->GetBattlePetMgr()->AddPet(speciesEntry->ID, BattlePets::BattlePetMgr::SelectPetDisplay(speciesEntry),
                    BattlePets::BattlePetMgr::RollPetBreed(speciesEntry->ID), BattlePets::BattlePetMgr::GetDefaultPetQuality(speciesEntry->ID));
                // If the spell summons a battle pet, we fake that it has been learned and the battle pet is added
                // marking as dependent prevents saving the spell to database (intended)
                dependent = true;
            }

            player->LearnSpell(trainerSpell->SpellId, dependent);
        }
    }

    uint32 Trainer::GetClassMask() const
    {
        if (_classMask)
            return _classMask;

        // The classes this trainer can teach: every class that owns at least one of its spells. (An intersection over the spells
        // let a single spell with a stray or foreign class skill line drop the trainer's own class, so warrior, priest, mage and
        // rogue trainers taught nobody. Each spell is still checked against the player's class by IsSpellFitByClassAndRace.)
        constexpr uint32 allClasses = (1u << MAX_CLASSES) - 1;
        uint32 mask = 0;
        for (Spell const& trainerSpell : _spells)
        {
            SkillLineAbilityMapBounds bounds = sSpellMgr->GetSkillLineAbilityMapBounds(trainerSpell.SpellId);
            uint32 spellMask = 0;
            for (auto itr = bounds.first; itr != bounds.second; ++itr)
            {
                SkillLineEntry const* skill = sSkillLineStore.LookupEntry(itr->second->SkillLine);
                if (!skill || skill->CategoryID != SKILL_CATEGORY_CLASS)
                    continue;
                for (uint8 cls = CLASS_WARRIOR; cls < MAX_CLASSES; ++cls)
                {
                    if (itr->second->ClassMask && !(itr->second->ClassMask & (1u << (cls - 1))))
                        continue;
                    for (uint8 race = 1; race < 40; ++race)
                        if (sDB2Manager.GetSkillRaceClassInfo(itr->second->SkillLine, race, cls))
                        {
                            spellMask |= 1u << (cls - 1);
                            break;
                        }
                }
            }
            mask |= spellMask;   // a spell without a class skill line says nothing
        }
        _classMask = mask ? mask : allClasses;
        return _classMask;
    }

    uint32 Trainer::TeachAvailableSpellsFree(Player* player, FreeTeachTally* tally) const
    {
        uint32 learned = 0;
        if (!(GetClassMask() & player->GetClassMask()))
            return 0;
        for (Spell const& trainerSpell : _spells)
        {
            if (tally)
                ++tally->spells;
            if (trainerSpell.ReqSkillLine)
            {
                // A class skill line (a rogue's Poisons) is the class's own, not a profession: the bot holds it at the value its level gives
                // (five per level, as a rogue's Poisons skill rises), and the poison ranks behind it open as at the trainer.
                SkillLineEntry const* line = sSkillLineStore.LookupEntry(trainerSpell.ReqSkillLine);
                if (!line || line->CategoryID != SKILL_CATEGORY_CLASS || !player->IsSpellFitByClassAndRace(trainerSpell.SpellId))
                {
                    if (tally) ++tally->skill_line;
                    continue;
                }
                uint16 const wanted = uint16(std::min<uint32>(player->GetLevel() * 5, 300));
                if (player->GetBaseSkillValue(trainerSpell.ReqSkillLine) < wanted)
                    player->SetSkill(trainerSpell.ReqSkillLine, 0, wanted, 300);
            }
            if (BattlePets::BattlePetMgr::GetBattlePetSpeciesBySpell(trainerSpell.SpellId))
            {
                if (tally) ++tally->battle_pet;
                continue;
            }
            // Riding is not the trainer's to give here: a retail trainer opens the tiers by level (Journeyman at 40), vanilla gives 75 at 40
            // and 150 at 60, and API::learn_riding_free sets exactly that.
            if (SpellInfo const* info = sSpellMgr->GetSpellInfo(trainerSpell.SpellId, DIFFICULTY_NONE))
                if (std::any_of(info->GetEffects().begin(), info->GetEffects().end(), [](SpellEffectInfo const& eff)
                    { return eff.IsEffect(SPELL_EFFECT_SKILL) && eff.MiscValue == SKILL_RIDING; }))
                {
                    if (tally) ++tally->skill_line;
                    continue;
                }
            if (!CanTeachSpell(player, &trainerSpell))
            {
                if (tally)
                {
                    // The same order as GetSpellState.
                    if (player->HasSpell(trainerSpell.SpellId))                              ++tally->known;
                    else if (!player->IsSpellFitByClassAndRace(trainerSpell.SpellId))        ++tally->class_race;
                    else if (std::any_of(trainerSpell.ReqAbility.begin(), trainerSpell.ReqAbility.end(),
                                [player](int32 req) { return req && !HasTrainerAbility(player, req); })) ++tally->ability;
                    else if (player->GetLevel() < trainerSpell.ReqLevel)                      ++tally->level;
                    else if (GetSpellState(player, &trainerSpell) == SpellState::Known)       ++tally->known;
                    else                                                                      ++tally->other;
                }
                continue;
            }
            if (tally)
                ++tally->taught;

            if (trainerSpell.IsCastable())
                player->CastSpell(player, trainerSpell.SpellId, true);
            else
                player->LearnSpell(trainerSpell.SpellId, false);
            ++learned;
        }
        return learned;
    }

    std::string Trainer::DescribeFreeTeachRefusal(Player const* player, uint32 spellId) const
    {
        Spell const* trainerSpell = GetSpell(spellId);
        if (!trainerSpell)
            return "not on this trainer";
        if (!(GetClassMask() & player->GetClassMask()))
            return Trinity::StringFormat("trainer {} class mask {:#x} leaves the class out", _id, GetClassMask());
        if (player->HasSpell(spellId))
            return "known";
        if (!player->IsSpellFitByClassAndRace(spellId))
            return "IsSpellFitByClassAndRace refuses the spell";
        if (trainerSpell->ReqSkillLine && player->GetBaseSkillValue(trainerSpell->ReqSkillLine) < trainerSpell->ReqSkillRank)
        {
            SkillLineEntry const* line = sSkillLineStore.LookupEntry(trainerSpell->ReqSkillLine);
            return Trinity::StringFormat("skill line {} rank {} needed, player has {} ({})", trainerSpell->ReqSkillLine, trainerSpell->ReqSkillRank,
                player->GetBaseSkillValue(trainerSpell->ReqSkillLine), line && line->CategoryID == SKILL_CATEGORY_CLASS ? "class line" : "other line");
        }
        for (int32 req : trainerSpell->ReqAbility)
            if (req && !HasTrainerAbility(player, req))
                return Trinity::StringFormat("needs spell {} first", req);
        if (player->GetLevel() < trainerSpell->ReqLevel)
            return Trinity::StringFormat("needs level {}", trainerSpell->ReqLevel);
        if (BattlePets::BattlePetMgr::GetBattlePetSpeciesBySpell(spellId))
            return "battle pet";
        return "would be taught";
    }

    Spell const* Trainer::GetSpell(uint32 spellId) const
    {
        auto itr = std::find_if(_spells.begin(), _spells.end(), [spellId](Spell const& trainerSpell)
        {
            return trainerSpell.SpellId == spellId;
        });

        if (itr != _spells.end())
            return &(*itr);

        return nullptr;
    }

    bool Trainer::CanTeachSpell(Player const* player, Spell const* trainerSpell) const
    {
        SpellState state = GetSpellState(player, trainerSpell);
        if (state != SpellState::Available)
            return false;

        SpellInfo const* trainerSpellInfo = sSpellMgr->AssertSpellInfo(trainerSpell->SpellId, DIFFICULTY_NONE);
        if (trainerSpellInfo->IsPrimaryProfessionFirstRank() && !player->GetFreePrimaryProfessionPoints())
            return false;

        for (SpellEffectInfo const& effect : trainerSpellInfo->GetEffects())
        {
            if (!effect.IsEffect(SPELL_EFFECT_LEARN_SPELL))
                continue;

            SpellInfo const* learnedSpellInfo = sSpellMgr->GetSpellInfo(effect.TriggerSpell, DIFFICULTY_NONE);
            if (learnedSpellInfo && learnedSpellInfo->IsPrimaryProfessionFirstRank() && !player->GetFreePrimaryProfessionPoints())
                return false;
        }

        return true;
    }

    SpellState Trainer::GetSpellState(Player const* player, Spell const* trainerSpell) const
    {
        if (player->HasSpell(trainerSpell->SpellId))
            return SpellState::Known;

        if (uint32 petSpell = Pet::GetTrainerTaughtPetSpell(trainerSpell->SpellId))
        {
            if (!player->IsSpellFitByClassAndRace(trainerSpell->SpellId))
                return SpellState::Unavailable;

            Pet const* pet = GetTrainablePet(player);
            if (!pet)
                return SpellState::Unavailable;

            if (pet->KnowsSpellOrHigherRank(petSpell))
                return SpellState::Known;

            for (int32 reqAbility : trainerSpell->ReqAbility)
                if (reqAbility && !HasTrainerAbility(player, reqAbility))
                    return SpellState::Unavailable;

            // the pet learns it at the trainer's level, like the hunter would
            if (player->GetLevel() < trainerSpell->ReqLevel || pet->GetLevel() < trainerSpell->ReqLevel)
                return SpellState::Unavailable;

            return SpellState::Available;
        }

        // A class trainer teaches its own class only. A spell of a shared skill line passes the class check for other classes too:
        // Dual Wield (674) is fit for a shaman by the client's data, so a shaman learned it at the warrior trainer (Azeldof, 2026-11-01).
        // A trainer without class spells (weapon masters, professions) has every class in its mask.
        if (!(GetClassMask() & player->GetClassMask()))
            return SpellState::Unavailable;

        // check race/class requirement
        if (!player->IsSpellFitByClassAndRace(trainerSpell->SpellId))
            return SpellState::Unavailable;

        // check skill requirement
        if (trainerSpell->ReqSkillLine && player->GetBaseSkillValue(trainerSpell->ReqSkillLine) < trainerSpell->ReqSkillRank)
            return SpellState::Unavailable;

        for (int32 reqAbility : trainerSpell->ReqAbility)
            if (reqAbility && !player->HasSpell(reqAbility))
                return SpellState::Unavailable;

        // check level requirement
        if (player->GetLevel() < trainerSpell->ReqLevel)
            return SpellState::Unavailable;

        // check ranks
        bool hasLearnSpellEffect = false;
        bool knowsAllLearnedSpells = true;
        for (SpellEffectInfo const& spellEffectInfo : sSpellMgr->AssertSpellInfo(trainerSpell->SpellId, DIFFICULTY_NONE)->GetEffects())
        {
            if (!spellEffectInfo.IsEffect(SPELL_EFFECT_LEARN_SPELL))
                continue;

            hasLearnSpellEffect = true;
            if (!player->HasSpell(spellEffectInfo.TriggerSpell))
                knowsAllLearnedSpells = false;
        }

        if (hasLearnSpellEffect && knowsAllLearnedSpells)
            return SpellState::Known;

        return SpellState::Available;
    }

    void Trainer::SendTeachFailure(Creature const* npc, Player const* player, uint32 spellId, FailReason reason) const
    {
        WorldPackets::NPC::TrainerBuyFailed trainerBuyFailed;
        trainerBuyFailed.TrainerGUID = npc->GetGUID();
        trainerBuyFailed.SpellID = spellId;
        trainerBuyFailed.TrainerFailedReason = AsUnderlyingType(reason);
        player->SendDirectMessage(trainerBuyFailed.Write());
    }

    std::string const& Trainer::GetGreeting(LocaleConstant locale) const
    {
        if (_greeting[locale].empty())
            return _greeting[DEFAULT_LOCALE];

        return _greeting[locale];
    }

    void Trainer::AddGreetingLocale(LocaleConstant locale, std::string_view greeting)
    {
        _greeting[locale] = greeting;
    }
}
