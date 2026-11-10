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

// Classic 1.60 port of VMaNGOS src/scripts/eastern_kingdoms/burning_steppes/molten_core/boss_majordomo_executus.cpp (GPL-2)
// Scripts: boss_majordomo

#include "ScriptMgr.h"
#include "GameObject.h"
#include "InstanceScript.h"
#include "Map.h"
#include "MotionMaster.h"
#include "Player.h"
#include "QuaternionData.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "TemporarySummon.h"
#include "classic_molten_core.h"
#include "classic_script_text.h"

namespace
{
enum ClassicMcMajordomo : uint32
{
    POINT_DOMO_RESPAWN               = 1,
    POINT_DOMO_SUMMON1               = 2,
    POINT_DOMO_SUMMON2               = 3,
    POINT_DOMO_SUMMON3               = 4,

    SAY_DOMO_AGGRO                   = 7612,
    SAY_DOMO_SLAY                    = 9425,
    SAY_DOMO_DEFEAT1                 = 7561,
    SAY_DOMO_DEFEAT2                 = 7567,
    SAY_DOMO_DEFEAT3                 = 7568,

    SAY_DOMO_LAST_ADD                = 8545,
    SAY_DOMO_MAJ                     = 7655,
    SAY_DOMO_SUMMON_MAJ              = 7657,
    SAY_DOMO_ARRIVAL1_RAG            = 7636,
    SAY_DOMO_ARRIVAL2_MAJ            = 7661,
    SAY_DOMO_ARRIVAL3_RAG            = 7662,

    SPELL_DOMO_AEGIS_OF_RAGNAROS     = 20620,  // Cast at start and at 50% health to fully heal Majordomo Executus

    SPELL_DOMO_MAGIC_REFLECTION      = 20619,  // Cast one of these every 30 seconds
    SPELL_DOMO_DAMAGE_SHIELD         = 21075,  // Cast one of these every 30 seconds

    SPELL_DOMO_TELEPORT_TARGET       = 20534,  // Teleport current target
    SPELL_DOMO_TELEPORT_RANDOM       = 20618,  // Teleport random target

    SPELL_DOMO_ENCOURAGEMENT         = 21086,  // Cast onto all remaining adds every time one is killed
    SPELL_DOMO_IMMUNITY              = 21087,  // Cast onto Flamewaker Healers when half the adds are dead
    SPELL_DOMO_CHAMPION              = 21090,  // Cast onto the last remaining add
    SPELL_DOMO_SEPARATION_ANXIETY    = 21094,  // Cast onto all adds at start by Majordomo Executus, if adds move out of range, they will cast spell 21095 on themselves

    SPELL_DOMO_VISUAL_TELEPORT       = 19484,  // Visual used after fight ending dialogue to teleport to Ragnaros
    SPELL_DOMO_MAJORDOMO_TELEPORT    = 19527,  // Spell cast to facilitate teleporting of Majordomo to Ragnaros
    SPELL_DOMO_ELEMENTAL_FIRE        = 19773,  // Spell used by Ragnaros to kill Majordomo Executus
    SPELL_DOMO_SUMMON_RAGNAROS       = 19774,  // Spell used by Majordomo Executus to summon Ragnaros

    OBJECT_DOMO_LAVA_STEAM           = 178107, // Lava steam spawned before Ragnaros
    OBJECT_DOMO_LAVA_SPLASH          = 178108, // Lava splashes spawned before Ragnaros

