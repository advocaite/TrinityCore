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

// Classic 1.60 port of VMaNGOS src/scripts/eastern_kingdoms/burning_steppes/molten_core/boss_ragnaros.cpp (ScriptDev2 lineage, GPL-2)
// Scripts: boss_ragnaros
// VMaNGOS note: can't get Ragnaros to stay in EMOTE_STATE_SUBMERGED during combat, so a stand state is used instead.

#include "ScriptMgr.h"
#include "GameObject.h"
#include "InstanceScript.h"
#include "Log.h"
#include "Map.h"
#include "MotionMaster.h"
#include "Player.h"
#include "QuaternionData.h"
#include "ScriptedCreature.h"
#include "SpellInfo.h"
#include "TemporarySummon.h"
#include "classic_molten_core.h"
#include "classic_script_text.h"
#include <list>
#include <vector>

namespace
{
enum ClassicMcRagnaros : uint32
{
    SAY_RAG_REINFORCEMENTS1       = 8572,
    SAY_RAG_REINFORCEMENTS2       = 8573,
    SAY_RAG_HAND                  = 9426,
    SAY_RAG_WRATH                 = 9427,
    SAY_RAG_KILL                  = 7626,
    SAY_RAG_ARRIVAL5_RAG          = 7685,
    // SAY_MAGMABURST = -1409018 (script_texts, no broadcast text): "MY PATIENCE IS DWINDLING! COME GNATS TO YOUR DEATH!", yell, sound 8048
    SOUND_RAG_MAGMABURST          = 8048,

    SPELL_RAG_ELEMENTAL_FIRE_KILL = 19773, // Kill Majordomo
    SPELL_RAG_ELEMENTAL_FIRE_AURA = 20563, // Aura: trigger Elemental Fire (20564) on every hit
    SPELL_RAG_MELT_WEAPON_AURA    = 21387, // Aura: trigger Melt Weapon (21388) on melee damage taken
    SPELL_RAG_WRATH_OF_RAGNAROS   = 20566, // PBAOE Knockback
    SPELL_RAG_MAGMA_BLAST         = 20565, // Ranged attack when no one in melee range
    SPELL_RAG_MIGHT_OF_RAGNAROS   = 21154, // Summon Flame of Ragnaros trigger to deal Knockback
    SPELL_RAG_INTENSE_HEAT        = 21155, // Knockback cast by Might of Ragnaros triggers
    SPELL_RAG_LAVASHIELD          = 21857, // Son of Flame mana drain aura -- this is applied in creature_template_addon

    SPELL_RAG_SUBMERGE_VISUAL     = 20567,
    SPELL_RAG_SUBMERGE_EFFECT     = 21859,
    SPELL_RAG_EMERGE_VISUAL       = 20568,

    NPC_RAG_FLAME_OF_RAGNAROS     = 13148,
    NPC_RAG_SON_OF_FLAME          = 12143,

    GO_RAG_LAVA_BURST             = 178088,

    MAX_RAG_ADDS_IN_SUBMERGE      = 8
};

char const* const TEXT_RAG_MAGMABURST = "MY PATIENCE IS DWINDLING! COME GNATS TO YOUR DEATH!";

// Lava Burst locations
float const ClassicRagLavaBursts[9][3] =
{
    {812.0f, -821.0f, -232.0f},
    {832.0f, -798.0f, -232.0f},
    {820.0f, -745.0f, -232.0f},
    {865.0f, -807.0f, -232.0f},
    {894.0f, -792.0f, -232.0f},
    {874.0f, -839.0f, -232.0f},
    {862.0f, -869.0f, -232.0f},
    {827.0f, -873.0f, -231.3f},
    {760.0f, -827.0f, -232.0f}
};

// Sons of Ragnaros spawn positions
Position const ClassicRagSonPositions[8] =
{
    { 811.448f, -814.058f, -233.177f, 0.0f },
    { 819.699f, -894.288f, -231.258f, 1.28281f },
    { 825.412f, -869.328f, -231.759f, 1.24253f },
    { 842.542f, -797.822f, -233.340f, 0.0f },
    { 866.345f, -891.080f, -231.449f, 2.01042f },
    { 870.668f, -821.862f, -232.938f, 0.0f },
    { 871.282f, -858.217f, -231.855f, 2.46003f },
    { 887.501f, -791.383f, -231.108f, 3.54988f }
};
}

struct classic_boss_ragnaros : public ScriptedAI
{
    classic_boss_ragnaros(Creature* creature) : ScriptedAI(creature), m_pInstance(creature->GetInstanceScript())
    {
        SetCombatMovement(false);
    }

