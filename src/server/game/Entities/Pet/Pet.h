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

#ifndef TRINITYCORE_PET_H
#define TRINITYCORE_PET_H

#include "PetDefines.h"
#include "TemporarySummon.h"

#define HAPPINESS_LEVEL_SIZE        333000

struct PetSpell
{
    ActiveStates active;
    PetSpellState state;
    PetSpellType type;
};

typedef std::unordered_map<uint32, PetSpell> PetSpellMap;
typedef std::vector<uint32> AutoSpellList;

class Player;
class PetAura;

class TC_GAME_API Pet final : public Guardian
{
    public:
        explicit Pet(Player* owner, PetType type = MAX_PET_TYPE);
        virtual ~Pet();

        void AddToWorld() override;
        void RemoveFromWorld() override;

        float GetNativeObjectScale() const override;
        float GetFollowAngle() const override;
        void SetDisplayId(uint32 modelId, bool setNative = false) override;

        PetType getPetType() const { return m_petType; }
        void setPetType(PetType type) { m_petType = type; }
        bool isControlled() const { return getPetType() == SUMMON_PET || getPetType() == HUNTER_PET; }
        bool isTemporarySummoned() const { return m_duration > 0; }

        bool IsPermanentPetFor(Player* owner) const;        // pet have tab in character windows and set UNIT_FIELD_PETNUMBER

        bool Create(ObjectGuid::LowType guidlow, Map* map, uint32 Entry, uint32 pet_number);
        bool CreateBaseAtCreature(Creature* creature);
        bool CreateBaseAtCreatureInfo(CreatureTemplate const* cinfo, Unit* owner);
        bool CreateBaseAtTamed(CreatureTemplate const* cinfo, Map* map);
        void SendNewlyTamed(bool playPingFx = true) const;
        static std::pair<PetStable::PetInfo const*, PetSaveMode> GetLoadPetInfo(PetStable const& stable, uint32 petEntry, uint32 petnumber, Optional<PetSaveMode> slot);
        bool LoadPetFromDB(Player* owner, uint32 petEntry, uint32 petnumber, bool current, Optional<PetSaveMode> forcedSlot = {});
        bool IsLoading() const override { return m_loading;}
        void SavePetToDB(PetSaveMode mode);
        void FillPetInfo(PetStable::PetInfo* petInfo, Optional<ReactStates> forcedReactState = {}) const;
        void Remove(PetSaveMode mode, bool returnreagent = false);
        static void DeleteFromDB(uint32 petNumber);

        void setDeathState(DeathState s) override;                   // overwrite virtual Creature::setDeathState and Unit::setDeathState
        void Update(uint32 diff) override;                           // overwrite virtual Creature::Update and Unit::Update

        uint8 GetPetAutoSpellSize() const override { return uint8(m_autospells.size()); }
        uint32 GetPetAutoSpellOnPos(uint8 pos) const override
        {
            if (pos >= m_autospells.size())
                return 0;
            else
                return m_autospells[pos];
        }

        void GivePetXP(uint32 xp);
        void GivePetLevel(uint8 level);
        void SetPetExperience(uint32 xp) { SetUpdateFieldValue(m_values.ModifyValue(&Unit::m_unitData).ModifyValue(&UF::UnitData::PetExperience), xp); }
        void SetPetNextLevelExperience(uint32 xp) { SetUpdateFieldValue(m_values.ModifyValue(&Unit::m_unitData).ModifyValue(&UF::UnitData::PetNextLevelExperience), xp); }
        void SynchronizeLevelWithOwner();
        bool HaveInDiet(ItemTemplate const* item) const;

        // Classic 1.60 hunter pet happiness (POWER_HAPPINESS, 0..1000000): vanilla rules
        enum HappinessState : uint8 { UNHAPPY = 1, CONTENT = 2, HAPPY = 3 };
        static constexpr int32 HAPPINESS_MAX = 1000000;
        static constexpr int32 HAPPINESS_TAMED = 166500;         // a freshly tamed beast is unhappy until fed
        static constexpr int32 HAPPINESS_LOSS = 670;             // every 7.5 s, twice that in combat
        static constexpr uint32 HAPPINESS_LOSS_INTERVAL = 7500;
        bool HasHappiness() const { return getPetType() == HUNTER_PET; }
        static uint32 GetHunterPetNextLevelExperience(uint8 level);
        HappinessState GetHappinessState() const;
        float GetHappinessDamageMod() const;                    // unhappy 75%, content 100%, happy 125%
        static int32 GetFoodBenefit(uint8 petLevel, uint32 foodLevel); // happiness per Feed Pet tick, 0 = too low level

