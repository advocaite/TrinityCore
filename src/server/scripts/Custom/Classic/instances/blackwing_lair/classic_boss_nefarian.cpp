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

// Classic 1.60 port of VMaNGOS src/scripts/eastern_kingdoms/burning_steppes/blackwing_lair/boss_nefarian.cpp (ScriptDev2 lineage, GPL-2)
// Scripts: boss_nefarian, npc_corrupted_totem, spell_nefarian_corrupted_totems (23424), spell_nefarian_shadow_flame_passive (22992),
//          spell_nefarian_class_call_warlock (23427), spell_nefarian_class_call_rogue (23414), spell_nefarian_class_call_mage (23410),
//          spell_nefarian_polymorph (23603)

#include "ScriptMgr.h"
#include "InstanceScript.h"
#include "Map.h"
#include "MapReference.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "SpellAuraEffects.h"
#include "SpellInfo.h"
#include "SpellScript.h"
#include "classic_blackwing_lair.h"
#include "classic_script_text.h"
#include <list>
#include <vector>

namespace
{
enum ClassicBwlNefarian : uint32
{
    CLASSIC_BWL_SAY_NEF_AGGRO                = 9973,
    CLASSIC_BWL_SAY_NEF_SHADOWFLAME          = 9974,
    CLASSIC_BWL_SAY_RAISE_SKELETONS          = 9883,
    CLASSIC_BWL_SAY_NEF_SLAY                 = 9972,
    CLASSIC_BWL_SAY_NEF_DEATH                = 9971,

    CLASSIC_BWL_SAY_NEF_MAGE                 = 9850,
    CLASSIC_BWL_SAY_NEF_WARRIOR              = 9855,
    CLASSIC_BWL_SAY_NEF_DRUID                = 9851,
    CLASSIC_BWL_SAY_NEF_PRIEST               = 9848,
    CLASSIC_BWL_SAY_NEF_PALADIN              = 9853,
    CLASSIC_BWL_SAY_NEF_SHAMAN               = 9854,
    CLASSIC_BWL_SAY_NEF_WARLOCK              = 9852,
    CLASSIC_BWL_SAY_NEF_HUNTER               = 9849,
    CLASSIC_BWL_SAY_NEF_ROGUE                = 9856,

    CLASSIC_BWL_SPELL_SHADOWFLAME_PASSIV     = 22992,
    CLASSIC_BWL_SPELL_NEF_SHADOWFLAME        = 22539,
    CLASSIC_BWL_SPELL_BELLOWING_ROAR         = 22686,
    CLASSIC_BWL_SPELL_VEIL_OF_SHADOW         = 22687, // old spell id 7068 -> wrong
    CLASSIC_BWL_SPELL_NEF_CLEAVE             = 20691,
    CLASSIC_BWL_SPELL_TAIL_LASH              = 23364,
    CLASSIC_BWL_SPELL_BONE_CONTRUST          = 23363,
    CLASSIC_BWL_SPELL_RAISE_DRAKONID         = 23362,

    CLASSIC_BWL_SPELL_NEF_MAGE               = 23410, // wild magic
    CLASSIC_BWL_SPELL_NEF_WARRIOR            = 23397, // beserk
    CLASSIC_BWL_SPELL_NEF_DRUID              = 23398, // cat form
    CLASSIC_BWL_SPELL_NEF_PRIEST             = 23401, // corrupted healing
    CLASSIC_BWL_SPELL_NEF_PALADIN            = 23418, // syphon blessing
    CLASSIC_BWL_SPELL_NEF_SHAMAN             = 23425, // totems
    CLASSIC_BWL_SPELL_CORRUPTED_TOTEM        = 23424,
    CLASSIC_BWL_SPELL_NEF_WARLOCK            = 23427, // infernals -> should trigger 23426
    CLASSIC_BWL_SPELL_NEF_HUNTER             = 23436, // bow broke
    CLASSIC_BWL_SPELL_NEF_ROGUE              = 23414, // Paralise

