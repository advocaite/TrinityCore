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

// Classic 1.60 port of VMaNGOS src/scripts/eastern_kingdoms/stranglethorn_vale/zulgurub/instance_zulgurub.cpp (ScriptDev2 lineage, GPL-2)
// Scripts: instance_zulgurub, event_summon_gahzranka (event 9104), + spell 24693 (Hakkar Power Down, VMaNGOS core script effect)
// + High Priest Thekal, Zealot Lor'Khan and Zealot Zath (VMaNGOS EventAI, sql/migrations/20240408140703_world.sql)

#include "ScriptMgr.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "DB2Stores.h"
#include "GameEventMgr.h"
#include "GameTime.h"
#include "InstanceScript.h"
#include "Map.h"
#include "MotionMaster.h"
#include "Player.h"
#include "Random.h"
#include "ScriptedCreature.h"
#include "Spell.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellScript.h"
#include "classic_script_text.h"
#include "classic_zulgurub.h"
#include <list>
#include <vector>

namespace
{
// DungeonEncounter.db2 ids of the vanilla Zul'Gurub (map 309); only registered when present in the client data
struct ClassicZgEncounterId
{
    uint32 BossId;
    uint32 DungeonEncounterId;
};

ClassicZgEncounterId const ClassicZgEncounterIds[] =
{
    { CLASSIC_ZG_BOSS_VENOXIS,   784 },
    { CLASSIC_ZG_BOSS_JEKLIK,    785 },
    { CLASSIC_ZG_BOSS_MARLI,     786 },
    { CLASSIC_ZG_BOSS_THEKAL,    789 },
    { CLASSIC_ZG_BOSS_GAHZRANKA, 790 },
    { CLASSIC_ZG_BOSS_ARLOKK,    791 },
    { CLASSIC_ZG_BOSS_JINDO,     792 },
    { CLASSIC_ZG_BOSS_HAKKAR,    793 }
    // TODO(classic): 787 (Bloodlord Mandokir) and 788 (Edge of Madness) have no VMaNGOS boss state
};

// VMaNGOS TYPE_* -> TC boss id (-1: not a boss state)
int32 ClassicZgTypeToBoss(uint32 type)
{
    switch (type)
    {
        case CLASSIC_ZG_TYPE_ARLOKK:    return CLASSIC_ZG_BOSS_ARLOKK;
        case CLASSIC_ZG_TYPE_JEKLIK:    return CLASSIC_ZG_BOSS_JEKLIK;
        case CLASSIC_ZG_TYPE_VENOXIS:   return CLASSIC_ZG_BOSS_VENOXIS;
        case CLASSIC_ZG_TYPE_MARLI:     return CLASSIC_ZG_BOSS_MARLI;
        case CLASSIC_ZG_TYPE_THEKAL:    return CLASSIC_ZG_BOSS_THEKAL;
        case CLASSIC_ZG_TYPE_HAKKAR:    return CLASSIC_ZG_BOSS_HAKKAR;
        case CLASSIC_ZG_TYPE_JINDO:     return CLASSIC_ZG_BOSS_JINDO;
        case CLASSIC_ZG_TYPE_GAHZRANKA: return CLASSIC_ZG_BOSS_GAHZRANKA;
        default:                        return -1;
    }
}

constexpr uint32 CLASSIC_ZG_SPELL_GREEN_GHOST_VISUAL = 25039;   // unused: SpawnRandomBoss() is deactivated in VMaNGOS
constexpr uint32 CLASSIC_ZG_AREA_MARLI = 3379;
constexpr uint32 CLASSIC_ZG_SPELL_GAHZRANKA_SPLASH = 12816;
}

class classic_instance_zulgurub : public InstanceMapScript
{
public:
    classic_instance_zulgurub() : InstanceMapScript(ClassicZulGurubScriptName, CLASSIC_ZG_MAP_ID) { }

    struct classic_instance_zulgurub_InstanceScript : public InstanceScript
    {
        explicit classic_instance_zulgurub_InstanceScript(InstanceMap* map) : InstanceScript(map),
            m_ohgan(*this, "Ohgan", NOT_STARTED), m_randomBoss(*this, "RandomBoss", 0)
        {
            SetHeaders(ClassicZulGurubDataHeader);
            SetBossNumber(CLASSIC_ZG_MAX_BOSSES);

            std::vector<DungeonEncounterData> encounters;
            for (ClassicZgEncounterId const& enc : ClassicZgEncounterIds)
                if (sDungeonEncounterStore.LookupEntry(enc.DungeonEncounterId))
                    encounters.push_back({ enc.BossId, {{ enc.DungeonEncounterId }} });
            LoadDungeonEncounterData(encounters);

            m_thekalDeathTime = 0;
            m_thekalRezTime = 0;
            m_randomBossSpawned = false;
            m_gahzrankaSpawnId = CLASSIC_ZG_GAHZRANKA_SPAWN_ID;
        }

        // VMaNGOS m_auiEncounter[]: boss slots are TC boss states, the rest are kept here
        PersistentInstanceScriptValue<uint32> m_ohgan;          // [7]
        PersistentInstanceScriptValue<uint32> m_randomBoss;     // [9]
        uint32 m_thekalDeathTime;                               // [5] (not persisted: transient timer)
        uint32 m_thekalRezTime;                                 // [6] (not persisted: transient timer)
        bool m_randomBossSpawned;

        // Storing Lorkhan, Zath and Thekal because we need to cast on them later. Jindo is needed for heal function too.
        ObjectGuid m_uiLorKhanGUID;
        ObjectGuid m_uiZathGUID;
        ObjectGuid m_uiThekalGUID;
        ObjectGuid m_uiJindoGUID;
        ObjectGuid m_uiHakkarGUID;
        ObjectGuid m_uiGahzrankaGUID;
        ObjectGuid::LowType m_gahzrankaSpawnId;

        ObjectGuid m_uiMarliGUID;
        std::list<ObjectGuid> m_lMarliTrashGUIDList;

