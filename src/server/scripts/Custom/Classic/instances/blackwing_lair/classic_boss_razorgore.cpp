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

// Classic 1.60 port of VMaNGOS src/scripts/eastern_kingdoms/burning_steppes/blackwing_lair/boss_razorgore.cpp (ScriptDev2 lineage, GPL-2)
// Scripts: boss_razorgore, trigger_orb_of_command

#include "ScriptMgr.h"
#include "GameObject.h"
#include "InstanceScript.h"
#include "Map.h"
#include "MapReference.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "TemporarySummon.h"
#include "ThreatManager.h"
#include "classic_blackwing_lair.h"
#include "classic_script_text.h"
#include <initializer_list>
#include <list>

namespace
{
// Razorgore Phase 2 Script
enum ClassicBwlRazorgore : uint32
{
    CLASSIC_BWL_SAY_RAZ_FREE             = 7980,
    CLASSIC_BWL_EMOTE_RAZ_FLEE           = 9592,
    CLASSIC_BWL_SAY_RAZ_DEATH            = 9591,   // VMaNGOS instance OnCreatureDeath (phase one death)

    CLASSIC_BWL_SPELL_RAZ_CLEAVE         = 19632,
    CLASSIC_BWL_SPELL_RAZ_WARSTOMP       = 24375,
    CLASSIC_BWL_SPELL_RAZ_FIREBALL_VOLLEY = 22425,
    CLASSIC_BWL_SPELL_RAZ_CONFLAGRATION  = 23023,
    CLASSIC_BWL_SPELL_RAZ_SUMMON_PLAYER  = 25104,
    CLASSIC_BWL_MODEL_INVISIBLE          = 11686,

    CLASSIC_BWL_SPELL_RAZ_EXPLOSION      = 20038
};

// North, South, East, West
Position const ClassicBwlRazSpawn[4] =
{
    { -7530.4072f, -1062.4177f, 407.2f },
    { -7659.7128f, -1043.3569f, 407.2f },
    { -7607.4643f, -1116.4702f, 407.2f },
    { -7584.3247f,  -990.5787f, 407.2f }
};

Position const ClassicBwlRazSpawnBis[4] =
{
    { -7547.8886f, -1041.3544f, 407.206f },
    { -7643.7973f, -1065.2515f, 407.207f },
    { -7623.4433f, -1094.6033f, 407.206f },
    { -7567.9516f, -1012.5256f, 407.206f }
};

// VMaNGOS GetCreatureListWithEntryInGrid(list, source, { entries }, range)
void ClassicBwlRazGetCreatureList(std::list<Creature*>& list, WorldObject* source, std::initializer_list<uint32> entries, float range)
{
    for (uint32 entry : entries)
    {
        std::list<Creature*> tmp;
        source->GetCreatureListWithEntryInGrid(tmp, entry, range);
        list.splice(list.end(), tmp);
    }
}

// VMaNGOS SetUInt64Value(UNIT_FIELD_CHANNEL_OBJECT) + SetUInt32Value(UNIT_CHANNEL_SPELL) (channel visual without a cast)
void ClassicBwlRazSetChannel(Unit* unit, ObjectGuid target, uint32 spellId)
{
    unit->ClearChannelObjects();
    if (!target.IsEmpty())
        unit->AddChannelObject(target);
    unit->SetChannelSpellId(spellId);
    SpellCastVisual visual;
    if (spellId)
        if (SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, unit->GetMap()->GetDifficultyID()))
            visual.SpellXSpellVisualID = unit->GetCastSpellXSpellVisualId(spellInfo);
    unit->SetChannelVisual(visual);
}
}

struct classic_boss_razorgore : public ScriptedAI
{
    classic_boss_razorgore(Creature* creature) : ScriptedAI(creature)
    {
        // TODO(classic): VMaNGOS SetUseAiAtControl(true): the AI keeps running while possessed. TC swaps in its PossessedAI
        // while charmed (VMaNGOS only melees during possession, and forwarded charm attack commands in AttackStart).
        m_pInstance = creature->GetInstanceScript();
        m_uiInit = false;
    }