        // Classic 1.60 hunter pet loyalty and training points (vanilla rules, VMaNGOS Pet::ModifyLoyalty / GetTPForSpell). The client shows
        // the loyalty level in the stable and the pet paper doll (StablePetInfo::LoyaltyLevel) and gets the training points in UnitData.
        enum PetLoyaltyLevel : uint8 { REBELLIOUS = 1, UNRULY = 2, SUBMISSIVE = 3, DEPENDABLE = 4, FAITHFUL = 5, BEST_FRIEND = 6 };
        static constexpr int32 LOYALTY_TAMED = 1000;             // loyalty points of a freshly tamed beast (Rebellious)
        static constexpr uint32 LOYALTY_TICK_INTERVAL = 12000;
        bool HasLoyalty() const { return getPetType() == HUNTER_PET; }
        uint8 GetLoyaltyLevel() const { return m_loyaltyLevel; }
        int32 GetLoyaltyPoints() const { return m_loyaltyPoints; }
        void InitLoyalty(uint8 loyaltyLevel, int32 loyaltyPoints);
        bool ModifyLoyalty(int32 addvalue);                      // false: the pet ran away (loyalty below 0 while Rebellious)
        void KillLoyaltyBonus(uint8 level);
        static int32 GetLoyaltyLevelUpPoints(uint8 loyaltyLevel);
        static int32 GetLoyaltyStartPoints(uint8 loyaltyLevel);
        static uint32 GetSpellTrainingPoints(uint32 spellId);   // classic_pet_spell_training_points, 0 = free
        uint32 GetTrainingPointsTotal() const;                  // level * (loyalty level - 1)
        uint32 GetTrainingPointsUsed() const;
        void UpdateTrainingPoints();

        void SetDuration(int32 dur) { m_duration = dur; }
        int32 GetDuration() const { return m_duration; }

        /*
        bool UpdateStats(Stats stat);
        bool UpdateAllStats();
        void UpdateResistances(uint32 school);
        void UpdateArmor();
        void UpdateMaxHealth();
        void UpdateMaxPower(Powers power);
        void UpdateAttackPowerAndDamage(bool ranged = false);
        void UpdateDamagePhysical(WeaponAttackType attType) override;
        */

        void ToggleAutocast(SpellInfo const* spellInfo, bool apply);

        bool HasSpell(uint32 spell) const override;

        void LearnPetPassives();
        void LearnClassicStartSpells();   // Classic 1.60: Growl + the family's combat abilities on a fresh hunter pet
        void CheckLearning(uint32 spellId); // Classic 1.60: using an ability can teach the owner its training spell
        void CastPetAuras(bool current);
        void CastPetAura(PetAura const* aura);
        bool IsPetAura(Aura const* aura);

        void _LoadAuras(PreparedQueryResult auraResult, PreparedQueryResult effectResult, uint32 timediff);
        void _SaveAuras(CharacterDatabaseTransaction trans);
        void _LoadSpells(PreparedQueryResult result);
        void _SaveSpells(CharacterDatabaseTransaction trans);

        bool addSpell(uint32 spellId, ActiveStates active = ACT_DECIDE, PetSpellState state = PETSPELL_NEW, PetSpellType type = PETSPELL_NORMAL);
        bool learnSpell(uint32 spell_id);
        void learnSpells(std::vector<uint32> const& spellIds);
        void learnSpellHighRank(uint32 spellid);
        void InitLevelupSpellsForLevel();
        bool unlearnSpell(uint32 spell_id, bool learn_prev, bool clear_ab = true);
        void unlearnSpells(std::vector<uint32> const& spellIds, bool learn_prev, bool clear_ab = true);
        bool removeSpell(uint32 spell_id, bool learn_prev, bool clear_ab = true);
        void CleanupActionBar();
        std::string GenerateActionBarData() const;

        PetSpellMap     m_spells;
        AutoSpellList   m_autospells;

        void InitPetCreateSpells();

        uint16 GetSpecialization() const { return m_petSpecialization; }
        void SetSpecialization(uint16 spec);
        void LearnSpecializationSpells();
        void RemoveSpecializationSpells(bool clearActionBar);

        uint32 GetGroupUpdateFlag() const { return m_groupUpdateMask; }
        void SetGroupUpdateFlag(uint32 flag);
        void ResetGroupUpdateFlag();

        DeclinedName const* GetDeclinedNames() const { return m_declinedname.get(); }

        bool    m_removed;                                  // prevent overwrite pet state in DB at next Pet::Update if pet already removed(saved)

        Player* GetOwner() const;

        std::string GetDebugInfo() const override;

    protected:
        PetType m_petType;
        int32   m_duration;                                 // time until unsummon (used mostly for summoned guardians and not used for controlled pets)
        bool    m_loading;
        uint32  m_focusRegenTimer;
        uint32  m_happinessTimer = HAPPINESS_LOSS_INTERVAL;
        uint32  m_loyaltyTimer = LOYALTY_TICK_INTERVAL;
        uint8   m_loyaltyLevel = REBELLIOUS;
        int32   m_loyaltyPoints = LOYALTY_TAMED;
        uint32  m_groupUpdateMask;

        std::unique_ptr<DeclinedName> m_declinedname;

        uint16 m_petSpecialization;

    private:
        void SaveToDB(uint32, std::vector<Difficulty> const&) override              // override of Creature::SaveToDB     - must not be called
        {
            ABORT();
        }
};
#endif