        // VMaNGOS Create(): new instance -> pick the Edge of Madness boss
        void Create() override
        {
            InstanceScript::Create();
            m_randomBoss = GenerateRandomBoss();
            if (!m_randomBossSpawned)
                SpawnRandomBoss();
        }

        void AfterDataLoad() override
        {
            // VMaNGOS Load(): IN_PROGRESS -> NOT_STARTED (boss states are handled by TC)
            if (uint32(m_ohgan) == IN_PROGRESS)
                m_ohgan = NOT_STARTED;
            if (!m_randomBossSpawned)
                SpawnRandomBoss();
        }

        // each time High Priest dies lower Hakkar's HP
        void UpdateHakkarPowerStacks()
        {
            Creature* pHakkar = instance->GetCreature(m_uiHakkarGUID);
            if (!pHakkar || !pHakkar->IsAlive())
                return;

            uint32 neededStacks = 0;
            for (uint32 i = 0; i < CLASSIC_ZG_HIGH_PRIEST_COUNT; ++i)
                if (GetBossState(i) != DONE)
                    ++neededStacks;

            uint32 currentStacks = 0;
            if (Aura* pAura = pHakkar->GetAura(CLASSIC_ZG_SPELL_HAKKAR_POWER))
                currentStacks = pAura->GetStackAmount();

            if (neededStacks == currentStacks)
                return;

            if (neededStacks == 0)
            {
                pHakkar->RemoveAurasDueToSpell(CLASSIC_ZG_SPELL_HAKKAR_POWER);
                return;
            }

            if (currentStacks == 0)
            {
                for (uint32 i = 0; i < neededStacks; ++i)
                    pHakkar->CastSpell(pHakkar, CLASSIC_ZG_SPELL_HAKKAR_POWER, true);
            }
            else if (Aura* pAura = pHakkar->GetAura(CLASSIC_ZG_SPELL_HAKKAR_POWER))
                pAura->SetStackAmount(uint8(neededStacks));
        }

        bool IsEncounterInProgress() const override
        {
            for (uint32 i = 0; i < CLASSIC_ZG_MAX_BOSSES; ++i)
                if (GetBossState(i) == IN_PROGRESS || GetBossState(i) == SPECIAL)
                    return true;
            if (uint32(m_ohgan) == IN_PROGRESS || uint32(m_ohgan) == SPECIAL)
                return true;
            return false;
        }

        // VMaNGOS HandleLoadCreature(): a creature of a finished encounter is removed when it (re)spawns
        void HandleLoadCreature(uint32 dataType, ObjectGuid* storeGuid, Creature* pCrea)
        {
            if (GetData(dataType) == DONE)
            {
                if (storeGuid)
                    storeGuid->Clear();
                pCrea->DespawnOrUnsummon(0s, Seconds(7 * DAY));
                return;
            }
            if (storeGuid)
                *storeGuid = pCrea->GetGUID();
        }

        void OnCreatureCreate(Creature* pCreature) override
        {
            InstanceScript::OnCreatureCreate(pCreature);

            switch (pCreature->GetEntry())
            {
                case CLASSIC_ZG_NPC_LORKHAN:
                    HandleLoadCreature(CLASSIC_ZG_TYPE_THEKAL, &m_uiLorKhanGUID, pCreature);
                    break;
                case CLASSIC_ZG_NPC_ZATH:
                    HandleLoadCreature(CLASSIC_ZG_TYPE_THEKAL, &m_uiZathGUID, pCreature);
                    break;
                case CLASSIC_ZG_NPC_THEKAL:
                    HandleLoadCreature(CLASSIC_ZG_TYPE_THEKAL, &m_uiThekalGUID, pCreature);
                    UpdateHakkarPowerStacks();
                    break;
                case CLASSIC_ZG_NPC_JINDO:
                    m_uiJindoGUID = pCreature->GetGUID();
                    break;
                case CLASSIC_ZG_NPC_HAKKAR:
                    HandleLoadCreature(CLASSIC_ZG_TYPE_HAKKAR, &m_uiHakkarGUID, pCreature);
                    // Hakkar's own stacks are applied from his AI (JustAppeared), once he is in the world
                    break;
                case CLASSIC_ZG_NPC_VENOXIS:
                    HandleLoadCreature(CLASSIC_ZG_TYPE_VENOXIS, nullptr, pCreature);
                    UpdateHakkarPowerStacks();
                    break;
                case CLASSIC_ZG_NPC_ARLOKK:
                    HandleLoadCreature(CLASSIC_ZG_TYPE_ARLOKK, nullptr, pCreature);
                    UpdateHakkarPowerStacks();
                    break;
                case CLASSIC_ZG_NPC_MARLI:
                    HandleLoadCreature(CLASSIC_ZG_TYPE_MARLI, nullptr, pCreature);
                    m_uiMarliGUID = pCreature->GetGUID();
                    UpdateHakkarPowerStacks();
                    break;
                case CLASSIC_ZG_NPC_JEKLIK:
                    UpdateHakkarPowerStacks();
                    break;
                case CLASSIC_ZG_NPC_RAZZASHI_SKITTERER:
                case CLASSIC_ZG_NPC_RAZZASHI_VENOMBROOD:
                case CLASSIC_ZG_NPC_HAKARI_SHADOWCASTER:
                case CLASSIC_ZG_NPC_RAZZASHI_BROODWIDOW:
                    m_lMarliTrashGUIDList.push_back(pCreature->GetGUID());
                    break;
                case CLASSIC_ZG_NPC_GAHZRANKA:
                    m_uiGahzrankaGUID = pCreature->GetGUID();
                    if (pCreature->GetSpawnId())
                        m_gahzrankaSpawnId = pCreature->GetSpawnId();
                    break;
                default:
                    break;
            }
        }