    InstanceScript* m_pInstance;

    uint32 m_uiCleaveTimer = 0;
    uint32 m_uiWarStompTimer = 0;
    uint32 m_uiFireballVolleyTimer = 0;
    uint32 m_uiConflagrationTimer = 0;
    uint32 m_uiInitTimer = 0;
    uint32 m_uiOutOfReachTimer = 0;
    bool m_uiInit;

    uint32 m_uiEvadeTroopsTimer = 0;

    void Reset() override
    {
        SetCombatMovement(true);
        m_uiCleaveTimer         = 9000; // These times are probably wrong
        m_uiWarStompTimer       = 22000;
        m_uiConflagrationTimer  = 12000;
        m_uiFireballVolleyTimer = 7000;
        m_uiOutOfReachTimer     = 10000;
        m_uiInitTimer           = 5000;

        m_uiEvadeTroopsTimer = 5000;
    }

    void SpellHitTarget(WorldObject* target, SpellInfo const* spellInfo) override
    {
        Unit* pTarget = target ? target->ToUnit() : nullptr;
        if (!pTarget)
            return;

        if (spellInfo->Id == CLASSIC_BWL_SPELL_RAZ_WARSTOMP)
            ModifyThreatByPercent(pTarget, -30);
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        if (!m_pInstance)
            return;

        m_pInstance->SetData(CLASSIC_BWL_TYPE_RAZORGORE, IN_PROGRESS);
        // the orb has to be clickable for the whole of phase 1, whatever state the last reset or phase 2 left it in
        if (GameObject* pOrb = me->GetMap()->GetGameObject(m_pInstance->GetGuidData(CLASSIC_BWL_DATA_ORB_DOMINATION_GUID)))
            pOrb->RemoveFlag(GO_FLAG_NOT_SELECTABLE);
        if (Creature* pTrigger = me->GetMap()->GetCreature(m_pInstance->GetGuidData(CLASSIC_BWL_DATA_TRIGGER_GUID)))
            ClassicBwlRazSetChannel(pTrigger, ObjectGuid::Empty, 0);
        DoZoneInCombat();

        // VMaNGOS instance OnCreatureEnterCombat(NPC_RAZORGORE): pull Grethok too
        if (Creature* pGrethok = me->GetMap()->GetCreature(m_pInstance->GetGuidData(CLASSIC_BWL_DATA_GRETOK_GUID)))
            if (pGrethok->IsAlive() && !pGrethok->IsInCombat())
                CreatureAI::DoZoneInCombat(pGrethok);
    }

    void MortPhaseUn()
    {
        for (MapReference const& ref : me->GetMap()->GetPlayers())
            if (Player* pPlayer = ref.GetSource())
                if (pPlayer->IsAlive())
                    pPlayer->CastSpell(pPlayer, CLASSIC_BWL_SPELL_RAZ_EXPLOSION, true);

        if (m_pInstance)
            m_pInstance->SetData(CLASSIC_BWL_TYPE_RAZORGORE, FAIL);

        // VMaNGOS instance OnCreatureDeath: yell when dying while the encounter failed
        ClassicScriptText(CLASSIC_BWL_SAY_RAZ_DEATH, me);

        // VMaNGOS calls Respawn() + SituationInitiale() right inside JustDied. Respawning from JustDied is unsafe in TC, so it is
        // deferred to the corpse's event processor (next update).
        // TODO(classic): with the dynamic respawn system Respawn() creates a new creature object; its AI then runs
        // SituationInitiale from the init timer (5s) instead of immediately.
        m_uiInit = false;
        Creature* self = me;
        me->m_Events.AddEventAtOffset([self]()
        {
            self->Respawn(true);
            if (self->IsAlive())
                if (classic_boss_razorgore* pAI = dynamic_cast<classic_boss_razorgore*>(self->AI()))
                    pAI->SituationInitiale();
        }, 100ms);
    }