    CLASSIC_BWL_SPELL_NEF_POLYMORPH          = 23603,
    CLASSIC_BWL_SPELL_NEF_HOVER              = 17131,

    CLASSIC_BWL_SPELL_WINDFURY_TOTEM_PASSIVE = 10612,
    CLASSIC_BWL_SPELL_WINDFURY_TOTEM         = 10610,
    CLASSIC_BWL_NPC_CORRUPTED_INFERNAL       = 14668,
    CLASSIC_BWL_NPC_CORRUPTED_STONESKIN_TOTEM_VI     = 14663,
    CLASSIC_BWL_NPC_CORRUPTED_HEALING_STREAM_TOTEM_V = 14664,
    CLASSIC_BWL_NPC_CORRUPTED_WINDFURY_TOTEM_III     = 14666,
    CLASSIC_BWL_NPC_CORRUPTED_FIRE_NOVA_TOTEM_V      = 14662
};

struct ClassicBwlClassCallInfo
{
    ClassicBwlClassCallInfo(uint8 uiClass, uint32 uiYell) : m_uiClass(uiClass), m_uiYell(uiYell) { }
    uint8 m_uiClass;
    uint32 m_uiYell;
};
}

struct classic_boss_nefarian : public ScriptedAI
{
    explicit classic_boss_nefarian(Creature* creature) : ScriptedAI(creature)
    {
        m_pInstance = creature->GetInstanceScript();
    }

    InstanceScript* m_pInstance;

    uint32 m_uiShadowFlameTimer = 0;
    uint32 m_uiBellowingRoarTimer = 0;
    uint32 m_uiVeilOfShadowTimer = 0;
    uint32 m_uiCleaveTimer = 0;
    uint32 m_uiTailLashTimer = 0;
    uint32 m_uiClassCallTimer = 0;
    bool m_bPhase3 = false;
    uint8 m_uiTransitionStage = 0;
    uint32 m_uiTransitionTimer = 100;
    bool m_bTransitionDone = false;
    bool m_bWarriorStance = false;

    std::vector<ClassicBwlClassCallInfo> m_vPossibleCalls;

    void Reset() override
    {
        m_uiShadowFlameTimer   = urand(18000, 25000);
        m_uiBellowingRoarTimer = urand(25000, 30000);
        m_uiVeilOfShadowTimer  = 15000;
        m_uiCleaveTimer        = urand(7000, 10000);
        m_uiTailLashTimer      = 10000;
        m_uiClassCallTimer     = urand(25000, 35000);
        m_bPhase3              = false;
        m_bTransitionDone      = me->GetMapId() != CLASSIC_BWL_MAP_ID;
        m_bWarriorStance       = false;
        m_uiTransitionStage    = 0;
        m_uiTransitionTimer    = 100;

        m_vPossibleCalls.clear();

        m_vPossibleCalls.emplace_back(CLASS_WARRIOR, CLASSIC_BWL_SAY_NEF_WARRIOR);
        m_vPossibleCalls.emplace_back(CLASS_PALADIN, CLASSIC_BWL_SAY_NEF_PALADIN);
        m_vPossibleCalls.emplace_back(CLASS_HUNTER,  CLASSIC_BWL_SAY_NEF_HUNTER);
        m_vPossibleCalls.emplace_back(CLASS_ROGUE,   CLASSIC_BWL_SAY_NEF_ROGUE);
        m_vPossibleCalls.emplace_back(CLASS_PRIEST,  CLASSIC_BWL_SAY_NEF_PRIEST);
        m_vPossibleCalls.emplace_back(CLASS_SHAMAN,  CLASSIC_BWL_SAY_NEF_SHAMAN);
        m_vPossibleCalls.emplace_back(CLASS_MAGE,    CLASSIC_BWL_SAY_NEF_MAGE);
        m_vPossibleCalls.emplace_back(CLASS_WARLOCK, CLASSIC_BWL_SAY_NEF_WARLOCK);
        m_vPossibleCalls.emplace_back(CLASS_DRUID,   CLASSIC_BWL_SAY_NEF_DRUID);
    }