    uint32 m_uiWrathOfRagnarosTimer = 0;
    uint32 m_uiMightOfRagnarosTimer = 0;
    uint32 m_uiMagmaBlastTimer = 0;
    uint32 m_uiLavaBurstTimer = 0;
    uint32 m_uiLavaBurstSecondaryTimer = 0;
    uint32 m_uiLavaBurstTertiaryTimer = 0;
    uint32 m_uiRestoreTargetTimer = 0;
    uint32 m_uiSubmergeTimer = 0;
    uint32 m_uiAttackTimer = 0;
    uint32 m_uiEmergeStateTimer = 0;
    uint32 m_uiEnterCombatTimer = 0;

    bool HasYelledAggro = false;
    bool HasYelledMagmaBlast = false;
    bool HasSubmergedOnce = false;
    bool IsBanished = false;
    bool HasAura = true;
    bool Explosion = false;
    bool m_bInMelee = false;

    InstanceScript* m_pInstance;

    void Reset() override
    {
        m_uiMagmaBlastTimer         = 2000;
        m_uiWrathOfRagnarosTimer    = urand(25000, 30000);
        m_uiMightOfRagnarosTimer    = urand(10000, 15000);
        m_uiRestoreTargetTimer      = 0;
        m_uiLavaBurstTimer          = urand(10000, 15000);
        m_uiLavaBurstSecondaryTimer = 0;
        m_uiLavaBurstTertiaryTimer  = 0;

        m_uiSubmergeTimer           = 3 * MINUTE * IN_MILLISECONDS;   // P1
        m_uiAttackTimer             = 90 * IN_MILLISECONDS;           // P2
        m_uiEmergeStateTimer        = 0;
        m_uiEnterCombatTimer        = 0;

        HasYelledMagmaBlast         = false;
        HasSubmergedOnce            = false;
        IsBanished                  = false;
        Explosion                   = false;
        HasYelledAggro              = false;
        m_bInMelee                  = false;

        HasAura = true;

        if (m_pInstance && me->IsAlive())
            m_pInstance->SetData(CLASSIC_MC_TYPE_RAGNAROS, NOT_STARTED);
    }

    void JustEngagedWith(Unit* who) override
    {
        if (who && who->GetTypeId() == TYPEID_UNIT && who->GetEntry() == CLASSIC_MC_NPC_MAJORDOMO)
            return;

        if (m_pInstance)
        {
            m_pInstance->SetData(CLASSIC_MC_TYPE_RAGNAROS, IN_PROGRESS);
            me->SetImmuneToPC(false);
        }
        DoZoneInCombat();
        if (!me->HasAura(SPELL_RAG_MELT_WEAPON_AURA))
            DoCastSelf(SPELL_RAG_MELT_WEAPON_AURA, true);
        if (!me->HasAura(SPELL_RAG_ELEMENTAL_FIRE_AURA))
            DoCastSelf(SPELL_RAG_ELEMENTAL_FIRE_AURA, true);
    }

    void SpellHitTarget(WorldObject* target, SpellInfo const* spellInfo) override
    {
        // As Majordomo is now killed, the last timer (until attacking) must be handled with Ragnaros script
        if (spellInfo->Id == SPELL_RAG_ELEMENTAL_FIRE_KILL && target->GetTypeId() == TYPEID_UNIT && target->GetEntry() == CLASSIC_MC_NPC_MAJORDOMO)
            m_uiEnterCombatTimer = 7000;
    }

    void JustDied(Unit* /*killer*/) override
    {
        if (m_pInstance)
            m_pInstance->SetData(CLASSIC_MC_TYPE_RAGNAROS, DONE);
    }

    void KilledUnit(Unit* victim) override
    {
        if (victim->GetEntry() == CLASSIC_MC_NPC_MAJORDOMO && victim->GetTypeId() == TYPEID_UNIT)
            return;

        ClassicScriptText(SAY_RAG_KILL, me);
    }

    void SummonSonsOfFlame()
    {
        // VMaNGOS builds a ThreatListCopier for Ragnaros himself (no effect on the sons); only the explicit attack remains.
        for (Position const& position : ClassicRagSonPositions)
        {
            if (Creature* pSonOfFlame = me->SummonCreature(NPC_RAG_SON_OF_FLAME, position, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 10s))
            {
                if (Unit* randomTarget = SelectTarget(SelectTargetMethod::Random, 0))
                {
                    pSonOfFlame->GetThreatManager().ModifyThreatByPercent(randomTarget, 90);
                    pSonOfFlame->AI()->AttackStart(randomTarget);
                    pSonOfFlame->GetMotionMaster()->MoveChase(randomTarget);
                }
            }
        }
    }