        // VMaNGOS OnCreatureDeath()
        void OnUnitDeath(Unit* unit) override
        {
            Creature* pCreature = unit->ToCreature();
            if (!pCreature)
                return;

            if (pCreature->GetEntry() >= CLASSIC_ZG_BOSS_ENTRY_GRILEK && pCreature->GetEntry() <= CLASSIC_ZG_BOSS_ENTRY_WUSHOOLAY)
                SetData(CLASSIC_ZG_TYPE_RANDOM_BOSS, DONE);

            if (pCreature->GetEntry() == CLASSIC_ZG_NPC_NIGHTMARE_ILLUSION)
                pCreature->DespawnOrUnsummon(3s, Seconds(345600000));    // VMaNGOS SetRespawnTime(345600000)
        }

        void SetData(uint32 uiType, uint32 uiData) override
        {
            int32 bossId = ClassicZgTypeToBoss(uiType);
            if (bossId >= 0)
            {
                // TC blocks DONE -> other state transitions
                if (GetBossState(uint32(bossId)) == DONE && uiData != DONE)
                    return;

                SetBossState(uint32(bossId), EncounterState(uiData));

                if (uiType == CLASSIC_ZG_TYPE_MARLI && uiData == IN_PROGRESS)
                {
                    Creature* Marli = instance->GetCreature(m_uiMarliGUID);
                    Unit* pVictim = Marli ? Marli->GetVictim() : nullptr;
                    if (pVictim)
                    {
                        for (ObjectGuid const& guid : m_lMarliTrashGUIDList)
                        {
                            if (Creature* MarliTrash = instance->GetCreature(guid))
                                if (MarliTrash->IsAlive() && !MarliTrash->IsInCombat() && MarliTrash->AI())
                                    if (MarliTrash->GetMapId() == CLASSIC_ZG_MAP_ID && MarliTrash->GetZoneId() == CLASSIC_ZG_ZONE_ID && MarliTrash->GetAreaId() == CLASSIC_ZG_AREA_MARLI)
                                        MarliTrash->AI()->AttackStart(pVictim);
                        }
                    }
                }
                return;
            }

            switch (uiType)
            {
                case CLASSIC_ZG_TYPE_HAKKAR_POWER:
                    UpdateHakkarPowerStacks();
                    break;
                case CLASSIC_ZG_TYPE_THEKAL_DEATH_TIME:
                    m_thekalDeathTime = uiData == SPECIAL ? uint32(GameTime::GetGameTime()) : uiData;
                    break;
                case CLASSIC_ZG_TYPE_THEKAL_REZ_TIME:
                    m_thekalRezTime = uiData == SPECIAL ? uint32(GameTime::GetGameTime()) : uiData;
                    break;
                case CLASSIC_ZG_TYPE_OHGAN:
                    m_ohgan = uiData;
                    break;
                case CLASSIC_ZG_TYPE_RANDOM_BOSS:
                    if (uiData == 0)
                    {
                        // m_auiEncounter[9] = GenerateRandomBoss();
                        if (sGameEventMgr->IsActiveEvent(29))
                            m_randomBoss = uint32(CLASSIC_ZG_BOSS_ENTRY_GRILEK);
                        else if (sGameEventMgr->IsActiveEvent(30))
                            m_randomBoss = uint32(CLASSIC_ZG_BOSS_ENTRY_HAZZARAH);
                        else if (sGameEventMgr->IsActiveEvent(31))
                            m_randomBoss = uint32(CLASSIC_ZG_BOSS_ENTRY_RENATAKI);
                        else if (sGameEventMgr->IsActiveEvent(32))
                            m_randomBoss = uint32(CLASSIC_ZG_BOSS_ENTRY_WUSHOOLAY);
                        // TODO(classic): game_event ids 29-32 are the VMaNGOS Edge of Madness events; verify they exist in the TC world DB
                    }
                    else
                        m_randomBoss = uiData;
                    break;
                default:
                    break;
            }
        }

        uint32 GetData(uint32 uiType) const override
        {
            int32 bossId = ClassicZgTypeToBoss(uiType);
            if (bossId >= 0)
                return GetBossState(uint32(bossId));

            switch (uiType)
            {
                case CLASSIC_ZG_TYPE_THEKAL_DEATH_TIME:
                    return m_thekalDeathTime;
                case CLASSIC_ZG_TYPE_THEKAL_REZ_TIME:
                    return m_thekalRezTime;
                case CLASSIC_ZG_TYPE_OHGAN:
                    return m_ohgan;
                case CLASSIC_ZG_TYPE_RANDOM_BOSS:
                {
                    uint32 randomBoss = m_randomBoss;
                    if (randomBoss >= 15080 && randomBoss <= 15085)
                        return randomBoss;
                    return 0;
                }
                case CLASSIC_ZG_DATA_THEKAL_COND_CAN_REZ:
                    return Thekal_GetUnitThatCanRez() != nullptr ? 1 : 0;
                case CLASSIC_ZG_DATA_THEKAL_COND_NEEDS_REZ:
                {
                    uint32 now = uint32(GameTime::GetGameTime());
                    return (Thekal_GetUnitThatNeedsRez() != nullptr && Thekal_GetUnitCastingRez() == nullptr &&
                        (m_thekalDeathTime + 10 < now) && (m_thekalRezTime + 10 < now)) ? 1 : 0;
                }
                default:
                    break;
            }
            return 0;
        }

        ObjectGuid GetGuidData(uint32 uiData) const override
        {
            switch (uiData)
            {
                case CLASSIC_ZG_DATA_LORKHAN:
                    return m_uiLorKhanGUID;
                case CLASSIC_ZG_DATA_ZATH:
                    return m_uiZathGUID;
                case CLASSIC_ZG_DATA_THEKAL:
                    return m_uiThekalGUID;
                case CLASSIC_ZG_DATA_JINDO:
                    return m_uiJindoGUID;
                case CLASSIC_ZG_DATA_HAKKAR:
                    return m_uiHakkarGUID;
                case CLASSIC_ZG_DATA_GAHZRANKA:
                    return m_uiGahzrankaGUID;
                case CLASSIC_ZG_DATA_THEKAL_NEED_REZ:
                    if (Unit* pTarget = Thekal_GetUnitThatNeedsRez())
                        return pTarget->GetGUID();
                    return ObjectGuid::Empty;
                default:
                    break;
            }
            return ObjectGuid::Empty;
        }