    // VMaNGOS gossip_menu_option 4108/0 -> gossip_scripts 4108 (SCRIPT_COMMAND_SEND_SCRIPT_EVENT + say 7649)
    GOSSIP_MENU_DOMO_SUMMON_RAG      = 4108,
    GOSSIP_OPTION_DOMO_SUMMON_RAG    = 0,
    SAY_DOMO_GOSSIP_START            = 7649
};

float const POINT_DOMO_RESPAWN_O = 2.91086f;

float const POINT_DOMO_SUMMON1_X = 839.1729f;
float const POINT_DOMO_SUMMON1_Y = -811.2748f;
float const POINT_DOMO_SUMMON1_Z = -229.5895f;

float const POINT_DOMO_SUMMON2_X = 830.4840f;
float const POINT_DOMO_SUMMON2_Y = -814.4016f;
float const POINT_DOMO_SUMMON2_Z = -228.9452f;

struct ClassicMcDomoSpawnLocation
{
    uint32 m_uiEntry;
    float m_fX, m_fY, m_fZ, m_fO;
};

ClassicMcDomoSpawnLocation const ClassicMcDomoAddSpawns[8] =
{
    {CLASSIC_MC_NPC_FLAMEWAKER_ELITE,  737.945f, -1156.48f, -118.945f, 4.46804f},
    {CLASSIC_MC_NPC_FLAMEWAKER_ELITE,  752.520f, -1191.02f, -118.218f, 2.49582f},
    {CLASSIC_MC_NPC_FLAMEWAKER_ELITE,  752.953f, -1163.94f, -118.869f, 3.70010f},
    {CLASSIC_MC_NPC_FLAMEWAKER_ELITE,  738.814f, -1197.40f, -118.018f, 1.83260f},
    {CLASSIC_MC_NPC_FLAMEWAKER_HEALER, 746.939f, -1194.87f, -118.016f, 2.21657f},
    {CLASSIC_MC_NPC_FLAMEWAKER_HEALER, 747.132f, -1158.87f, -118.897f, 4.03171f},
    {CLASSIC_MC_NPC_FLAMEWAKER_HEALER, 757.116f, -1170.12f, -118.793f, 3.40339f},
    {CLASSIC_MC_NPC_FLAMEWAKER_HEALER, 755.910f, -1184.46f, -118.449f, 2.80998f}
};
}

struct classic_boss_majordomo : public ScriptedAI
{
    classic_boss_majordomo(Creature* creature) : ScriptedAI(creature), m_pInstance(creature->GetInstanceScript()) { }

    ObjectGuid m_addSpawns[8];
    uint32 Reflection_Timer = 30000;
    uint32 TPDomo_Timer = 0;
    uint32 AddCount = 8;
    bool AddSpawn = false;

    InstanceScript* m_pInstance;

    // Post-defeat dialogue sequence timer
    uint32 DialogueDefeatTimer = 0;
    bool DialogueDefeatStart0 = false;
    bool DialogueDefeatStart1 = false;
    bool DialogueDefeatStart2 = false;
    bool DialogueDefeatStart3 = false;
    bool DialogueTeleportStart = false;
    bool DialogueTeleportFinished = false;

    // Ragnaros Event
    uint32 DialogueRagnaros_M = 0;
    uint32 DialogueRagnarosTimer = 0;
    bool RagnarosEventStart = false;

    // VMaNGOS marks the defeat with UNIT_FLAG_PET_RENAME (checked in JustReachedHome); a member is used instead
    bool m_bDefeatMarker = false;
    // VMaNGOS invincibility hp threshold (removed right before Ragnaros kills him)
    bool m_bAllowDeath = false;

    bool IsFriendly() const { return me->GetFaction() == CLASSIC_MC_FACTION_DOMO_FRIENDLY; }

    // Majordomo can respawn after 2 hours; the event progress *must* stay DONE
    void Reset() override
    {
        me->SetDefaultMovementType(IDLE_MOTION_TYPE);
        Reflection_Timer = 30000;
        TPDomo_Timer = 10000 + urand(0, 19999);
        AddSpawn = false;
        AddCount = 8;

        for (ObjectGuid& guid : m_addSpawns)
        {
            if (!guid.IsEmpty())
            {
                if (Creature* pOldAdd = me->GetMap()->GetCreature(guid))
                {
                    pOldAdd->DespawnOrUnsummon();
                    guid.Clear();
                }
            }
        }

        if (m_pInstance && m_pInstance->GetData(CLASSIC_MC_TYPE_MAJORDOMO) != DONE)
            m_pInstance->SetData(CLASSIC_MC_TYPE_MAJORDOMO, NOT_STARTED);

        // Post-defeat dialogue event
        DialogueDefeatTimer = 0;
        DialogueDefeatStart0 = false;
        DialogueDefeatStart1 = false;
        DialogueDefeatStart2 = false;
        DialogueDefeatStart3 = false;

        // Ragnaros Event
        DialogueRagnaros_M = 0;
        DialogueRagnarosTimer = 0;
        RagnarosEventStart = false;
    }