    void KilledUnit(Unit* pVictim) override
    {
        if (urand(0, 4))
            return;

        ClassicScriptText(CLASSIC_BWL_SAY_NEF_SLAY, me, pVictim);
    }

    void JustDied(Unit* /*killer*/) override
    {
        ClassicScriptText(CLASSIC_BWL_SAY_NEF_DEATH, me);

        if (m_pInstance)
            m_pInstance->SetData(CLASSIC_BWL_TYPE_NEFARIAN, DONE);
    }

    void EnterEvadeMode(EvadeReason why) override
    {
        if (m_pInstance)
        {
            m_pInstance->SetData(CLASSIC_BWL_TYPE_NEFARIAN, FAIL);
            if (Creature* pNefarius = me->GetMap()->GetCreature(m_pInstance->GetGuidData(CLASSIC_BWL_DATA_NEFARIUS_GUID)))
                if (pNefarius->AI())
                    pNefarius->AI()->EnterEvadeMode(why);
        }
        me->DespawnOrUnsummon(); // VMaNGOS DeleteLater()
    }

    void JustSummoned(Creature* pSummoned) override
    {
        CreatureAI::DoZoneInCombat(pSummoned);
    }

    void MovementInform(uint32 uiType, uint32 uiPointId) override
    {
        if (uiType != POINT_MOTION_TYPE)
            return;

        switch (uiPointId)
        {
            case 1:
                // VMaNGOS: MOVE_FORCE_DESTINATION, speed 17
                me->GetMotionMaster()->MovePoint(2, -7495.964f, -1252.402f, 476.795f, false, {}, 17.0f);
                m_uiTransitionTimer = 0;
                break;
            case 2:
                me->GetMotionMaster()->MoveIdle();
                m_uiTransitionTimer = 100;
                break;
            default:
                break;
        }
    }

    bool HandleClassCall(uint8 uiClassCalled)
    {
        if (!uiClassCalled)
            return false;

        bool bClassFound = false;
        for (MapReference const& itr : me->GetMap()->GetPlayers())
        {
            Player* pPlayer = itr.GetSource();
            if (pPlayer &&
                pPlayer->IsAlive() &&
                !pPlayer->IsGameMaster())
            {
                if (pPlayer->GetClass() == uiClassCalled)
                {
                    bClassFound = true;
                    switch (uiClassCalled)
                    {
                        case CLASS_WARRIOR:
                            pPlayer->AddAura(CLASSIC_BWL_SPELL_NEF_WARRIOR, pPlayer);
                            break;
                        case CLASS_PALADIN:
                            pPlayer->CastSpell(pPlayer, CLASSIC_BWL_SPELL_NEF_PALADIN, true);
                            break;
                        case CLASS_HUNTER:
                            pPlayer->CastSpell(pPlayer, CLASSIC_BWL_SPELL_NEF_HUNTER, true);
                            break;
                        case CLASS_ROGUE:
                            pPlayer->CastSpell(pPlayer, CLASSIC_BWL_SPELL_NEF_ROGUE, true);
                            break;
                        case CLASS_PRIEST:
                            pPlayer->AddAura(CLASSIC_BWL_SPELL_NEF_PRIEST, pPlayer);
                            break;
                        case CLASS_SHAMAN:
                            pPlayer->AddAura(CLASSIC_BWL_SPELL_NEF_SHAMAN, pPlayer);
                            break;
                        case CLASS_MAGE:
                            pPlayer->CastSpell(pPlayer, CLASSIC_BWL_SPELL_NEF_MAGE, true);
                            break;
                        case CLASS_WARLOCK:
                            pPlayer->CastSpell(pPlayer, CLASSIC_BWL_SPELL_NEF_WARLOCK, true);
                            break;
                        case CLASS_DRUID:
                            pPlayer->AddAura(CLASSIC_BWL_SPELL_NEF_DRUID, pPlayer);
                            break;
                        default:
                            break;
                    }
                }
            }
        }
        m_bWarriorStance = bClassFound && uiClassCalled == CLASS_WARRIOR;

        return bClassFound;
    }