        uint32 GenerateRandomBoss() const
        {
            // VMaNGOS sWorld.GetGameDay()
            uint32 dayCount = uint32(GameTime::GetGameTime() / DAY);
            uint32 weekmod = ((dayCount - (dayCount % 14)) / 14) % 3;
            uint32 bossId = CLASSIC_ZG_BOSS_ENTRY_GRILEK + weekmod;
            return bossId;
        }

        void SpawnRandomBoss()
        {
            m_randomBossSpawned = true;
            // function deactivated in VMaNGOS (would summon m_auiEncounter[9] at -11901.45, -1906.337, 65.37 with visual 25039)
            (void)CLASSIC_ZG_SPELL_GREEN_GHOST_VISUAL;
        }

        Unit* Thekal_GetUnitThatCanRez() const
        {
            if (Creature* pLorKhan = instance->GetCreature(m_uiLorKhanGUID))
                if (pLorKhan->IsAlive() && pLorKhan->GetStandState() != UNIT_STAND_STATE_DEAD)
                    return pLorKhan;
            if (Creature* pZath = instance->GetCreature(m_uiZathGUID))
                if (pZath->IsAlive() && pZath->GetStandState() != UNIT_STAND_STATE_DEAD)
                    return pZath;
            if (Creature* pThekal = instance->GetCreature(m_uiThekalGUID))
                if (pThekal->IsAlive() && pThekal->GetStandState() != UNIT_STAND_STATE_DEAD)
                    return pThekal;
            return nullptr;
        }

        Unit* Thekal_GetUnitThatNeedsRez() const
        {
            if (Creature* pLorKhan = instance->GetCreature(m_uiLorKhanGUID))
                if (pLorKhan->IsAlive() && pLorKhan->GetStandState() == UNIT_STAND_STATE_DEAD)
                    return pLorKhan;
            if (Creature* pZath = instance->GetCreature(m_uiZathGUID))
                if (pZath->IsAlive() && pZath->GetStandState() == UNIT_STAND_STATE_DEAD)
                    return pZath;
            if (Creature* pThekal = instance->GetCreature(m_uiThekalGUID))
                if (pThekal->IsAlive() && pThekal->GetStandState() == UNIT_STAND_STATE_DEAD)
                    return pThekal;
            return nullptr;
        }

        static bool IsCastingThekalRez(Unit const* unit)
        {
            if (Spell* pSpell = unit->GetCurrentSpell(CURRENT_GENERIC_SPELL))
                if (pSpell->GetSpellInfo()->Id == CLASSIC_ZG_SPELL_THEKAL_RESURRECTION)
                    return true;
            return false;
        }

        Unit* Thekal_GetUnitCastingRez() const
        {
            if (Creature* pLorKhan = instance->GetCreature(m_uiLorKhanGUID))
                if (IsCastingThekalRez(pLorKhan))
                    return pLorKhan;
            if (Creature* pZath = instance->GetCreature(m_uiZathGUID))
                if (IsCastingThekalRez(pZath))
                    return pZath;
            if (Creature* pThekal = instance->GetCreature(m_uiThekalGUID))
                if (IsCastingThekalRez(pThekal))
                    return pThekal;
            return nullptr;
        }

        // Used by event_summon_gahzranka: TC removes despawned creatures from the map, so respawn by spawn id
        bool RespawnGahzranka()
        {
            if (Creature* pCreature = instance->GetCreature(m_uiGahzrankaGUID))
            {
                pCreature->Respawn();
                return true;
            }

            if (!m_gahzrankaSpawnId)
                return false;

            instance->Respawn(SPAWN_TYPE_CREATURE, m_gahzrankaSpawnId);
            return true;
        }
    };

    InstanceScript* GetInstanceScript(InstanceMap* map) const override
    {
        return new classic_instance_zulgurub_InstanceScript(map);
    }
};

/*######
## event_summon_gahzranka (event 9104, Mudskunk Lure)
######*/

class classic_event_summon_gahzranka : public EventScript
{
public:
    classic_event_summon_gahzranka() : EventScript("classic_event_summon_gahzranka") { }

    // TC: object = VMaNGOS "target", invoker = VMaNGOS "source"
    void OnTrigger(WorldObject* /*object*/, WorldObject* invoker, uint32 /*eventId*/) override
    {
        // No target or source, block event
        if (!invoker)
            return;

        Player* pPlayer = invoker->ToPlayer();
        if (!pPlayer)
            return;

        InstanceScript* instanceScript = pPlayer->GetInstanceScript();
        if (!instanceScript)
            return;

        auto* m_pInstance = dynamic_cast<classic_instance_zulgurub::classic_instance_zulgurub_InstanceScript*>(instanceScript);
        if (!m_pInstance)
            return;

        // return if already summoned
        if (m_pInstance->GetData(CLASSIC_ZG_TYPE_GAHZRANKA) != NOT_STARTED)
            return;

        pPlayer->CastSpell(pPlayer, CLASSIC_ZG_SPELL_GAHZRANKA_SPLASH, true);

        // VMaNGOS: SetData(IN_PROGRESS) + Respawn() of the stored creature (its AI checks the state on spawn)
        m_pInstance->SetData(CLASSIC_ZG_TYPE_GAHZRANKA, IN_PROGRESS);
        if (!m_pInstance->RespawnGahzranka())
            m_pInstance->SetData(CLASSIC_ZG_TYPE_GAHZRANKA, NOT_STARTED);
    }
};

/*######
## 24693 - Hakkar Power Down
## VMaNGOS core script effect: instance SetData(TYPE_HAKKAR_POWER). Cast (triggered) by the High Priests on death.
######*/

class classic_spell_zg_hakkar_power_down : public SpellScript
{
    void HandleCast()
    {
        // OnCast instead of an effect hook: the caster is dead and targets itself (TARGET_UNIT_CASTER)
        Unit* caster = GetCaster();
        if (!caster)
            return;

        if (InstanceScript* instanceScript = caster->GetInstanceScript())
            instanceScript->SetData(CLASSIC_ZG_TYPE_HAKKAR_POWER, 0);
    }