    void JustDied(Unit* /*killer*/) override
    {
        if (m_pInstance)
        {
            if (m_pInstance->GetData(CLASSIC_BWL_DATA_EGG) == DONE)
            {
                m_pInstance->SetData(CLASSIC_BWL_TYPE_RAZORGORE, DONE);
                return;
            }
        }
        MortPhaseUn();
    }

    void JustReachedHome() override
    {
        if (m_pInstance)
            m_pInstance->SetData(CLASSIC_BWL_TYPE_RAZORGORE, FAIL);

        SituationInitiale();
        ClassicScriptText(CLASSIC_BWL_SAY_RAZ_FREE, me);
    }

    void SituationInitiale()
    {
        m_uiInit = true;
        if (!m_pInstance)
            return;
        if (m_pInstance->GetData(CLASSIC_BWL_TYPE_RAZORGORE) != IN_PROGRESS)
        {
            std::list<Creature*> lCreature;
            ClassicBwlRazGetCreatureList(lCreature, me, { CLASSIC_BWL_NPC_BLACKWING_LEGGIONAIRE, CLASSIC_BWL_NPC_BLACKWING_MAGE, CLASSIC_BWL_NPC_DEATH_TALON_DRAGONSPAWN }, 250.0f);

            for (Creature* itr : lCreature)
                itr->DespawnOrUnsummon();

            if (GameObject* pGO = me->GetMap()->GetGameObject(m_pInstance->GetGuidData(CLASSIC_BWL_DATA_ORB_DOMINATION_GUID)))
            {
                if (Creature* pCreature = me->GetMap()->GetCreature(m_pInstance->GetGuidData(CLASSIC_BWL_DATA_TRIGGER_GUID)))
                {
                    if (pCreature->AI())
                        pCreature->AI()->EnterEvadeMode(EvadeReason::Other);
                    ClassicBwlRazSetChannel(pCreature, me->GetGUID(), CLASSIC_BWL_SPELL_POSSESS_VISUAL);
                    pCreature->SetDisplayId(CLASSIC_BWL_MODEL_INVISIBLE);
                    pCreature->SetUninteractible(true);
                }
                else if (Creature* pNewTrigger = me->SummonCreature(CLASSIC_BWL_NPC_ORB_OF_DOMINATION, pGO->GetPositionX(), pGO->GetPositionY(), pGO->GetPositionZ(), pGO->GetOrientation(), TEMPSUMMON_MANUAL_DESPAWN))
                {
                    ClassicBwlRazSetChannel(pNewTrigger, me->GetGUID(), CLASSIC_BWL_SPELL_POSSESS_VISUAL);
                    pNewTrigger->SetDisplayId(CLASSIC_BWL_MODEL_INVISIBLE);
                    pNewTrigger->SetUninteractible(true);
                }
            }

            std::list<Creature*> GardesListe;
            ClassicBwlRazGetCreatureList(GardesListe, me, { CLASSIC_BWL_NPC_BLACKWING_GUARDSMAN, CLASSIC_BWL_NPC_GRETHOK_THE_CONTROLLER }, 150.0f);

            for (Creature* itr : GardesListe)
            {
                // TODO(classic): TC only finds guards whose corpse is still in the map
                if (!itr->IsAlive())
                    itr->Respawn(true);

                if (itr->GetEntry() == CLASSIC_BWL_NPC_GRETHOK_THE_CONTROLLER)
                    ClassicBwlRazSetChannel(itr, ObjectGuid::Empty, CLASSIC_BWL_SPELL_POSSESS);
            }

            if (GameObject* pOrb = me->GetMap()->GetGameObject(m_pInstance->GetGuidData(CLASSIC_BWL_DATA_ORB_DOMINATION_GUID)))
                pOrb->RemoveFlag(GO_FLAG_NOT_SELECTABLE); // VMaNGOS GO_FLAG_NO_INTERACT
        }
        // TP to the right place
        me->NearTeleportTo(me->GetRespawnPosition());
    }