    void UpdateAI(uint32 uiDiff) override
    {
        if (m_uiTransitionTimer && !m_bTransitionDone)
        {
            if (m_uiTransitionTimer <= uiDiff)
            {
                switch (m_uiTransitionStage)
                {
                    case 0:
                        SetCombatMovement(false);
                        DoZoneInCombat();
                        // VMaNGOS SetFly(true)
                        me->SetCanFly(true);
                        me->SetDisableGravity(true);

                        ClassicScriptText(CLASSIC_BWL_SAY_NEF_AGGRO, me);

                        // VMaNGOS: MovePoint + MonsterMoveWithSpeed(17, MOVE_FORCE_DESTINATION)
                        me->GetMotionMaster()->MovePoint(1, -7449.145f, -1320.647f, 476.795f, false, {}, 17.0f);
                        m_uiTransitionTimer = 0;
                        break;
                    case 1:
                        // VMaNGOS SetFly(false)
                        me->SetCanFly(false);
                        me->SetDisableGravity(false);
                        ClassicScriptText(CLASSIC_BWL_SAY_NEF_SHADOWFLAME, me);
                        me->HandleEmoteCommand(EMOTE_ONESHOT_LAND);
                        m_uiTransitionTimer = 1000;
                        break;
                    case 2:
                    {
                        // Landed: players can fight him now. The VMaNGOS template has none of these flags; a 12.x world DB row can carry
                        // them (an event summon's), and nothing here took them off: every raider's attack on him was refused as an invalid
                        // target and he killed the raid unopposed (bwl_nefarian 1111a: phase one won in all three runs, then 0 damage on him, the
                        // bots' "start_attack" on 11583 refused, the raid wiped to Shadow Flame, Cleave and his swings).
                        uint32 const flags = *me->m_unitData->Flags;
                        if (flags & (UNIT_FLAG_IMMUNE_TO_PC | UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_UNINTERACTIBLE))
                            TC_LOG_INFO("scripts.classic", "BWL Nefarian landed with unit flags 0x{:X}: clearing immune to players / not attackable / "
                                        "not interactible", flags);
                        me->SetImmuneToPC(false);
                        me->SetUninteractible(false);
                        me->RemoveUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
                        me->SetWalk(true);
                        me->RemoveAurasDueToSpell(CLASSIC_BWL_SPELL_NEF_HOVER);
                        if (Unit* pTarget = me->GetVictim())
                        {
                            me->GetMotionMaster()->MoveChase(pTarget);
                            SetCombatMovement(true);
                            me->CastSpell(pTarget, CLASSIC_BWL_SPELL_SHADOWFLAME_PASSIV, true);
                        }
                        m_bTransitionDone = true;
                        break;
                    }
                    default:
                        break;
                }
                ++m_uiTransitionStage;
            }
            else
                m_uiTransitionTimer -= uiDiff;
        }

        if (!UpdateVictim() || !m_bTransitionDone)
            return;

        // ShadowFlame_Timer
        if (m_uiShadowFlameTimer < uiDiff)
        {
            if (DoCastSelf(CLASSIC_BWL_SPELL_NEF_SHADOWFLAME) == SPELL_CAST_OK)
                m_uiShadowFlameTimer = urand(18000, 25000);
        }
        else
            m_uiShadowFlameTimer -= uiDiff;

        // BellowingRoar_Timer
        if (m_uiBellowingRoarTimer < uiDiff)
        {
            if (DoCastSelf(CLASSIC_BWL_SPELL_BELLOWING_ROAR) == SPELL_CAST_OK)
                m_uiBellowingRoarTimer = urand(25000, 30000);
        }
        else
            m_uiBellowingRoarTimer -= uiDiff;

        // VeilOfShadow_Timer
        if (m_uiVeilOfShadowTimer < uiDiff)
        {
            if (DoCastVictim(CLASSIC_BWL_SPELL_VEIL_OF_SHADOW) == SPELL_CAST_OK)
                m_uiVeilOfShadowTimer = urand(10000, 15000);
        }
        else
            m_uiVeilOfShadowTimer -= uiDiff;

        // Cleave_Timer
        if (m_uiCleaveTimer < uiDiff)
        {
            if (DoCastVictim(CLASSIC_BWL_SPELL_NEF_CLEAVE) == SPELL_CAST_OK)
                m_uiCleaveTimer = urand(7000, 10000);
        }
        else
            m_uiCleaveTimer -= uiDiff;

        // TailLash_Timer
        if (m_uiTailLashTimer < uiDiff)
        {
            if (DoCastSelf(CLASSIC_BWL_SPELL_TAIL_LASH) == SPELL_CAST_OK)
                m_uiTailLashTimer = urand(4000, 8000);
        }
        else
            m_uiTailLashTimer -= uiDiff;

        // ClassCall_Timer
        if (m_uiClassCallTimer < uiDiff)
        {
            // Cast a random class call
            // On official it is based on what classes are currently on the hostil list
            // but we can't do that yet so just randomly call one
            // We do it now
            if (!m_vPossibleCalls.empty())
            {
                uint8 uiRandClass = uint8(urand(0, uint32(m_vPossibleCalls.size()) - 1));
                if (HandleClassCall(m_vPossibleCalls[uiRandClass].m_uiClass))
                {
                    ClassicScriptText(m_vPossibleCalls[uiRandClass].m_uiYell, me);
                    m_uiClassCallTimer = urand(25000, 35000);
                }
                else
                    m_vPossibleCalls.erase(m_vPossibleCalls.begin() + uiRandClass);
            }
        }
        else
            m_uiClassCallTimer -= uiDiff;

        // Phase3 begins when we are below X health
        if (!m_bPhase3 && me->GetHealthPct() < 20.0f)
        {
            m_bPhase3 = true;
            ClassicScriptText(CLASSIC_BWL_SAY_RAISE_SKELETONS, me);
            me->CastSpell(me, CLASSIC_BWL_SPELL_RAISE_DRAKONID, true);
        }

        // melee: TC master auto-melee
        // VMaNGOS: after a melee swing, 1 in 5 chance to proc the corrupted Windfury Totem
        if (me->isAttackReady() && me->HasAura(CLASSIC_BWL_SPELL_WINDFURY_TOTEM_PASSIVE) && !me->HasAura(CLASSIC_BWL_SPELL_WINDFURY_TOTEM))
            if (!urand(0, 4))
                me->CastSpell(me, CLASSIC_BWL_SPELL_WINDFURY_TOTEM, true);
    }
};