    void Register() override
    {
        OnCast += SpellCastFn(classic_spell_zg_hakkar_power_down::HandleCast);
    }
};

/*######
## High Priest Thekal, Zealot Lor'Khan, Zealot Zath
## VMaNGOS runs the trio from EventAI (sql/migrations/20240408140703_world.sql: creature_ai_events 14509xx / 11347xx /
## 11348xx, generic_scripts 14508 / 14509 / 1450903, creature_spells 145090 / 145091 / 113470 / 113480). The conditions
## 14509 / 14510 are the instance's CLASSIC_ZG_DATA_THEKAL_COND_CAN_REZ / COND_NEEDS_REZ.
##
## Phase 0: all three fight with their spell list and cannot drop below 1 hp; at 1% they feign death (pacified, lying
## dead, not selectable) and note the time in the instance. Every 10 s a standing one resurrects a fallen one, but only
## when the last fall and the last resurrection are more than 10 s old. Phase 1 (fallen): every 2 s, if nobody is
## standing any more, the zealots despawn and Thekal rises as a tiger (phase 2) with his second spell list; at 20% he
## enrages. Reset / evade restores everything.
######*/

namespace
{
enum ClassicZgThekal : uint32
{
    // texts (broadcast ids; creature_text rows exist for all three)
    SAY_ZEALOT_DIES                 = 10453,    // "%s dies."
    SAY_ZEALOT_RESURRECTED          = 10454,    // "%s is resurrected by a nearby ally!"
    SAY_THEKAL_RAGE                 = 10455,    // "Shirvallah, fill me with your RAGE!"
    SAY_THEKAL_ENRAGE               = 2384,     // "%s becomes enraged!"
    SAY_THEKAL_DEATH                = 10451,    // "Hakkar binds me no more!  Peace at last!"

    SPELL_THEKAL_PACIFY_SELF        = 19951,
    SPELL_THEKAL_HEAL_MAX_HEALTH    = 23329,
    SPELL_THEKAL_RES_IMPACT_VISUAL  = 24171,
    SPELL_THEKAL_TRANSFORM          = 24169,
    SPELL_THEKAL_ENRAGE             = 8269,

    // Thekal, troll form (creature_spells 145090)
    SPELL_THEKAL_SILENCE            = 22666,
    SPELL_THEKAL_MORTAL_CLEAVE      = 22859,
    SPELL_THEKAL_BLOODLUST          = 24185,
    // Thekal, tiger form (creature_spells 145091)
    SPELL_THEKAL_CHARGE             = 24193,
    SPELL_THEKAL_SUMMON_GUARDIANS   = 24183,
    SPELL_THEKAL_FORCE_PUNCH        = 24189,
    SPELL_THEKAL_SPEED_SLASH        = 24192,
    // Zealot Lor'Khan (creature_spells 113470)
    SPELL_LORKHAN_DISARM            = 6713,
    SPELL_LORKHAN_DISPEL_MAGIC      = 17201,
    SPELL_LORKHAN_LIGHTNING_SHIELD  = 20545,
    SPELL_LORKHAN_GREAT_HEAL        = 24208,
    // Zealot Zath (creature_spells 113480)
    SPELL_ZATH_GOUGE                = 12540,
    SPELL_ZATH_SWEEPING_STRIKES     = 18765,
    SPELL_ZATH_SINISTER_STRIKE      = 15581,
    SPELL_ZATH_KICK                 = 15614,
    SPELL_ZATH_BLIND                = 21060,

    THEKAL_PHASE_FIGHTING           = 0,
    THEKAL_PHASE_FALLEN             = 1,
    THEKAL_PHASE_TIGER              = 2
};

constexpr uint32 kThekalRezCheckMs = 10000;     // creature_ai_events 1450904 / 1134704 / 1134804: 10 s initial, 10 s repeat
constexpr uint32 kThekalAllFallenCheckMs = 2000;// creature_ai_events 1450903 / 1134703 / 1134803: 2 s repeat
constexpr uint32 kThekalRezLandMs = 2000;       // generic_scripts 14509: the fallen one stands up 2 s after Resurrection starts
constexpr uint32 kThekalFeignDeathPct = 1;      // creature_ai_events ...02: EVENT_T_HP 1%

// One timed spell of a VMaNGOS creature_spells list (delayInitialMin/Max, delayRepeatMin/Max in seconds)
struct ClassicZgSpellTimer
{
    uint32 Timer = 0;

    void Start(uint32 initialMinSec, uint32 initialMaxSec) { Timer = urand(initialMinSec, initialMaxSec) * IN_MILLISECONDS; }

    // true once per repeat window
    bool Ready(uint32 diff, uint32 repeatMinSec, uint32 repeatMaxSec)
    {
        if (Timer > diff)
        {
            Timer -= diff;
            return false;
        }
        Timer = urand(repeatMinSec, repeatMaxSec) * IN_MILLISECONDS;
        return true;
    }
};
}

// What Thekal and both zealots share: invincibility, feign death, resurrecting a partner, standing up again.
struct classic_zg_zealot_base : public ScriptedAI
{
    classic_zg_zealot_base(Creature* creature) : ScriptedAI(creature), m_pInstance(creature->GetInstanceScript()) { }

    InstanceScript* m_pInstance;
    uint32 m_phase = THEKAL_PHASE_FIGHTING;
    bool m_invincible = true;               // VMaNGOS invincibility threshold 1: no hit takes the unit below 1 hp
    uint32 m_rezCheckTimer = kThekalRezCheckMs;
    uint32 m_allFallenCheckTimer = kThekalAllFallenCheckMs;
    uint32 m_rezLandTimer = 0;
    ObjectGuid m_rezTargetGuid;