    template <typename Fn>
    void ForEachAliveAdd(Fn&& fn)
    {
        for (ObjectGuid const& guid : m_addSpawns)
            if (!guid.IsEmpty())
                if (Creature* pAdd = me->GetMap()->GetCreature(guid))
                    if (pAdd->IsAlive())
                        fn(pAdd);
    }

    // Handler for spells cast on adds death
    void SummonedCreatureDies(Creature* /*summon*/, Unit* /*killer*/) override
    {
        if (AddCount > 0)
            AddCount--;

        if (AddCount > 0)
        {
            ForEachAliveAdd([this](Creature* pAdd)
            {
                DoCast(pAdd, SPELL_DOMO_ENCOURAGEMENT);
            });
        }
        if (AddCount <= 4)
        {
            ForEachAliveAdd([this](Creature* pAdd)
            {
                if (!pAdd->HasAura(SPELL_DOMO_IMMUNITY))
                    DoCast(pAdd, SPELL_DOMO_IMMUNITY, true);
            });
        }
        if (AddCount == 1)
        {
            ForEachAliveAdd([this](Creature* pAdd)
            {
                ClassicScriptText(SAY_DOMO_LAST_ADD, me);
                DoCast(pAdd, SPELL_DOMO_CHAMPION, true);
            });
        }
        else if (AddCount == 0)
        {
            if (m_pInstance)
                if (m_pInstance->GetData(CLASSIC_MC_TYPE_MAJORDOMO) != DONE)
                    m_pInstance->SetData(CLASSIC_MC_TYPE_MAJORDOMO, DONE);

            EnterEvadeMode(EvadeReason::Other);
            // VMaNGOS: UNIT_FIELD_FLAGS = PET_RENAME | IMMUNE_TO_PLAYER | IN_COMBAT
            m_bDefeatMarker = true;
            me->SetImmuneToPC(true);
        }
    }

    // Handler for beginning dialogue sequence
    void JustReachedHome() override
    {
        if (m_bDefeatMarker && !DialogueDefeatStart0)
            DialogueDefeatStart0 = true;
    }

    // Dialogue: "Ashes to ashes!"
    void KilledUnit(Unit* /*victim*/) override
    {
        if (!IsFriendly())
            ClassicScriptText(SAY_DOMO_SLAY, me);
    }