/*######
## npc_corrupted_totem
######*/

namespace
{
enum ClassicBwlCorruptedTotem : uint32
{
    CLASSIC_BWL_SPELL_ROOT_SELF      = 17507,
    CLASSIC_BWL_SPELL_AVOIDANCE      = 23198,
    CLASSIC_BWL_SPELL_STONESKIN      = 10405,
    CLASSIC_BWL_SPELL_HEALING_STREAM = 10461,
    CLASSIC_BWL_SPELL_FIRE_NOVA      = 11311
};
}

struct classic_npc_corrupted_totem : public ScriptedAI
{
    explicit classic_npc_corrupted_totem(Creature* creature) : ScriptedAI(creature)
    {
        m_uiCreatureEntry = creature->GetEntry();

        uint32 hp = urand(200, 2000);
        creature->SetMaxHealth(hp);
        creature->SetHealth(hp);

        m_bAuraAdded = false;
    }

    uint32 m_uiCreatureEntry;
    uint32 m_uiCheckTimer = 1000;
    bool m_bAuraAdded;

    void Reset() override
    {
        me->AddUnitState(UNIT_STATE_ROOT);

        if (!me->HasAura(CLASSIC_BWL_SPELL_ROOT_SELF))
            me->AddAura(CLASSIC_BWL_SPELL_ROOT_SELF, me);

        // VMaNGOS AddAura(SPELL_AVOIDANCE, ADD_AURA_PERMANENT): Avoidance: not affected by AoE
        if (Aura* aura = me->AddAura(CLASSIC_BWL_SPELL_AVOIDANCE, me))
        {
            aura->SetMaxDuration(-1);
            aura->SetDuration(-1);
        }
        m_uiCheckTimer = 1000;
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        DoZoneInCombat();
    }