    // Phase 0 spell list
    virtual void ResetSpellList() = 0;
    virtual void UpdateSpellList(uint32 diff) = 0;
    // creature_ai_events ...03: nobody left standing
    virtual void OnAllFallen() = 0;

    void Reset() override
    {
        m_phase = THEKAL_PHASE_FIGHTING;
        m_invincible = true;
        m_rezCheckTimer = kThekalRezCheckMs;
        m_allFallenCheckTimer = kThekalAllFallenCheckMs;
        m_rezLandTimer = 0;
        m_rezTargetGuid.Clear();
        StandUp();
        ResetSpellList();
    }

    // The three are one group: pulling one pulls all (VMaNGOS creature_groups leader 49310, flags 2 | 8 | 32)
    void JustEngagedWith(Unit* who) override
    {
        if (!m_pInstance)
            return;
        for (uint32 data : { CLASSIC_ZG_DATA_LORKHAN, CLASSIC_ZG_DATA_ZATH, CLASSIC_ZG_DATA_THEKAL })
            if (Creature* partner = me->GetMap()->GetCreature(m_pInstance->GetGuidData(data)))
                if (partner != me && partner->IsAlive() && !partner->IsInCombat() && partner->AI())
                    partner->AI()->AttackStart(who);
    }

    void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo*/) override
    {
        if (m_invincible && damage >= me->GetHealth())
            damage = me->GetHealth() - 1;
    }

    // generic_scripts 14508 (feign death)
    void FeignDeath()
    {
        m_phase = THEKAL_PHASE_FALLEN;
        m_allFallenCheckTimer = kThekalAllFallenCheckMs;
        m_rezLandTimer = 0;
        me->SetCanMelee(false);
        SetCombatMovement(false);
        me->GetMotionMaster()->MoveIdle();
        me->RemoveAllAuras();
        DoCastSelf(SPELL_THEKAL_PACIFY_SELF, true);
        me->SetStandState(UNIT_STAND_STATE_DEAD);
        me->SetUnitFlag(UNIT_FLAG_UNINTERACTIBLE | UNIT_FLAG_STUNNED);
        ClassicScriptText(SAY_ZEALOT_DIES, me);
        if (m_pInstance)
            m_pInstance->SetData(CLASSIC_ZG_TYPE_THEKAL_DEATH_TIME, SPECIAL);
    }

    // The common part of generic_scripts 14509 (resurrected) and 1450907 / 1134705 (reset): on its feet, able to act
    void StandUp()
    {
        me->RemoveUnitFlag(UNIT_FLAG_UNINTERACTIBLE | UNIT_FLAG_STUNNED);
        me->SetStandState(UNIT_STAND_STATE_STAND);
        me->RemoveAurasDueToSpell(SPELL_THEKAL_PACIFY_SELF);
        me->SetCanMelee(true);
        SetCombatMovement(true);
        if (Unit* victim = me->GetVictim())
            me->GetMotionMaster()->MoveChase(victim);
    }

    // generic_scripts 14509, run on the fallen one 2 s after a partner started Resurrection on it
    void DoAction(int32 action) override
    {
        if (action != CLASSIC_ZG_ACTION_ZEALOT_RESURRECT || m_phase != THEKAL_PHASE_FALLEN)
            return;

        m_phase = THEKAL_PHASE_FIGHTING;
        m_rezCheckTimer = kThekalRezCheckMs;
        DoCastSelf(SPELL_THEKAL_HEAL_MAX_HEALTH, true);
        DoCastSelf(SPELL_THEKAL_RES_IMPACT_VISUAL, true);
        StandUp();
        ClassicScriptText(SAY_ZEALOT_RESURRECTED, me);
    }

    // creature_ai_scripts 1450904 / 1134704 / 1134804: Resurrection on the partner the instance names
    void TryResurrectPartner()
    {
        if (!m_pInstance || !m_pInstance->GetData(CLASSIC_ZG_DATA_THEKAL_COND_NEEDS_REZ))
            return;

        Creature* fallen = me->GetMap()->GetCreature(m_pInstance->GetGuidData(CLASSIC_ZG_DATA_THEKAL_NEED_REZ));
        if (!fallen)
            return;

        m_pInstance->SetData(CLASSIC_ZG_TYPE_THEKAL_REZ_TIME, SPECIAL);
        DoCast(fallen, CLASSIC_ZG_SPELL_THEKAL_RESURRECTION);
        m_rezTargetGuid = fallen->GetGUID();
        m_rezLandTimer = kThekalRezLandMs;
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        if (m_phase == THEKAL_PHASE_FALLEN)
        {
            if (m_allFallenCheckTimer <= diff)
            {
                m_allFallenCheckTimer = kThekalAllFallenCheckMs;
                if (m_pInstance && !m_pInstance->GetData(CLASSIC_ZG_DATA_THEKAL_COND_CAN_REZ))
                    OnAllFallen();
            }
            else
                m_allFallenCheckTimer -= diff;
            return;
        }

        if (m_phase == THEKAL_PHASE_FIGHTING)
        {
            if (me->GetHealthPct() <= float(kThekalFeignDeathPct))
            {
                FeignDeath();
                return;
            }

            if (m_rezLandTimer)
            {
                if (m_rezLandTimer <= diff)
                {
                    m_rezLandTimer = 0;
                    if (Creature* fallen = me->GetMap()->GetCreature(m_rezTargetGuid))
                        if (fallen->AI())
                            fallen->AI()->DoAction(CLASSIC_ZG_ACTION_ZEALOT_RESURRECT);
                }
                else
                    m_rezLandTimer -= diff;
            }

            if (m_rezCheckTimer <= diff)
            {
                m_rezCheckTimer = kThekalRezCheckMs;
                TryResurrectPartner();
            }
            else
                m_rezCheckTimer -= diff;
        }

        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        UpdateSpellList(diff);
    }

    // creature_spells cast targets (the 2024 enum: 14 friendly, 15 most injured friendly, 18 friendly missing the buff but not self)
    Creature* Partner(uint32 data) const
    {
        if (!m_pInstance)
            return nullptr;
        Creature* partner = me->GetMap()->GetCreature(m_pInstance->GetGuidData(data));
        if (partner && partner->IsAlive() && partner->GetStandState() != UNIT_STAND_STATE_DEAD)
            return partner;
        return nullptr;
    }

    std::vector<Creature*> StandingTrio() const
    {
        std::vector<Creature*> trio;
        for (uint32 data : { CLASSIC_ZG_DATA_LORKHAN, CLASSIC_ZG_DATA_ZATH, CLASSIC_ZG_DATA_THEKAL })
            if (Creature* partner = Partner(data))
                trio.push_back(partner);
        return trio;
    }

    Creature* RandomFriendly() const
    {
        std::vector<Creature*> trio = StandingTrio();
        return trio.empty() ? nullptr : trio[urand(0, uint32(trio.size()) - 1)];
    }

    Creature* MostInjuredFriendly() const
    {
        Creature* best = nullptr;
        for (Creature* partner : StandingTrio())
            if (!best || partner->GetHealthPct() < best->GetHealthPct())
                best = partner;
        return best;
    }

    Creature* FriendlyMissingBuffExceptSelf(uint32 spellId) const
    {
        for (Creature* partner : StandingTrio())
            if (partner != me && !partner->HasAura(spellId))
                return partner;
        return nullptr;
    }

    Unit* RandomHostileCasting()
    {
        return SelectTarget(SelectTargetMethod::Random, 0, [](Unit const* target) { return target->HasUnitState(UNIT_STATE_CASTING); });
    }
};