    void EvadeTroops()
    {
        std::list<Creature*> lCreatureNear;
        ClassicBwlRazGetCreatureList(lCreatureNear, me, { CLASSIC_BWL_NPC_BLACKWING_LEGGIONAIRE, CLASSIC_BWL_NPC_BLACKWING_MAGE, CLASSIC_BWL_NPC_DEATH_TALON_DRAGONSPAWN }, 250.0f);

        for (Creature* it : lCreatureNear)
            if (it->IsAlive() && it->AI())
                it->AI()->EnterEvadeMode(EvadeReason::Other);
    }

    void UpdateAI(uint32 uiDiff) override
    {
        // VMaNGOS: while possessed only melee (TC: PossessedAI runs instead of this AI)

        if (!m_uiInit)
        {
            if (m_uiInitTimer < uiDiff)
                SituationInitiale();
            else
                m_uiInitTimer -= uiDiff;
            // No AI without initial situation
            return;
        }

        if (!UpdateVictim())
            return;

        if (m_pInstance && m_pInstance->GetData(CLASSIC_BWL_DATA_EGG) == DONE)
        {
            // backup Evade Troops call, in case mages are re-aggroed by their in-flight Fireballs
            if (m_uiEvadeTroopsTimer)
            {
                if (m_uiEvadeTroopsTimer <= uiDiff)
                {
                    EvadeTroops();
                    m_uiEvadeTroopsTimer = 0;
                }
                else
                    m_uiEvadeTroopsTimer -= uiDiff;
            }
        }

        // World of Warcraft Client Patch 1.8.0 (2005-10-11)
        // - Razorgore now has the ability to summon players to him if he cannot reach them for a time.
        if (me->CanNotReachTarget())
        {
            if (m_uiOutOfReachTimer < uiDiff)
            {
                if (DoCastVictim(CLASSIC_BWL_SPELL_RAZ_SUMMON_PLAYER, true) == SPELL_CAST_OK)
                    m_uiOutOfReachTimer = 10000;
            }
            else
                m_uiOutOfReachTimer -= uiDiff;
        }

        if (m_uiCleaveTimer < uiDiff)
        {
            if (DoCastVictim(CLASSIC_BWL_SPELL_RAZ_CLEAVE) == SPELL_CAST_OK)
                m_uiCleaveTimer = urand(5000, 10000);
        }
        else
            m_uiCleaveTimer -= uiDiff;

        // War Stomp
        if (m_uiWarStompTimer < uiDiff)
        {
            if (DoCastSelf(CLASSIC_BWL_SPELL_RAZ_WARSTOMP) == SPELL_CAST_OK)
                m_uiWarStompTimer = urand(25000, 45000);
        }
        else
            m_uiWarStompTimer -= uiDiff;

        // Fireball Volley
        if (m_uiFireballVolleyTimer < uiDiff)
        {
            if (DoCastSelf(CLASSIC_BWL_SPELL_RAZ_FIREBALL_VOLLEY) == SPELL_CAST_OK)
                m_uiFireballVolleyTimer = urand(15000, 20000);
        }
        else
            m_uiFireballVolleyTimer -= uiDiff;

        // Conflagration
        if (m_uiConflagrationTimer < uiDiff)
        {
            if (DoCastSelf(CLASSIC_BWL_SPELL_RAZ_CONFLAGRATION) == SPELL_CAST_OK)
                m_uiConflagrationTimer = urand(15000, 25000);
        }
        else
            m_uiConflagrationTimer -= uiDiff;

        // melee: TC master auto-melee
    }
};

/*######
## trigger_orb_of_command
######*/

struct classic_trigger_orb_of_command : public ScriptedAI
{
    classic_trigger_orb_of_command(Creature* creature) : ScriptedAI(creature)
    {
        m_pInstance = creature->GetInstanceScript();
    }

    InstanceScript* m_pInstance;

    uint32 m_uiPopTimer = 45000;
    bool m_uiRazorgorePhase = true;
    bool m_uiCombatStarted = false;
    ObjectGuid m_uiPossesseurGuid;