    void JustDied(Unit* /*killer*/) override
    {
        switch (m_uiCreatureEntry)
        {
            case CLASSIC_BWL_NPC_CORRUPTED_STONESKIN_TOTEM_VI:
                SetAura(false, CLASSIC_BWL_SPELL_STONESKIN);
                break;
            case CLASSIC_BWL_NPC_CORRUPTED_HEALING_STREAM_TOTEM_V:
                SetAura(false, CLASSIC_BWL_SPELL_HEALING_STREAM);
                break;
            case CLASSIC_BWL_NPC_CORRUPTED_WINDFURY_TOTEM_III:
                SetAura(false, CLASSIC_BWL_SPELL_WINDFURY_TOTEM_PASSIVE);
                break;
            default:
                break;
        }
    }

    void SetAura(bool on, uint32 uiSpellId)
    {
        uint32 const mobsEntries[] =
        {
            CLASSIC_BWL_NPC_NEFARIAN,
            CLASSIC_BWL_NPC_BONE_CONSTRUCT,
            CLASSIC_BWL_NPC_BRONZE_DRAKANOID,
            CLASSIC_BWL_NPC_BLUE_DRAKANOID,
            CLASSIC_BWL_NPC_RED_DRAKANOID,
            CLASSIC_BWL_NPC_GREEN_DRAKANOID,
            CLASSIC_BWL_NPC_BLACK_DRAKANOID,
            CLASSIC_BWL_NPC_CHROMATIC_DRAKANOID
        };

        for (uint32 entry : mobsEntries)
        {
            std::list<Creature*> tmpMobsList;
            me->GetCreatureListWithEntryInGrid(tmpMobsList, entry, 55.0f);
            for (Creature* curr : tmpMobsList)
            {
                if (!curr->IsAlive())
                    continue;

                if (on && me->IsAlive())
                {
                    if (me->IsWithinDistInMap(curr, 40.0f))
                    {
                        if (!curr->HasAura(uiSpellId))
                        {
                            int32 damage = 0;
                            switch (uiSpellId)
                            {
                                case CLASSIC_BWL_SPELL_STONESKIN:
                                    damage = -310;
                                    break; // Stoneskin : base -31
                                case CLASSIC_BWL_SPELL_HEALING_STREAM:
                                    damage = 14000;
                                    break; // Healing Stream : base 14
                                default:
                                    break;
                            }

                            if (damage)
                            {
                                // VMaNGOS CastCustomSpell(curr, uiSpellId, damage, triggered)
                                CastSpellExtraArgs args(true);
                                args.AddSpellBP0(damage);
                                curr->CastSpell(curr, uiSpellId, args);
                            }
                            else
                                curr->AddAura(uiSpellId, curr);
                        }
                    }
                    else
                    {
                        if (curr->HasAura(uiSpellId))
                            curr->RemoveAurasDueToSpell(uiSpellId);
                    }
                }
                else
                    curr->RemoveAurasDueToSpell(uiSpellId);
            }
        }
    }