// 14509 - High Priest Thekal
struct classic_boss_thekal : public classic_zg_zealot_base
{
    classic_boss_thekal(Creature* creature) : classic_zg_zealot_base(creature) { }

    ClassicZgSpellTimer m_silence, m_mortalCleave, m_bloodlust;
    ClassicZgSpellTimer m_charge, m_summonGuardians, m_forcePunch, m_speedSlash;
    uint32 m_tigerStep = 0;          // generic_scripts 1450903: the steps at 0 / 2 / 4 / 5 s
    uint32 m_tigerStepTimer = 0;
    bool m_enraged = false;

    void ResetSpellList() override
    {
        m_silence.Start(6, 18);
        m_mortalCleave.Start(12, 24);
        m_bloodlust.Start(20, 40);
        m_tigerStep = 0;
        m_tigerStepTimer = 0;
        m_enraged = false;
        me->LoadEquipment(me->GetOriginalEquipmentId(), true);
        me->RemoveAurasDueToSpell(SPELL_THEKAL_TRANSFORM);
    }

    void Reset() override
    {
        classic_zg_zealot_base::Reset();
        // creature_ai_scripts 1450907 (evade): the encounter and both clocks start over
        if (m_pInstance && m_pInstance->GetData(CLASSIC_ZG_TYPE_THEKAL) != DONE)
        {
            m_pInstance->SetData(CLASSIC_ZG_TYPE_THEKAL, NOT_STARTED);
            m_pInstance->SetData(CLASSIC_ZG_TYPE_THEKAL_DEATH_TIME, 0);
            m_pInstance->SetData(CLASSIC_ZG_TYPE_THEKAL_REZ_TIME, 0);
        }
    }

    void JustEngagedWith(Unit* who) override
    {
        classic_zg_zealot_base::JustEngagedWith(who);
        if (m_pInstance)
            m_pInstance->SetData(CLASSIC_ZG_TYPE_THEKAL, IN_PROGRESS);
    }

    void JustDied(Unit* /*killer*/) override
    {
        ClassicScriptText(SAY_THEKAL_DEATH, me);
        if (m_pInstance)
        {
            m_pInstance->SetData(CLASSIC_ZG_TYPE_THEKAL, DONE);
            m_pInstance->SetData(CLASSIC_ZG_TYPE_THEKAL_DEATH_TIME, DONE);
            m_pInstance->SetData(CLASSIC_ZG_TYPE_THEKAL_REZ_TIME, DONE);
        }
        // Like the other High Priests: one Hakkar Power stack less
        me->CastSpell(me, CLASSIC_ZG_SPELL_HAKKAR_POWER_DOWN, true);
    }

    // creature_ai_events 1450903 -> generic_scripts 1450903: the tiger rises
    void OnAllFallen() override
    {
        m_phase = THEKAL_PHASE_TIGER;
        m_tigerStep = 0;
        m_tigerStepTimer = 0;
        me->RemoveUnitFlag(UNIT_FLAG_STUNNED);
        me->SetStandState(UNIT_STAND_STATE_STAND);
        DoCastSelf(SPELL_THEKAL_RES_IMPACT_VISUAL, true);
        m_tigerStep = 1;
        m_tigerStepTimer = 2000;
    }

    void UpdateTigerRise(uint32 diff)
    {
        if (!m_tigerStep)
            return;
        if (m_tigerStepTimer > diff)
        {
            m_tigerStepTimer -= diff;
            return;
        }

        switch (m_tigerStep)
        {
            case 1: // 2 s: weapons away, roar, the line
                me->LoadEquipment(0, true);
                me->HandleEmoteCommand(EMOTE_ONESHOT_BATTLE_ROAR);
                ClassicScriptText(SAY_THEKAL_RAGE, me);
                m_tigerStep = 2;
                m_tigerStepTimer = 2000;
                break;
            case 2: // 4 s: tiger form, second spell list, selectable and fighting again
                DoCastSelf(SPELL_THEKAL_TRANSFORM, true);
                m_charge.Start(12, 12);
                m_summonGuardians.Start(2, 3);
                m_forcePunch.Start(4, 4);
                m_speedSlash.Start(15, 20);
                me->RemoveUnitFlag(UNIT_FLAG_UNINTERACTIBLE);
                me->RemoveAurasDueToSpell(SPELL_THEKAL_PACIFY_SELF);
                me->SetCanMelee(true);
                SetCombatMovement(true);
                if (Unit* victim = me->GetVictim())
                    me->GetMotionMaster()->MoveChase(victim);
                m_tigerStep = 3;
                m_tigerStepTimer = 1000;
                break;
            case 3: // 5 s: he can die now
                m_invincible = false;
                m_tigerStep = 0;
                break;
            default:
                m_tigerStep = 0;
                break;
        }
    }