    void UpdateLavaBurstAI(uint32 diff)
    {
        // Lava Bursts spawn in waves of three
        if (m_uiLavaBurstTimer < diff)
        {
            DoLavaBurst();
            m_uiLavaBurstTimer = urand(15000, 20000);
            m_uiLavaBurstSecondaryTimer = urand(2000, 4000);
        }
        else
            m_uiLavaBurstTimer -= diff;

        if (m_uiLavaBurstSecondaryTimer)
        {
            if (m_uiLavaBurstSecondaryTimer < diff)
            {
                DoLavaBurst();
                m_uiLavaBurstSecondaryTimer = 0;
                m_uiLavaBurstTertiaryTimer = urand(2000, 4000);
            }
            else
                m_uiLavaBurstSecondaryTimer -= diff;
        }

        if (m_uiLavaBurstTertiaryTimer)
        {
            if (m_uiLavaBurstTertiaryTimer < diff)
            {
                DoLavaBurst();
                m_uiLavaBurstTertiaryTimer = 0;
            }
            else
                m_uiLavaBurstTertiaryTimer -= diff;
        }
    }

    void DoLavaBurst()
    {
        uint8 const point = uint8(urand(0, 8));
        float const angle = frand(0.0f, float(M_PI));
        if (GameObject* pGo = me->SummonGameObject(GO_RAG_LAVA_BURST, ClassicRagLavaBursts[point][0], ClassicRagLavaBursts[point][1], ClassicRagLavaBursts[point][2],
            angle, QuaternionData::fromEulerAnglesZYX(angle, 0.0f, 0.0f), 0s))
        {
            pGo->Use(me);
        }
    }