    void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo*/) override
    {
        // TODO(classic): VMaNGOS keeps Majordomo alive with an invincibility hp threshold (lifted at the end of the
        // Ragnaros summoning, see DomoEvent case 76). Emulated here by clamping lethal damage.
        if (!m_bAllowDeath && damage >= me->GetHealth())
            damage = me->GetHealth() > 1 ? uint32(me->GetHealth() - 1) : 0;
    }

    // Aggro handler
    void JustEngagedWith(Unit* who) override
    {
        if (!IsFriendly())
        {
            ForEachAliveAdd([this](Creature* pAdd)
            {
                if (!pAdd->HasAura(SPELL_DOMO_SEPARATION_ANXIETY))
                    DoCast(pAdd, SPELL_DOMO_SEPARATION_ANXIETY, true);
            });

            DoCastSelf(SPELL_DOMO_AEGIS_OF_RAGNAROS, true);
            ClassicScriptText(SAY_DOMO_AGGRO, me);

            // VMaNGOS instance OnCreatureEnterCombat: link Majordomo and his adds
            if (who)
            {
                ForEachAliveAdd([who](Creature* pAdd)
                {
                    if (!pAdd->IsEngaged())
                        pAdd->EngageWithTarget(who);
                });
            }
        }

        if (m_pInstance && m_pInstance->GetData(CLASSIC_MC_TYPE_MAJORDOMO) != DONE)
            m_pInstance->SetData(CLASSIC_MC_TYPE_MAJORDOMO, IN_PROGRESS);
    }

    // Movement handler for Ragnaros summoning sequence
    void MovementInform(uint32 uiType, uint32 uiPointId) override
    {
        if (uiType != POINT_MOTION_TYPE)
            return;

        switch (uiPointId)
        {
            // Face forward when resetting or before defeat dialogue
            case POINT_DOMO_RESPAWN:
                me->SetFacingTo(POINT_DOMO_RESPAWN_O);
                break;
            // Begin Ragnaros Event movement sequence
            case POINT_DOMO_SUMMON1:
                me->GetMotionMaster()->Clear();
                me->GetMotionMaster()->MovePoint(POINT_DOMO_SUMMON2, POINT_DOMO_SUMMON2_X, POINT_DOMO_SUMMON2_Y, POINT_DOMO_SUMMON2_Z);
                break;
            // SetOrientation etc. move the creature back to the previous point, so we're turning him this way:
            case POINT_DOMO_SUMMON2:
                me->GetMotionMaster()->Clear();
                me->GetMotionMaster()->MovePoint(POINT_DOMO_SUMMON3, POINT_DOMO_SUMMON2_X + 1, POINT_DOMO_SUMMON2_Y - 1, POINT_DOMO_SUMMON2_Z);
                break;
            // Ragnaros Event final position
            case POINT_DOMO_SUMMON3:
                me->GetMotionMaster()->Clear();
                me->GetMotionMaster()->MoveIdle();
                break;
            default:
                break;
        }
    }

    // Ragnaros Event sequence
    void DomoEvent()
    {
        switch (DialogueRagnaros_M)
        {
            case 6:
                me->GetMotionMaster()->MovePoint(POINT_DOMO_SUMMON1, POINT_DOMO_SUMMON1_X, POINT_DOMO_SUMMON1_Y, POINT_DOMO_SUMMON1_Z);
                me->SummonGameObject(OBJECT_DOMO_LAVA_STEAM, 838.951f, -830.383f, -230.206f, 0.837757f, QuaternionData(0.0f, 0.0f, 0.406736f, 0.913546f), 0s);
                me->SummonGameObject(OBJECT_DOMO_LAVA_SPLASH, 839.279f, -831.058f, -230.202f, 4.90438f, QuaternionData(0.0f, 0.0f, -0.636078f, 0.771625f), 0s);
                me->CastSpell(me, SPELL_DOMO_SUMMON_RAGNAROS, false);
                ClassicScriptText(SAY_DOMO_MAJ, me);
                break;
            case 15:
                me->SetFacingTo(5.231960f);
                break;
            case 21:
                ClassicScriptText(SAY_DOMO_SUMMON_MAJ, me);
                break;
            case 28:
            {
                // World of Warcraft Client Patch 1.4.0 (2005-04-19)
                // - Ragnaros now stays up 2 hours rather than 1 after being summoned.
                // Instance.MoltenCoreSkipRunes already put Ragnaros in his lair: no second one
                if (Creature* pExisting = me->FindNearestCreature(CLASSIC_MC_NPC_RAGNAROS, 200.0f, true))
                    me->SetFacingToObject(pExisting);
                else if (Creature* pRagnaros = me->SummonCreature(CLASSIC_MC_NPC_RAGNAROS, 838.308f, -831.466f, -232.185f, 2.19911f, TEMPSUMMON_TIMED_OR_DEAD_DESPAWN, 2h))
                    me->SetFacingToObject(pRagnaros);
                break;
            }
            case 36:
                if (Creature* pRagnaros = me->FindNearestCreature(CLASSIC_MC_NPC_RAGNAROS, 100.0f, true))
                {
                    ClassicScriptText(SAY_DOMO_ARRIVAL1_RAG, pRagnaros);
                    pRagnaros->HandleEmoteCommand(EMOTE_ONESHOT_ROAR);
                }
                break;
            case 50:
                ClassicScriptText(SAY_DOMO_ARRIVAL2_MAJ, me);
                break;
            case 60:
                if (Creature* pRagnaros = me->FindNearestCreature(CLASSIC_MC_NPC_RAGNAROS, 100.0f, true))
                {
                    pRagnaros->SetTarget(me->GetGUID());
                    ClassicScriptText(SAY_DOMO_ARRIVAL3_RAG, pRagnaros);
                    pRagnaros->HandleEmoteCommand(EMOTE_ONESHOT_ROAR);
                }
                break;
            case 76:
                m_bAllowDeath = true;
                if (Creature* pRagnaros = me->FindNearestCreature(CLASSIC_MC_NPC_RAGNAROS, 100.0f, true))
                    pRagnaros->CastSpell(me, SPELL_DOMO_ELEMENTAL_FIRE, false);
                // Handle rest in Ragnaros script
                break;
            default:
                break;
        }
    }

    // VMaNGOS instance OnCreatureEnterCombat: an add pulled on its own drags Majordomo and the other adds in
    void LinkAddsCombat()
    {
        if (me->IsEngaged() || IsFriendly() || me->IsImmuneToPC())
            return;

        Unit* victim = nullptr;
        ForEachAliveAdd([&victim](Creature* pAdd)
        {
            if (!victim && pAdd->IsEngaged())
                victim = pAdd->GetVictim();
        });

        if (!victim)
            return;

        me->EngageWithTarget(victim);
        ForEachAliveAdd([victim](Creature* pAdd)
        {
            if (!pAdd->IsEngaged())
                pAdd->EngageWithTarget(victim);
        });
    }

    // Update handler
    void UpdateAI(uint32 diff) override
    {
        // If respawning, resummon adds
        if (!me->IsImmuneToPC() && !IsFriendly() && !AddSpawn)
        {
            for (int i = 0; i < 8; i++)
            {
                if (!m_addSpawns[i].IsEmpty())
                {
                    if (Creature* pOldAdd = me->GetMap()->GetCreature(m_addSpawns[i]))
                    {
                        pOldAdd->DespawnOrUnsummon();
                        m_addSpawns[i].Clear();
                    }
                }

                ClassicMcDomoSpawnLocation const& loc = ClassicMcDomoAddSpawns[i];
                if (Creature* pNewAdd = me->SummonCreature(loc.m_uiEntry, loc.m_fX, loc.m_fY, loc.m_fZ, loc.m_fO, TEMPSUMMON_DEAD_DESPAWN))
                    m_addSpawns[i] = pNewAdd->GetGUID();
            }

            AddSpawn = true;
        }

        LinkAddsCombat();

        // Dialogue: "Impossible! Stay your attack, mortals..."
        if (DialogueDefeatStart0)
        {
            DialogueDefeatTimer += diff;
            if (DialogueDefeatTimer > 2400)
            {
                DialogueDefeatStart0 = false;
                DialogueDefeatStart1 = true;
                DialogueDefeatTimer = 0;

                me->SetFaction(CLASSIC_MC_FACTION_DOMO_FRIENDLY);
                // VMaNGOS: UNIT_FIELD_FLAGS = IMMUNE_TO_PLAYER | IN_COMBAT (drops the PET_RENAME marker)
                m_bDefeatMarker = false;
                me->SetImmuneToPC(true);
                ClassicScriptText(SAY_DOMO_DEFEAT1, me);
            }
        }

        // Dialogue: "Brashly, you have come..."
        if (DialogueDefeatStart1)
        {
            DialogueDefeatTimer += diff;
            // VMaNGOS drops the IN_COMBAT unit flag after 3.6s here (flags = IMMUNE_TO_PLAYER); nothing to do in TC
            if (DialogueDefeatTimer > 7700)
            {
                DialogueDefeatStart1 = false;
                DialogueDefeatStart2 = true;
                DialogueDefeatTimer = 0;

                ClassicScriptText(SAY_DOMO_DEFEAT2, me);
            }
        }

        // Dialogue: "I go now to summon..."
        if (DialogueDefeatStart2)
        {
            DialogueDefeatTimer += diff;
            if (DialogueDefeatTimer > 8600)
            {
                DialogueDefeatStart2 = false;
                DialogueDefeatStart3 = true;
                DialogueDefeatTimer = 0;

                ClassicScriptText(SAY_DOMO_DEFEAT3, me);
            }
        }

        // Teleport to Ragnaros after dialogue
        if (DialogueDefeatStart3)
        {
            if (me->GetDistance2d(758.089f, -1176.71f) < 2.0f && IsFriendly())
            {
                DialogueDefeatTimer += diff;
                if (DialogueDefeatTimer > 17600)
                {
                    DialogueDefeatStart3 = false;
                    DialogueTeleportStart = true;
                    DialogueDefeatTimer = 0;

                    DoCastSelf(SPELL_DOMO_VISUAL_TELEPORT, true); // VMaNGOS: CF_FORCE_CAST
                }
            }
        }

        // Majordomo Teleport sequence start
        if (DialogueTeleportStart)
        {
            DialogueDefeatTimer += diff;
            if (DialogueDefeatTimer > 1510)
            {
                DialogueTeleportStart = false;
                DialogueTeleportFinished = true;
                DialogueDefeatTimer = 0;

                DoCastSelf(SPELL_DOMO_MAJORDOMO_TELEPORT, true); // VMaNGOS: CF_FORCE_CAST
            }
        }

        // Majordomo Teleport sequence end
        if (DialogueTeleportFinished)
        {
            DialogueDefeatTimer += diff;
            if (DialogueDefeatTimer > 100)
            {
                DialogueTeleportFinished = false;
                DialogueDefeatTimer = 0;

                me->ReplaceAllNpcFlags(UNIT_NPC_FLAG_GOSSIP);
            }
        }

        // Start the Ragnaros summoning event
        if (RagnarosEventStart && IsFriendly())
        {
            bool up = false;
            DialogueRagnarosTimer += diff;
            if (DialogueRagnarosTimer > 1000)
            {
                DialogueRagnaros_M++;
                DialogueRagnarosTimer = 0;
                up = true;
            }
            if (up)
                DomoEvent();
        }

        // Raid wipe handler
        if (!IsFriendly() && !UpdateVictim())
            return;

        // Cast Aegis Of Ragnaros if less than 50% hp
        if (me->GetHealthPct() < 50.0f)
            DoCastSelf(SPELL_DOMO_AEGIS_OF_RAGNAROS);

        // Reflect spell timer
        if (Reflection_Timer < diff)
        {
            uint32 const reflect = urand(0, 1) ? SPELL_DOMO_MAGIC_REFLECTION : SPELL_DOMO_DAMAGE_SHIELD;
            ForEachAliveAdd([this, reflect](Creature* pAdd)
            {
                DoCast(pAdd, reflect);
            });
            Reflection_Timer = 30000;
        }
        else
            Reflection_Timer -= diff;

        // Teleport spell timer
        if (TPDomo_Timer < diff)
        {
            Unit* uTarget = me->GetVictim();
            if (urand(0, 1) == 0)
            {
                uTarget = SelectTarget(SelectTargetMethod::Random, 1);
                if (uTarget && uTarget->GetTypeId() == TYPEID_PLAYER && DoCast(uTarget, SPELL_DOMO_TELEPORT_RANDOM) == SPELL_CAST_OK)
                {
                    ResetThreatList();
                    TPDomo_Timer = 20000 + urand(0, 9999);
                }
            }
            else
            {
                if (uTarget && uTarget->GetTypeId() == TYPEID_PLAYER && DoCast(uTarget, SPELL_DOMO_TELEPORT_TARGET) == SPELL_CAST_OK)
                {
                    ResetThreatList();
                    TPDomo_Timer = 20000 + urand(0, 9999);
                }
            }
        }
        else
            TPDomo_Timer -= diff;

        // melee: TC master auto-melee
    }

    // VMaNGOS: gossip_scripts 4108 sends a script event (OnScriptEventHappened) that starts the Ragnaros sequence
    // TODO(classic): assumes the imported gossip_menu_option keeps VMaNGOS menu 4108 / option 0.
    bool OnGossipSelect(Player* player, uint32 menuId, uint32 gossipListId) override
    {
        if (menuId != GOSSIP_MENU_DOMO_SUMMON_RAG || gossipListId != GOSSIP_OPTION_DOMO_SUMMON_RAG)
            return false;

        CloseGossipMenuFor(player);

        RagnarosEventStart = true;
        me->ReplaceAllNpcFlags(UNIT_NPC_FLAG_NONE);
        ClassicScriptText(SAY_DOMO_GOSSIP_START, me, player);
        return true;
    }
};

void AddSC_classic_boss_majordomo_executus()
{
    RegisterCreatureAI(classic_boss_majordomo);
}