    void Reset() override
    {
        m_uiPopTimer       = 45000; // Timer confirmed by BigWigs
        m_uiRazorgorePhase = true;
        m_uiCombatStarted  = false;
        m_uiPossesseurGuid.Clear();
    }

    static void SetRazorgoreCombatMovement(Creature* pRazorgore, bool allow)
    {
        // Only reachable when Razorgore is not possessed (TC swaps the AI while charmed)
        if (classic_boss_razorgore* pAI = dynamic_cast<classic_boss_razorgore*>(pRazorgore->AI()))
            pAI->SetCombatMovement(allow);
    }

    void PhaseSwitch()
    {
        if (!m_pInstance)
            return;

        std::list<Creature*> lCreatureNear;
        ClassicBwlRazGetCreatureList(lCreatureNear, me, { CLASSIC_BWL_NPC_BLACKWING_LEGGIONAIRE, CLASSIC_BWL_NPC_BLACKWING_MAGE, CLASSIC_BWL_NPC_DEATH_TALON_DRAGONSPAWN }, 250.0f);

        for (Creature* it : lCreatureNear)
        {
            if (it->IsAlive())
            {
                ClassicScriptText(CLASSIC_BWL_EMOTE_RAZ_FLEE, it, it);
                it->SetHomePosition(-7555.55f, -1025.16f, 408.4914f, 0.65f);
                // VMaNGOS: UNIT_FLAG_IMMUNE_TO_NPC | UNIT_FLAG_SPAWNING | UNIT_FLAG_PACIFIED | UNIT_FLAG_SILENCED
                // TODO(classic): the vanilla UNIT_FLAG_SILENCED (0x2000) has no TC master unit flag equivalent
                it->SetImmuneToNPC(true);
                it->SetUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
                it->SetUnitFlag(UNIT_FLAG_PACIFIED);
                if (it->AI())
                    it->AI()->EnterEvadeMode(EvadeReason::Other);
            }
        }

        ClassicBwlRazSetChannel(me, ObjectGuid::Empty, 0);

        m_uiRazorgorePhase = false;

        if (GameObject* pOrb = me->GetMap()->GetGameObject(m_pInstance->GetGuidData(CLASSIC_BWL_DATA_ORB_DOMINATION_GUID)))
            pOrb->SetFlag(GO_FLAG_NOT_SELECTABLE); // VMaNGOS GO_FLAG_NO_INTERACT

        // Restore Razorgore
        if (Creature* pRazorgore = me->GetMap()->GetCreature(m_pInstance->GetGuidData(CLASSIC_BWL_DATA_RAZORGORE_GUID)))
        {
            pRazorgore->GetThreatManager().ResetAllThreat();
            SetRazorgoreCombatMovement(pRazorgore, true);
            if (Unit* pPossesser = ObjectAccessor::GetUnit(*me, m_uiPossesseurGuid))
            {
                if (pRazorgore->AI())
                    pRazorgore->AI()->AttackStart(pPossesser);
                pRazorgore->GetMotionMaster()->MoveChase(pPossesser);
                pRazorgore->GetThreatManager().AddThreat(pPossesser, 1000000.f); // Endless threat
            }
            CreatureAI::DoZoneInCombat(pRazorgore);
        }
        me->DespawnOrUnsummon();
    }