    void UpdateAI(uint32 uiDiff) override
    {
        if (!me->HasAura(CLASSIC_BWL_SPELL_ROOT_SELF))
            me->AddAura(CLASSIC_BWL_SPELL_ROOT_SELF, me);

        if (!UpdateVictim())
            return;

        uint32 addAuraEntry = 0;
        switch (m_uiCreatureEntry)
        {
            case CLASSIC_BWL_NPC_CORRUPTED_FIRE_NOVA_TOTEM_V:
                if (!m_bAuraAdded)
                {
                    me->AddAura(CLASSIC_BWL_SPELL_FIRE_NOVA, me);  // Fire Nova
                    m_bAuraAdded = true;
                    me->DespawnOrUnsummon(); // VMaNGOS DeleteLater()
                    return;
                }
                break;
            case CLASSIC_BWL_NPC_CORRUPTED_STONESKIN_TOTEM_VI:
                addAuraEntry = CLASSIC_BWL_SPELL_STONESKIN; // Stoneskin -30 dmg really ???
                break;
            case CLASSIC_BWL_NPC_CORRUPTED_HEALING_STREAM_TOTEM_V:
                addAuraEntry = CLASSIC_BWL_SPELL_HEALING_STREAM; // Healing Stream +14 hp really ???
                break;
            case CLASSIC_BWL_NPC_CORRUPTED_WINDFURY_TOTEM_III:
                addAuraEntry = CLASSIC_BWL_SPELL_WINDFURY_TOTEM_PASSIVE;
                break;
            default:
                break;
        }
        if (!addAuraEntry)
            return;

        if (m_uiCheckTimer < uiDiff)
        {
            m_uiCheckTimer = 1000;
            SetAura(true, addAuraEntry);
        }
        else
            m_uiCheckTimer -= uiDiff;
    }
};

// 23424 - Nefarian Class Call Shaman Corrupted Totems
class classic_spell_nefarian_corrupted_totems : public SpellScript
{
    void HandleEffect(SpellEffIndex /*effIndex*/)
    {
        if (Unit* caster = GetCaster())
        {
            uint32 const summonSpells[] = { 23419, 23420, 23422, 23423 };
            caster->CastSpell(caster, summonSpells[urand(0, 3)], true);
        }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(classic_spell_nefarian_corrupted_totems::HandleEffect, EFFECT_0, SPELL_EFFECT_ANY);
    }
};

// 22992 - Shadow Flame
// When Nefarian lands at the start of Phase 2 of his encounter he will use an AoE Shadowflame.
// This spell does about 1000 initial shadow damage, but applies a deadly DoT if an Onyxia Scale Cloak is not equipped.
// Players can avoid getting hit by hiding behind Nefarians throne.
class classic_spell_nefarian_shadow_flame_passive : public SpellScript
{
    static constexpr uint32 SPELL_SHADOWFLAME_TRIGGER = 22986;

    void HandleEffect(SpellEffIndex /*effIndex*/)
    {
        if (Unit* target = GetHitUnit())
            GetCaster()->CastSpell(target, SPELL_SHADOWFLAME_TRIGGER, true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(classic_spell_nefarian_shadow_flame_passive::HandleEffect, EFFECT_0, SPELL_EFFECT_ANY);
    }
};

// 23427 - Nefarian Class Call Warlock
// Each Warlock will summon 2 hostile Corrupted Infernals.
// They will stun and do damage to the Warlocks and anyone near them.
class classic_spell_nefarian_class_call_warlock : public SpellScript
{
    static constexpr uint32 SPELL_SUMMON_INFERNALS = 23426;