    void UpdateAI(uint32 diff) override
    {
        if (m_phase == THEKAL_PHASE_TIGER)
        {
            if (!UpdateVictim())
                return;
            UpdateTigerRise(diff);
            if (me->HasUnitState(UNIT_STATE_CASTING))
                return;
            UpdateSpellList(diff);
            return;
        }

        classic_zg_zealot_base::UpdateAI(diff);
    }

    void UpdateSpellList(uint32 diff) override
    {
        if (m_phase == THEKAL_PHASE_FIGHTING)
        {
            // creature_spells 145090
            if (m_silence.Ready(diff, 6, 24))
                if (Unit* target = RandomHostileCasting())
                    DoCast(target, SPELL_THEKAL_SILENCE);
            if (m_mortalCleave.Ready(diff, 12, 24))
                DoCastVictim(SPELL_THEKAL_MORTAL_CLEAVE);
            if (m_bloodlust.Ready(diff, 20, 40))
                if (Creature* partner = FriendlyMissingBuffExceptSelf(SPELL_THEKAL_BLOODLUST))
                    DoCast(partner, SPELL_THEKAL_BLOODLUST);
            return;
        }

        if (m_phase != THEKAL_PHASE_TIGER || m_tigerStep)
            return;

        // creature_ai_events 1450905: Enrage at 20% once
        if (!m_enraged && me->HealthBelowPct(20))
        {
            m_enraged = true;
            DoCastSelf(SPELL_THEKAL_ENRAGE);
            ClassicScriptText(SAY_THEKAL_ENRAGE, me);
        }

        // creature_spells 145091
        if (m_charge.Ready(diff, 15, 22))
            if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
                DoCast(target, SPELL_THEKAL_CHARGE);
        if (m_summonGuardians.Ready(diff, 50, 50))
            DoCastVictim(SPELL_THEKAL_SUMMON_GUARDIANS);
        if (m_forcePunch.Ready(diff, 16, 21))
            DoCastVictim(SPELL_THEKAL_FORCE_PUNCH);
        if (m_speedSlash.Ready(diff, 20, 25))
            DoCastVictim(SPELL_THEKAL_SPEED_SLASH);
    }
};

// 11347 - Zealot Lor'Khan
struct classic_npc_zealot_lorkhan : public classic_zg_zealot_base
{
    classic_npc_zealot_lorkhan(Creature* creature) : classic_zg_zealot_base(creature) { }

    ClassicZgSpellTimer m_disarm, m_dispelMagic, m_lightningShield, m_greatHeal;

    void ResetSpellList() override
    {
        m_disarm.Start(18, 22);
        m_dispelMagic.Start(15, 25);
        m_lightningShield.Start(25, 35);
        m_greatHeal.Start(10, 20);
    }

    // creature_ai_scripts 1134703: the last one down takes the zealots with him
    void OnAllFallen() override
    {
        m_invincible = false;
        me->DespawnOrUnsummon();
    }

    void UpdateSpellList(uint32 diff) override
    {
        // creature_spells 113470
        if (m_disarm.Ready(diff, 28, 42))
            DoCastVictim(SPELL_LORKHAN_DISARM);
        if (m_dispelMagic.Ready(diff, 15, 25))
            if (Creature* partner = RandomFriendly())
                DoCast(partner, SPELL_LORKHAN_DISPEL_MAGIC);
        if (m_lightningShield.Ready(diff, 25, 35))
            if (!me->HasAura(SPELL_LORKHAN_LIGHTNING_SHIELD))
                DoCastSelf(SPELL_LORKHAN_LIGHTNING_SHIELD);
        if (m_greatHeal.Ready(diff, 15, 20))
            if (Creature* partner = MostInjuredFriendly())
                DoCast(partner, SPELL_LORKHAN_GREAT_HEAL);
    }
};

// 11348 - Zealot Zath
struct classic_npc_zealot_zath : public classic_zg_zealot_base
{
    classic_npc_zealot_zath(Creature* creature) : classic_zg_zealot_base(creature) { }

    ClassicZgSpellTimer m_gouge, m_sweepingStrikes, m_sinisterStrike, m_kick, m_blind;

    void ResetSpellList() override
    {
        m_gouge.Start(12, 24);
        m_sweepingStrikes.Start(20, 40);
        m_sinisterStrike.Start(4, 10);
        m_kick.Start(6, 12);
        m_blind.Start(20, 35);
    }

    // creature_ai_scripts 1134803
    void OnAllFallen() override
    {
        m_invincible = false;
        me->DespawnOrUnsummon();
    }

    void UpdateSpellList(uint32 diff) override
    {
        // creature_spells 113480
        if (m_gouge.Ready(diff, 20, 40))
            DoCastVictim(SPELL_ZATH_GOUGE);
        if (m_sweepingStrikes.Ready(diff, 20, 30))
            DoCastSelf(SPELL_ZATH_SWEEPING_STRIKES);
        if (m_sinisterStrike.Ready(diff, 4, 16))
            DoCastVictim(SPELL_ZATH_SINISTER_STRIKE);
        if (m_kick.Ready(diff, 6, 12))
            if (Unit* target = RandomHostileCasting())
                DoCast(target, SPELL_ZATH_KICK);
        if (m_blind.Ready(diff, 35, 45))
            DoCastVictim(SPELL_ZATH_BLIND);
    }
};

void AddSC_classic_instance_zulgurub()
{
    new classic_instance_zulgurub();
    new classic_event_summon_gahzranka();
    RegisterSpellScript(classic_spell_zg_hakkar_power_down);
    RegisterCreatureAI(classic_boss_thekal);
    RegisterCreatureAI(classic_npc_zealot_lorkhan);
    RegisterCreatureAI(classic_npc_zealot_zath);
}