    void PopAdd(uint32 uiHow)
    {
        if (!m_pInstance)
            return;

        Creature* pRazorgore = me->GetMap()->GetCreature(m_pInstance->GetGuidData(CLASSIC_BWL_DATA_RAZORGORE_GUID));
        if (!pRazorgore)
            return;

        uint32 uiID = 0;
        bool bSpawnTwo = false;

        // Counting spawned mobs
        std::list<Creature*> lDrake;
        std::list<Creature*> lOrc; // VMaNGOS never fills this list
        uint32 uiType = 0;

        ClassicBwlRazGetCreatureList(lDrake, me, { CLASSIC_BWL_NPC_DEATH_TALON_DRAGONSPAWN, CLASSIC_BWL_NPC_BLACKWING_LEGGIONAIRE, CLASSIC_BWL_NPC_BLACKWING_MAGE }, 250.0f);

        if ((lDrake.size() >= 12) && (lOrc.size() >= 40))
            return;
        else if (lDrake.size() >= 12)
            uiType = urand(1, 2);
        else if (lOrc.size() >= 40)
            uiType = 3;
        else
            uiType = urand(1, 3);

        switch (uiType)
        {
            case 1:
                uiID = CLASSIC_BWL_NPC_BLACKWING_LEGGIONAIRE;
                if (urand(0, 1))
                    bSpawnTwo = true;
                break;
            case 2:
                uiID = CLASSIC_BWL_NPC_BLACKWING_MAGE;
                if (urand(0, 1))
                    bSpawnTwo = true;
                break;
            case 3:
                uiID = CLASSIC_BWL_NPC_DEATH_TALON_DRAGONSPAWN;
                break;
            default:
                break;
        }

        Position pos;
        Position posBis;
        if (!bSpawnTwo)
        {
            bool bSide = urand(0, 1) != 0;
            pos = bSide ? ClassicBwlRazSpawn[uiHow] : ClassicBwlRazSpawnBis[uiHow];
        }
        else
        {
            pos = ClassicBwlRazSpawn[uiHow];
            posBis = ClassicBwlRazSpawnBis[uiHow];
        }

        if (Creature* pCreature = me->SummonCreature(uiID, pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(), 0.0f, TEMPSUMMON_CORPSE_DESPAWN))
        {
            CreatureAI::DoZoneInCombat(pCreature);
            if (pCreature->AI())
                pCreature->AI()->AttackStart(pRazorgore);
            pCreature->GetThreatManager().AddThreat(pRazorgore, 10.0f);
        }
        if (bSpawnTwo)
        {
            if (Creature* pCreature = me->SummonCreature(uiID, posBis.GetPositionX(), posBis.GetPositionY(), posBis.GetPositionZ(), 0.0f, TEMPSUMMON_CORPSE_DESPAWN))
            {
                CreatureAI::DoZoneInCombat(pCreature);
                if (pCreature->AI())
                    pCreature->AI()->AttackStart(pRazorgore);
                pCreature->GetThreatManager().AddThreat(pRazorgore, 10.0f);
            }
        }
    }