    void HandleEffect(SpellEffIndex /*effIndex*/)
    {
        Unit* target = GetHitUnit();
        if (!target)
            return;
        // Only the warlock the call was cast on summons. Nefarian has every warlock cast this on itself, and the spell hits everyone
        // around the caster: each raider beside a warlock summoned two more, 25-254 infernals per call instead of two per warlock
        // (bwl_nefarian 1111d: 254 and 215 at the warlock call, 47 deaths to them, both runs wiped with Nefarian at 81% and 92%).
        if (target != GetCaster() || target->GetClass() != CLASS_WARLOCK)
            return;

        // Each Warlock summons 2 Corrupted Infernals
        target->CastSpell(target, SPELL_SUMMON_INFERNALS, true);
        target->CastSpell(target, SPELL_SUMMON_INFERNALS, true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(classic_spell_nefarian_class_call_warlock::HandleEffect, EFFECT_0, SPELL_EFFECT_ANY);
    }
};

// 23414 - Nefarian Class Call Rogue
// Paralyze and teleport player to random position near Nefarian
class classic_spell_nefarian_class_call_rogue : public SpellScript
{
    void HandleEffect(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        Player* player = GetHitPlayer();
        if (!caster || !player)
            return;

        // Teleport player to a random position near caster (Nefarian)
        // Use GetFirstCollisionPosition to avoid teleporting into walls/obstacles
        float angle = frand(0.0f, float(M_PI) * 2);
        Position pos = caster->GetFirstCollisionPosition(5.0f, angle);
        player->NearTeleportTo(pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(), angle - float(M_PI));
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(classic_spell_nefarian_class_call_rogue::HandleEffect, EFFECT_0, SPELL_EFFECT_ANY);
    }
};

// 23410 - Nefarian Class Call Mage - Wild Magic
// Randomly cast Wild Polymorph on raid members, polymorphing them
// TODO(classic): VMaNGOS forces a 5 s periodic timer on apply; here the tick interval comes from the 1.60 client
// aura data (verify 23410 is a periodic aura there, else the class call mage does nothing).
class classic_spell_nefarian_class_call_mage : public AuraScript
{
    void HandlePeriodic(AuraEffect const* /*aurEff*/)
    {
        PreventDefaultAction();

        Player* pMage = GetTarget() ? GetTarget()->ToPlayer() : nullptr;
        if (!pMage)
            return;

        if (!pMage->IsAlive() || pMage->HasAura(CLASSIC_BWL_SPELL_NEF_POLYMORPH))
            return;

        // Use GetMap()->GetPlayers() instead of group to prevent exploit where mage
        // could leave group to avoid casting polymorph on members
        std::vector<Player*> possibleTargets;
        for (MapReference const& itr : pMage->GetMap()->GetPlayers())
        {
            Player* pPlayer = itr.GetSource();
            if (!pPlayer ||
                !pPlayer->IsAlive() ||
                !pPlayer->IsInWorld() ||
                // pPlayer == pMage || // Skip self
                pPlayer->HasAura(CLASSIC_BWL_SPELL_NEF_POLYMORPH) ||
                !pMage->IsWithinDist(pPlayer, 60.0f))
                continue;

            possibleTargets.push_back(pPlayer);
        }

        if (possibleTargets.empty())
            return;

        Player* polymorphTarget = possibleTargets[urand(0, uint32(possibleTargets.size()) - 1)];
        if (polymorphTarget)
        {
            pMage->InterruptNonMeleeSpells(false);
            pMage->CastSpell(polymorphTarget, CLASSIC_BWL_SPELL_NEF_POLYMORPH, false);
        }
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(classic_spell_nefarian_class_call_mage::HandlePeriodic, EFFECT_0, SPELL_AURA_ANY);
    }
};

// 23603 - Nefarian Class Call Mage - Polymorph (Transform Display-ID)
class classic_spell_nefarian_polymorph : public AuraScript
{
    void HandleApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Unit* target = GetTarget();
        if (!target)
            return;

        // Randomly select one of three display IDs for the polymorph transform
        uint32 display_id = 0;
        switch (urand(0, 2))
        {
            case 0:
                display_id = 1060;
                break;
            case 1:
                display_id = 4473;
                break;
            case 2:
                display_id = 7898;
                break;
        }

        if (display_id)
            target->SetDisplayId(display_id); // VMaNGOS also SetTransformScale(1.0f); TC keeps the native scale
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(classic_spell_nefarian_polymorph::HandleApply, EFFECT_1, SPELL_AURA_ANY, AURA_EFFECT_HANDLE_REAL);
    }
};

void AddSC_classic_boss_nefarian()
{
    RegisterCreatureAI(classic_boss_nefarian);
    RegisterCreatureAI(classic_npc_corrupted_totem);
    RegisterSpellScript(classic_spell_nefarian_corrupted_totems);
    RegisterSpellScript(classic_spell_nefarian_shadow_flame_passive);
    RegisterSpellScript(classic_spell_nefarian_class_call_warlock);
    RegisterSpellScript(classic_spell_nefarian_class_call_rogue);
    RegisterSpellScript(classic_spell_nefarian_class_call_mage);
    RegisterSpellScript(classic_spell_nefarian_polymorph);
}