    void UpdateAI(uint32 diff) override
    {
        // After killing Majordomo
        if (m_uiEnterCombatTimer)
        {
            if (m_uiEnterCombatTimer <= diff)
            {
                if (!HasYelledAggro)
                {
                    // 7 seconds have passed, do emote and yell
                    me->HandleEmoteCommand(EMOTE_ONESHOT_ROAR);
                    ClassicScriptText(SAY_RAG_ARRIVAL5_RAG, me);
                    DoZoneInCombat();
                    HasYelledAggro = true;
                    m_uiEnterCombatTimer = 3000;
                    return;
                }
                else
                {
                    // 10 seconds have passed, engage the raid
                    me->SetUninteractible(false);   // VMaNGOS: UNIT_FLAG_UNINTERACTIBLE
                    me->SetImmuneToPC(false);
                    m_uiEnterCombatTimer = 0;
                    // TC: the zone-in-combat above is refused while the unit is still immune to players
                    DoZoneInCombat();

                    // Despawn Majordomo's corpse
                    if (Creature* domo = me->FindNearestCreature(CLASSIC_MC_NPC_MAJORDOMO, 20.f, false))
                        domo->DespawnOrUnsummon(5s);
                }
            }
            else
            {
                m_uiEnterCombatTimer -= diff;
                return;
            }
        }

        if (me->IsImmuneToPC())
            return;

        // For transition back to P1, return during emerge visual cast animation
        if (m_uiEmergeStateTimer)
        {
            if (m_uiEmergeStateTimer <= diff)
                m_uiEmergeStateTimer = 0;
            else
            {
                m_uiEmergeStateTimer -= diff;
                return;
            }
        }

        // Phase 2 -----------------------------------------------------------------------------------

        if (IsBanished)
        {
            if (m_uiAttackTimer < diff)
            {
                me->RemoveAurasDueToSpell(SPELL_RAG_SUBMERGE_VISUAL);
                me->RemoveAurasDueToSpell(SPELL_RAG_SUBMERGE_EFFECT);
                me->SetStandState(UNIT_STAND_STATE_STAND);

                if (DoCastSelf(SPELL_RAG_EMERGE_VISUAL) == SPELL_CAST_OK)
                {
                    // Become unbanished again
                    IsBanished = false;
                    TC_LOG_INFO("scripts", "[MoltenCore.Ragnaros] emerged");
                    m_uiMagmaBlastTimer = 3000;
                    HasYelledMagmaBlast = false;
                    m_uiEmergeStateTimer = 2900;
                    me->SetCanMelee(true);
                    return;
                }

                TC_LOG_ERROR("scripts", "[MoltenCore.Ragnaros] Emerge failed.");
            }
            else
            {
                m_uiAttackTimer -= diff;
                if (m_uiAttackTimer > 1500)
                {
                    bool allBanished = true;
                    std::list<Creature*> sonList;
                    me->GetCreatureListWithEntryInGrid(sonList, NPC_RAG_SON_OF_FLAME, 150.0f);

                    for (Creature* son : sonList)
                    {
                        if (son->IsAlive())
                        {
                            // VMaNGOS: UNIT_STATE_ISOLATED (banished)
                            if (!son->HasAuraWithMechanic(1 << MECHANIC_BANISH))
                            {
                                allBanished = false;
                                break;
                            }
                        }
                    }
                    if (allBanished)
                        m_uiAttackTimer = 0;
                }

                if (!me->HasAura(SPELL_RAG_SUBMERGE_VISUAL))
                    me->AddAura(SPELL_RAG_SUBMERGE_VISUAL, me);

                if (!me->HasAura(SPELL_RAG_SUBMERGE_EFFECT))
                    me->AddAura(SPELL_RAG_SUBMERGE_EFFECT, me);

                UpdateLavaBurstAI(diff);
                return;
            }
        }

        // Return since we have no target
        if (!UpdateVictim())
            return;

        // Phase 1 -----------------------------------------------------------------------------------

        UpdateLavaBurstAI(diff);

        // Restore target after casting Might of Ragnaros
        if (m_uiRestoreTargetTimer)
        {
            if (m_uiRestoreTargetTimer <= diff)
            {
                if (Unit* pTarget = me->GetVictim())
                {
                    me->SetTarget(pTarget->GetGUID());
                    me->SetFacingToObject(pTarget);
                    m_uiRestoreTargetTimer = 0;
                }
            }
            else
                m_uiRestoreTargetTimer -= diff;
        }

        // Timer to Phase 2
        if (!IsBanished && m_uiSubmergeTimer < diff)
        {
            if (DoCastSelf(SPELL_RAG_SUBMERGE_EFFECT, true) == SPELL_CAST_OK)
            {
                if (DoCastSelf(SPELL_RAG_SUBMERGE_VISUAL, true) == SPELL_CAST_OK)
                {
                    ClassicScriptText(HasSubmergedOnce ? SAY_RAG_REINFORCEMENTS2 : SAY_RAG_REINFORCEMENTS1, me);
                    me->SetStandState(UNIT_STAND_STATE_SUBMERGED);  // VMaNGOS: UNIT_STAND_STATE_CUSTOM
                    me->SetCanMelee(false);                         // VMaNGOS returns before melee while submerged
                    SummonSonsOfFlame();

                    TC_LOG_INFO("scripts", "[MoltenCore.Ragnaros] submerged ({}), Sons of Flame summoned, emerges in 90 s",
                        HasSubmergedOnce ? "second or later" : "first");
                    HasSubmergedOnce = true;
                    IsBanished = true;
                    m_uiSubmergeTimer = 3 * MINUTE * IN_MILLISECONDS;
                    m_uiAttackTimer = 90 * IN_MILLISECONDS;
                    return;
                }
            }

            TC_LOG_ERROR("scripts", "[MoltenCore.Ragnaros] Submerge failed.");
        }
        else if (m_uiSubmergeTimer >= diff)
            m_uiSubmergeTimer -= diff;

        // Wrath of Ragnaros
        if (m_uiWrathOfRagnarosTimer < diff)
        {
            if (DoCastSelf(SPELL_RAG_WRATH_OF_RAGNAROS) == SPELL_CAST_OK)
            {
                ResetThreatList();
                m_uiWrathOfRagnarosTimer = urand(25000, 30000);
                ClassicScriptText(SAY_RAG_WRATH, me);
            }
        }
        else
            m_uiWrathOfRagnarosTimer -= diff;

        // Might of Ragnaros
        if (m_uiMightOfRagnarosTimer < diff)
        {
            std::vector<Player*> manaPlayers;

            for (ThreatReference const* ref : me->GetThreatManager().GetUnsortedThreatList())
            {
                Player* pPlayer = ref->GetVictim()->ToPlayer();
                if (pPlayer && pPlayer->IsAlive() && pPlayer->GetPowerType() == POWER_MANA && !pPlayer->IsGameMaster())
                    manaPlayers.push_back(pPlayer);
            }
            if (!manaPlayers.empty())
            {
                if (Player* pTarget = manaPlayers[urand(0, uint32(manaPlayers.size()) - 1)])
                {
                    if (DoCast(pTarget, SPELL_RAG_MIGHT_OF_RAGNAROS) == SPELL_CAST_OK)
                    {
                        me->SetInFront(pTarget);
                        me->SetTarget(pTarget->GetGUID());  // Ragnaros faces targets he casts Might of Ragnaros on
                        m_uiMightOfRagnarosTimer = urand(9000, 14000);
                        m_uiRestoreTargetTimer = 800;
                        if (urand(0, 1))
                            ClassicScriptText(SAY_RAG_HAND, me);
                    }
                }
            }
        }
        else
            m_uiMightOfRagnarosTimer -= diff;

        // every tick we check for melee targets to attack
        CheckForMelee();

        if (m_bInMelee && HasYelledMagmaBlast)
        {
            HasYelledMagmaBlast = false;
            m_uiMagmaBlastTimer = 3000;
        }

        // no one is engaged in melee for some seconds - burn, baby, burn
        if (!m_bInMelee)
        {
            // Magma Blast
            if (m_uiMagmaBlastTimer < diff)
            {
                if (!HasYelledMagmaBlast)
                {
                    me->Yell(TEXT_RAG_MAGMABURST, LANG_UNIVERSAL);
                    me->PlayDirectSound(SOUND_RAG_MAGMABURST);
                    HasYelledMagmaBlast = true;
                }

                // at first we try to select player, then pet
                Unit* target = SelectTarget(SelectTargetMethod::Random, 0, [](Unit* u)
                {
                    return u->GetTypeId() == TYPEID_PLAYER && !u->ToPlayer()->IsGameMaster();
                });

                if (!target)
                    target = SelectTarget(SelectTargetMethod::Random, 0, [](Unit* u) { return u->IsPet(); });

                if (target)
                {
                    if (DoCast(target, SPELL_RAG_MAGMA_BLAST) == SPELL_CAST_OK)
                        m_uiMagmaBlastTimer = 2500;

                    return;
                }

                TC_LOG_ERROR("scripts", "[MoltenCore.Ragnaros] No target to Magma Blast.");
            }
            else
                m_uiMagmaBlastTimer -= diff;
        }
    }