    void UpdateAI(uint32 uiDiff) override
    {
        if (!m_pInstance)
            return;

        if (!m_uiCombatStarted && m_pInstance->GetData(CLASSIC_BWL_TYPE_RAZORGORE) == IN_PROGRESS)
        {
            if (Creature* pRazorgore = me->GetMap()->GetCreature(m_pInstance->GetGuidData(CLASSIC_BWL_DATA_RAZORGORE_GUID)))
            {
                if (m_pInstance->GetData(CLASSIC_BWL_DATA_EGG) != DONE)
                    m_uiRazorgorePhase = true;

                m_uiCombatStarted = true;
                DoZoneInCombat();
                CreatureAI::DoZoneInCombat(pRazorgore);

                std::list<Creature*> lGuards;
                ClassicBwlRazGetCreatureList(lGuards, me, { CLASSIC_BWL_NPC_BLACKWING_GUARDSMAN, CLASSIC_BWL_NPC_GRETHOK_THE_CONTROLLER }, 150.0f);

                for (Creature* pGuard : lGuards)
                {
                    if (!pGuard->IsInCombat())
                    {
                        CreatureAI::DoZoneInCombat(pGuard);
                        if (pGuard->GetEntry() == CLASSIC_BWL_NPC_GRETHOK_THE_CONTROLLER)
                            ClassicBwlRazSetChannel(pGuard, ObjectGuid::Empty, 0);
                    }
                }
            }
        }

        if (m_uiRazorgorePhase && m_uiCombatStarted)
        {
            if (m_pInstance->GetData(CLASSIC_BWL_DATA_EGG) == DONE)
            {
                PhaseSwitch();
                return;
            }
            if (Creature* pRazorgore = me->GetMap()->GetCreature(m_pInstance->GetGuidData(CLASSIC_BWL_DATA_RAZORGORE_GUID)))
            {
                if (pRazorgore->HasAura(CLASSIC_BWL_SPELL_POSSESS))
                {
                    if (!m_uiPossesseurGuid)
                    {
                        m_uiPossesseurGuid = pRazorgore->GetCharmerGUID();
                        // VMaNGOS: SetCombatMovement(false), MotionMaster()->Initialize(), StopMoving().
                        // TC: the charm handling already stops Razorgore's own movement and swaps the AI.
                    }

                    pRazorgore->SetMaxHealth(225000);

                    if (Unit* pChanneler = ObjectAccessor::GetUnit(*me, m_uiPossesseurGuid))
                    {
                        if (pChanneler->isDead())
                        {
                            pRazorgore->RemoveAurasDueToSpell(CLASSIC_BWL_SPELL_POSSESS);
                            if (pRazorgore->GetThreatManager().IsThreatListEmpty() && pRazorgore->AI())
                                pRazorgore->AI()->EnterEvadeMode(EvadeReason::NoHostiles);
                        }

                        std::list<Creature*> lCreature;
                        ClassicBwlRazGetCreatureList(lCreature, me, { CLASSIC_BWL_NPC_DEATH_TALON_DRAGONSPAWN, CLASSIC_BWL_NPC_BLACKWING_LEGGIONAIRE, CLASSIC_BWL_NPC_BLACKWING_MAGE }, 150.0f);

                        for (Creature* itr : lCreature)
                        {
                            if (float threat = itr->GetThreatManager().GetThreat(pRazorgore))
                            {
                                itr->GetThreatManager().ResetThreat(pChanneler);
                                itr->GetThreatManager().AddThreat(pChanneler, threat);
                            }
                        }
                    }
                }
                else if (!pRazorgore->HasAura(CLASSIC_BWL_SPELL_POSSESS) && !m_uiPossesseurGuid.IsEmpty()) // Possess finished
                {
                    if (pRazorgore->GetMaxHealth() != CLASSIC_BWL_RAZORGORE_MAX_HEALTH_DURING_POSESSION)
                        pRazorgore->SetMaxHealth(CLASSIC_BWL_RAZORGORE_MAX_HEALTH_DURING_POSESSION);

                    pRazorgore->GetMotionMaster()->Initialize();
                    ClassicBwlRazSetChannel(me, ObjectGuid::Empty, 0);

                    std::list<Creature*> lCreature;
                    ClassicBwlRazGetCreatureList(lCreature, me, { CLASSIC_BWL_NPC_DEATH_TALON_DRAGONSPAWN, CLASSIC_BWL_NPC_BLACKWING_LEGGIONAIRE, CLASSIC_BWL_NPC_BLACKWING_MAGE }, 150.0f);

                    for (Creature* itr : lCreature)
                        itr->GetThreatManager().ResetThreat(pRazorgore);

                    pRazorgore->GetThreatManager().ResetAllThreat();
                    SetRazorgoreCombatMovement(pRazorgore, true);

                    if (Unit* pPossesser = ObjectAccessor::GetUnit(*me, m_uiPossesseurGuid))
                    {
                        // Razorgore must attack the possessor
                        if (pRazorgore->AI())
                            pRazorgore->AI()->AttackStart(pPossesser);
                        pRazorgore->GetMotionMaster()->MoveChase(pPossesser);
                        pRazorgore->GetThreatManager().AddThreat(pPossesser, 1000000.f); // Endless threat
                    }

                    CreatureAI::DoZoneInCombat(pRazorgore);
                    m_uiPossesseurGuid.Clear();
                }
            }

            if (m_uiPopTimer < uiDiff)
            {
                for (uint8 i = 0; i < 4; ++i)
                    PopAdd(i);

                m_uiPopTimer = 15000;
            }
            else
                m_uiPopTimer -= uiDiff;
        }
    }
};

void AddSC_classic_boss_razorgore()
{
    RegisterCreatureAI(classic_boss_razorgore);
    RegisterCreatureAI(classic_trigger_orb_of_command);
}
