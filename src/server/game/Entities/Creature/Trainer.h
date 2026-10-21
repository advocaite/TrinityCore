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

#ifndef Trainer_h__
#define Trainer_h__

#include "Common.h"
#include <array>
#include <string>
#include <vector>

class Creature;
class ObjectMgr;
class Player;

namespace Trainer
{
    enum class Type : uint32
    {
        None = 0,
        Talent = 1,
        Tradeskill = 2,
        Pet = 3,
    };

    enum class SpellState : uint8
    {
        Known = 0,
        Available = 1,
        Unavailable = 2
    };

    enum class FailReason : uint32
    {
        Unavailable = 0,
        NotEnoughMoney = 1
    };

    struct Spell
    {
        uint32 SpellId = 0;
        uint32 MoneyCost = 0;
        uint32 ReqSkillLine = 0;
        uint32 ReqSkillRank = 0;
        std::array<uint32, 3> ReqAbility = { };
        uint8 ReqLevel = 0;

        bool IsCastable() const;
        bool TeachesPetAbility() const;
    };

    class Trainer
    {
    public:
        Trainer(uint32 id, Type type, std::string_view greeting, std::vector<Spell> spells);

        void SendSpells(Creature const* npc, Player* player, LocaleConstant locale) const;
        void TeachSpell(Creature const* npc, Player* player, uint32 spellId) const;
        // Read-only access to the underlying spell list. Lets callers
        // enumerate trainer offerings without going through the full
        // SendSpells packet build (used by Playerbot to drive bulk-train).
        std::vector<Spell> const& GetSpells() const { return _spells; }
        Type GetType() const { return _type; }
        // Teaches, free of charge and without an NPC, every spell of this trainer the player may learn now (class, race,
        // level and prerequisite checks as at the trainer). Profession and battle pet spells are skipped. For bots that are
        // levelled by command and never walk to a trainer. Returns the number of spells learned.
        // `tally`, when given, counts per trainer spell why it was not taught (diagnostics for a pass that learns nothing).
        struct FreeTeachTally
        {
            uint32 spells = 0;        // trainer spells looked at
            uint32 known = 0;         // the player already has the spell (or everything it teaches)
            uint32 class_race = 0;    // IsSpellFitByClassAndRace
            uint32 skill_line = 0;    // ReqSkillLine (professions): skipped
            uint32 battle_pet = 0;
            uint32 ability = 0;       // a ReqAbility the player lacks
            uint32 level = 0;         // ReqLevel above the player's level
            uint32 other = 0;
            uint32 taught = 0;
        };
        uint32 TeachAvailableSpellsFree(Player* player, FreeTeachTally* tally = nullptr) const;
        // Bit mask (1 << (class - 1)) of the classes this trainer teaches: the union over its spells of the classes their
        // class skill lines name; all classes when the data does not say. Some Forever class spells (Paladin Judgement, Seal of Righteousness) carry no
        // class restriction of their own, so IsSpellFitByClassAndRace lets them through for any class: the trainer decides.
        uint32 GetClassMask() const;
        // Why TeachAvailableSpellsFree would not teach `spellId` to this player, in the order it checks ("would be taught" when it would).
        std::string DescribeFreeTeachRefusal(Player const* player, uint32 spellId) const;

    private:
        Spell const* GetSpell(uint32 spellId) const;
        bool CanTeachSpell(Player const* player, Spell const* trainerSpell) const;
        SpellState GetSpellState(Player const* player, Spell const* trainerSpell) const;
        void SendTeachFailure(Creature const* npc, Player const* player, uint32 spellId, FailReason reason) const;
        std::string const& GetGreeting(LocaleConstant locale) const;

        friend ObjectMgr;
        void AddGreetingLocale(LocaleConstant locale, std::string_view greeting);

        mutable uint32 _classMask = 0;   // 0 = not computed yet
        uint32 _id;
        Type _type;
        std::vector<Spell> _spells;
        std::array<std::string, TOTAL_LOCALES> _greeting;
    };
}

#endif // Trainer_h__