    // TC master handles the actual swings (auto-melee on the current victim); this only reproduces the VMaNGOS target
    // switching by moving threat onto a unit that is actually in melee range.
    void SwitchMeleeTarget(Unit* pTarget)
    {
        if (Unit* victim = me->GetVictim())
            if (victim != pTarget)
                me->GetThreatManager().ModifyThreatByPercent(victim, -100);   // erase current target's threat

        // give the new target aggro
        me->GetThreatManager().ModifyThreatByPercent(pTarget, 100);
    }

    void CheckForMelee()
    {
        // at first we check for the current player-type target
        Unit* pMainTarget = me->GetVictim();
        if (pMainTarget && pMainTarget->GetTypeId() == TYPEID_PLAYER && !pMainTarget->ToPlayer()->IsGameMaster() &&
            me->IsWithinMeleeRange(pMainTarget) && me->IsWithinLOSInMap(pMainTarget))
        {
            m_bInMelee = true;
            return;
        }

        Creature* self = me;

        // at second we look for any melee player-type target (if current target is not reachable)
        if (Unit* pTarget = SelectTarget(SelectTargetMethod::MaxThreat, 0, [self](Unit* u)
            {
                return u->GetTypeId() == TYPEID_PLAYER && !u->ToPlayer()->IsGameMaster() && self->IsWithinLOSInMap(u) && self->IsWithinMeleeRange(u);
            }))
        {
            m_bInMelee = true;
            SwitchMeleeTarget(pTarget);
            return;
        }

        // reaching this point means there are no more reachable player-type targets in melee range
        m_bInMelee = false;

        // at third we take any melee pet target just to punch in the face
        if (Unit* pTarget = SelectTarget(SelectTargetMethod::MaxThreat, 0, [self](Unit* u)
            {
                return u->IsPet() && self->IsWithinLOSInMap(u) && self->IsWithinMeleeRange(u);
            }))
        {
            SwitchMeleeTarget(pTarget);
            return;
        }

        // at fourth we take anything to wipe it out
        if (Unit* pTarget = SelectTarget(SelectTargetMethod::MaxThreat, 0, [self](Unit* u)
            {
                return u->GetTypeId() != TYPEID_PLAYER && self->IsWithinLOSInMap(u) && self->IsWithinMeleeRange(u);
            }))
        {
            SwitchMeleeTarget(pTarget);
        }

        // nothing in melee at all
    }
};

void AddSC_classic_boss_ragnaros()
{
    RegisterCreatureAI(classic_boss_ragnaros);
}
