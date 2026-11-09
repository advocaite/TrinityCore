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

// Classic 1.60 port of VMaNGOS src/scripts/eastern_kingdoms/burning_steppes/blackwing_lair/boss_chromaggus.cpp (ScriptDev2 lineage, GPL-2)
// Scripts: boss_chromaggus

#include "ScriptMgr.h"
#include "GameObject.h"
#include "InstanceScript.h"
#include "Map.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Log.h"
#include "ScriptedCreature.h"
#include "SmartEnum.h"
#include "SpellInfo.h"
#include "classic_blackwing_lair.h"
#include "classic_script_text.h"
#include <vector>

namespace
{
enum ClassicBwlChromaggus : uint32
{
    CLASSIC_BWL_EMOTE_CHROM_FRENZY           = 7797,
    CLASSIC_BWL_EMOTE_SHIMMER                = 9793,

    // These spells are actually called elemental shield
    // What they do is decrease all damage by 75% then they increase
    // One school of damage by 1100%
    CLASSIC_BWL_SPELL_CHROM_FIRE_VULN        = 22277,
    CLASSIC_BWL_SPELL_CHROM_FROST_VULN       = 22278,
    CLASSIC_BWL_SPELL_CHROM_SHADOW_VULN      = 22279,
    CLASSIC_BWL_SPELL_CHROM_NATURE_VULN      = 22280,
    CLASSIC_BWL_SPELL_CHROM_ARCANE_VULN      = 22281,
    CLASSIC_BWL_SPELL_CHROMA_HEAL            = 23168,

    CLASSIC_BWL_MAX_BREATHS                  = 5,
    CLASSIC_BWL_SPELL_INCINERATE             = 23308,   // Incinerate 23308, 23309
    CLASSIC_BWL_SPELL_TIME_LAPSE             = 23310,   // Time lapse 23310, 23311 (old threat mod that was removed in 2.01)
    CLASSIC_BWL_SPELL_CORROSIVE_ACID         = 23313,   // Corrosive Acid 23313, 23314
    CLASSIC_BWL_SPELL_IGNITE_FLESH           = 23315,   // Ignite Flesh 23315, 23316
    CLASSIC_BWL_SPELL_FROST_BURN             = 23187,   // Frost burn 23187, 23189

    // Brood Affliction 23173 - Scripted Spell that cycles through all targets within 100 yards and has a chance to cast one of the afflictions on them
    // Since Scripted spells aren't coded I'll just write a function that does the same thing
    CLASSIC_BWL_SPELL_BROODAF_BLUE           = 23153,   // Blue affliction 23153
    CLASSIC_BWL_SPELL_BROODAF_BLACK          = 23154,   // Black affliction 23154
    CLASSIC_BWL_SPELL_BROODAF_RED            = 23155,   // Red affliction 23155 (23168 on death)
    CLASSIC_BWL_SPELL_BROODAF_BRONZE         = 23170,   // Bronze Affliction 23170
    CLASSIC_BWL_SPELL_BROODAF_GREEN          = 23169,   // Brood Affliction Green 23169

    CLASSIC_BWL_SPELL_CHROMATIC_MUT_1        = 23174,   // Spell cast on player if they get all 5 debuffs

    CLASSIC_BWL_SPELL_CHROM_FRENZY           = 23128,   // 28371 The frenzy spell may be wrong
    CLASSIC_BWL_SPELL_CHROM_ENRAGE           = 28747,

    CLASSIC_BWL_SPELL_CHROMATIC_MUTATION_ONE = 23175,
    CLASSIC_BWL_SPELL_CHROMATIC_MUTATION_TWO = 23177,
    CLASSIC_BWL_SPELL_BROOD_AFFLICTION_RED   = 23168
};

uint32 const ClassicBwlPossibleBreaths[CLASSIC_BWL_MAX_BREATHS] =
{
    CLASSIC_BWL_SPELL_INCINERATE,
    CLASSIC_BWL_SPELL_TIME_LAPSE,
    CLASSIC_BWL_SPELL_CORROSIVE_ACID,
    CLASSIC_BWL_SPELL_IGNITE_FLESH,
    CLASSIC_BWL_SPELL_FROST_BURN
};

uint32 const ClassicBwlVulnerabilities[] =
{
    CLASSIC_BWL_SPELL_CHROM_FIRE_VULN,
    CLASSIC_BWL_SPELL_CHROM_FROST_VULN,
    CLASSIC_BWL_SPELL_CHROM_SHADOW_VULN,
    CLASSIC_BWL_SPELL_CHROM_NATURE_VULN,
    CLASSIC_BWL_SPELL_CHROM_ARCANE_VULN
};

uint32 const ClassicBwlAfflictions[] =
{
    CLASSIC_BWL_SPELL_BROODAF_BLUE,
    CLASSIC_BWL_SPELL_BROODAF_BLACK,
    CLASSIC_BWL_SPELL_BROODAF_RED,
    CLASSIC_BWL_SPELL_BROODAF_BRONZE,
    CLASSIC_BWL_SPELL_BROODAF_GREEN
};
}

struct classic_boss_chromaggus : public ScriptedAI
{
    classic_boss_chromaggus(Creature* creature) : ScriptedAI(creature), m_uiBreathOneSpell(0), m_uiBreathTwoSpell(0)
    {
        // Select the 2 different breaths that we are going to use until despawned
        // 5 possiblities for the first breath, 4 for the second, 20 total possiblites

        // select two different numbers between 0..MAX_BREATHS-1
        m_pInstance = creature->GetInstanceScript();
        if (m_pInstance)
        {
            uint32 breaths = m_pInstance->GetData(CLASSIC_BWL_DATA_CHROM_BREATH);
            m_uiBreathOneSpell = ClassicBwlPossibleBreaths[breaths % CLASSIC_BWL_MAX_BREATHS];
            uint32 idx2 = breaths / CLASSIC_BWL_MAX_BREATHS;
            if (idx2 >= (breaths % CLASSIC_BWL_MAX_BREATHS))
                ++idx2;
            m_uiBreathTwoSpell = ClassicBwlPossibleBreaths[idx2 % CLASSIC_BWL_MAX_BREATHS];
        }
        SetChromaggusInactiveFlags(true);
        m_bEngagedOnce = false;
    }

    InstanceScript* m_pInstance;

    uint32 m_uiMovetoLeverTimer = 2000;

    uint32 m_uiBreathOneSpell;
    uint32 m_uiBreathTwoSpell;
    uint32 m_uiCurrentVulnerabilitySpell = 0;

    uint32 m_uiShimmerTimer = 0;
    uint32 m_uiBreathOneTimer = 30000;
    uint32 m_uiBreathTwoTimer = 60000;
    uint32 m_uiAfflictionTimer = 7500;
    uint32 m_uiFrenzyTimer = 15000;
    bool m_bEnraged = false;
    bool m_bEngagedOnce;

    std::vector<ObjectGuid> m_lRedAfflictionPlayerGUID;
    std::vector<ObjectGuid> m_lChromaticPlayerGUID;

    // VMaNGOS: UNIT_FLAG_NOT_SELECTABLE | UNIT_FLAG_SPAWNING (| UNIT_FLAG_IMMUNE_TO_NPC when withNpcImmunity)
    void SetChromaggusInactiveFlags(bool on, bool withNpcImmunity = true)
    {
        me->SetUninteractible(on);
        if (on)
            me->SetUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
        else
            me->RemoveUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
        if (withNpcImmunity)
            me->SetImmuneToNPC(on);
    }

    void Reset() override
    {
        m_uiMovetoLeverTimer = 2000;

        m_uiCurrentVulnerabilitySpell = 0;              // We use this to store our last vulnerability spell so we can remove it later

        m_uiShimmerTimer    = 0;        // Vulnurability is applied at pull. Changes every 20 secs.
        m_uiBreathOneTimer  = 30000;    // First breath happens in 30 secs. Repeats every 60 secs.
        m_uiBreathTwoTimer  = 60000;    // Second breath happens in 60 secs. Repeats every 60 secs.
        m_uiAfflictionTimer = 7500;     // Afflictions are applied every 7.5 secs.
        m_uiFrenzyTimer     = 15000;    // Frenzy happens every 15 secs.

        m_bEnraged          = false;
        m_lRedAfflictionPlayerGUID.clear();

        for (ObjectGuid const& guid : m_lChromaticPlayerGUID)
        {
            if (Player* pTarget = ObjectAccessor::GetPlayer(*me, guid))
            {
                pTarget->RemoveAurasDueToSpell(CLASSIC_BWL_SPELL_CHROMATIC_MUTATION_ONE);
                pTarget->RemoveAurasDueToSpell(CLASSIC_BWL_SPELL_CHROMATIC_MUTATION_TWO);
                pTarget->KillSelf();    // VMaNGOS: DealDamage(self, GetHealth())
            }
        }
        m_lChromaticPlayerGUID.clear();

        if (m_pInstance)
        {
            if (GameObject* pGO = me->GetMap()->GetGameObject(m_pInstance->GetGuidData(CLASSIC_BWL_DATA_DOOR_CHROMAGGUS_SIDE)))
            {
                if (pGO->GetGoState() == GO_STATE_ACTIVE) // Door open
                    SetChromaggusInactiveFlags(false, false);
                else
                    SetChromaggusInactiveFlags(true);
            }
        }
    }

    void MoveInLineOfSight(Unit* pUnit) override
    {
        if (!pUnit || me->GetVictim())
            return;

        if (m_bEngagedOnce && pUnit->GetTypeId() == TYPEID_PLAYER && me->GetDistance2d(pUnit) < 55.0f && me->IsWithinLOSInMap(pUnit)
          && me->IsValidAttackTarget(pUnit) && pUnit->isInAccessiblePlaceFor(me))
            AttackStart(pUnit);
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        if (m_pInstance)
            m_pInstance->SetData(CLASSIC_BWL_TYPE_CHROMAGGUS, IN_PROGRESS);

        SetChromaggusInactiveFlags(false);
        DoZoneInCombat();
    }

    void JustDied(Unit* /*killer*/) override
    {
        if (m_pInstance)
            m_pInstance->SetData(CLASSIC_BWL_TYPE_CHROMAGGUS, DONE);
    }

    // Scenario bwl_chromaggus 2026-10-31 reset twice ("the boss is evading", at 80% and at 100% health) with nothing in the log to
    // say why: every evade now writes its reason, where he stands against his home, and what he still has on his threat list.
    void EnterEvadeMode(EvadeReason why) override
    {
        Unit* victim = me->GetVictim();
        TC_LOG_INFO("classic.bwl.chromaggus", "Chromaggus evades ({}) at {:.0f}% health, at {:.1f},{:.1f},{:.1f}, home {:.1f},{:.1f},{:.1f}, "
            "in combat {}, threat list {}, victim {}, engaged once {}, motion {}",
            EnumUtils::ToConstant(why), me->GetHealthPct(), me->GetPositionX(), me->GetPositionY(), me->GetPositionZ(),
            me->GetHomePosition().GetPositionX(), me->GetHomePosition().GetPositionY(), me->GetHomePosition().GetPositionZ(),
            me->IsInCombat(), me->GetThreatManager().GetThreatListSize(), victim ? victim->GetName() : "none", m_bEngagedOnce,
            uint32(me->GetMotionMaster()->GetCurrentMovementGeneratorType()));
        ScriptedAI::EnterEvadeMode(why);
    }

    void JustReachedHome() override
    {
        if (m_pInstance)
            m_pInstance->SetData(CLASSIC_BWL_TYPE_CHROMAGGUS, FAIL);
    }

    void MovementInform(uint32 uiType, uint32 uiPointId) override
    {
        if (uiType != POINT_MOTION_TYPE)
            return;

        TC_LOG_INFO("classic.bwl.chromaggus", "Chromaggus reached walk point {} at {:.1f},{:.1f},{:.1f}, in combat {}", uiPointId,
            me->GetPositionX(), me->GetPositionY(), me->GetPositionZ(), me->IsInCombat());

        switch (uiPointId)
        {
            case 0:
                // walk to Flamegor's room on first pull of lever
                me->GetMotionMaster()->MovePoint(1, -7379.223f, -1002.1122f, 477.0402f, true, 3.7662f);
                break;
            case 1:
                // didn't find anyone! walk back to home position
                me->GetMotionMaster()->MovePoint(2, -7484.609385f, -1075.678101f, 477.144623f, true, 0.616172f);
                break;
            case 2:
                me->GetMotionMaster()->MoveTargetedHome();
                break;
            default:
                break;
        }
    }

    void UpdateAI(uint32 uiDiff) override
    {
        if (!me->IsInCombat() && !m_bEngagedOnce && m_pInstance)
        {
            if (GameObject* pGO = me->GetMap()->GetGameObject(m_pInstance->GetGuidData(CLASSIC_BWL_DATA_DOOR_CHROMAGGUS_SIDE)))
            {
                if (pGO->GetGoState() == GO_STATE_ACTIVE) // Door open
                {
                    if (m_uiMovetoLeverTimer < uiDiff)
                    {
                        float x = -7484.609385f;
                        float y = -1075.678101f;
                        float z =   477.144623f;
                        float o =     0.616172f;
                        me->SetHomePosition(x, y, z, o);
                        me->SetWalk(true);
                        me->GetMotionMaster()->MovePoint(0, x, y, z); // VMaNGOS MOVE_PATHFINDING
                        SetChromaggusInactiveFlags(false);
                        m_bEngagedOnce = true;
                    }
                    else
                        m_uiMovetoLeverTimer -= uiDiff;
                }
                else if (!me->IsUninteractible())
                    SetChromaggusInactiveFlags(true);
            }
        }

        if (!UpdateVictim())
            return;

        // Shimmer Timer
        if (m_uiShimmerTimer < uiDiff)
        {
            // Remove old vulnerability spell
            if (m_uiCurrentVulnerabilitySpell)
                me->RemoveAurasDueToSpell(m_uiCurrentVulnerabilitySpell);

            // Cast new random vurlnabilty on self
            uint32 uiSpell = ClassicBwlVulnerabilities[urand(0, 4)];

            if (DoCastSelf(uiSpell) == SPELL_CAST_OK)
            {
                m_uiCurrentVulnerabilitySpell = uiSpell;

                ClassicScriptText(CLASSIC_BWL_EMOTE_SHIMMER, me);
                m_uiShimmerTimer = 20000;
            }
        }
        else
            m_uiShimmerTimer -= uiDiff;

        // Breath One Timer
        if (m_uiBreathOneTimer < uiDiff)
        {
            if (DoCastSelf(m_uiBreathOneSpell) == SPELL_CAST_OK)
                m_uiBreathOneTimer = 60000;
        }
        else
            m_uiBreathOneTimer -= uiDiff;

        // Breath Two Timer
        if (m_uiBreathTwoTimer < uiDiff)
        {
            if (DoCastSelf(m_uiBreathTwoSpell) == SPELL_CAST_OK)
                m_uiBreathTwoTimer = 60000;
        }
        else
            m_uiBreathTwoTimer -= uiDiff;

        // Affliction Timer
        if (m_uiAfflictionTimer < uiDiff)
        {
            uint32 uiSpellAfflict = ClassicBwlAfflictions[urand(0, 4)];

            if (uiSpellAfflict == CLASSIC_BWL_SPELL_BROODAF_RED)
                m_lRedAfflictionPlayerGUID.clear();

            for (uint32 i = 0; i < urand(11, 15); ++i) // Affliction is applied 11-15 times per cast. Creatures such as pets can be targetted
            {
                if (Unit* afflictionTarget = SelectTarget(SelectTargetMethod::Random, 0))
                {
                    if (afflictionTarget->HasAura(CLASSIC_BWL_SPELL_CHROMATIC_MUT_1)) // To make sure mutated players are not being targeted with affliction.
                        continue;

                    // Cast affliction (VMaNGOS: CF_TRIGGERED)
                    if (DoCast(afflictionTarget, uiSpellAfflict, true) == SPELL_CAST_OK)
                    {
                        if (uiSpellAfflict == CLASSIC_BWL_SPELL_BROODAF_RED && afflictionTarget->GetTypeId() == TYPEID_PLAYER)
                            m_lRedAfflictionPlayerGUID.push_back(afflictionTarget->GetGUID());
                    }
                    // Chromatic mutation if target is effected by all afflictions
                    if (afflictionTarget->HasAura(CLASSIC_BWL_SPELL_BROODAF_BLUE)
                            && afflictionTarget->HasAura(CLASSIC_BWL_SPELL_BROODAF_BLACK)
                            && afflictionTarget->HasAura(CLASSIC_BWL_SPELL_BROODAF_RED)
                            && afflictionTarget->HasAura(CLASSIC_BWL_SPELL_BROODAF_BRONZE)
                            && afflictionTarget->HasAura(CLASSIC_BWL_SPELL_BROODAF_GREEN))
                    {
                        afflictionTarget->RemoveAurasDueToSpell(CLASSIC_BWL_SPELL_BROODAF_BLUE);
                        afflictionTarget->RemoveAurasDueToSpell(CLASSIC_BWL_SPELL_BROODAF_BLACK);
                        afflictionTarget->RemoveAurasDueToSpell(CLASSIC_BWL_SPELL_BROODAF_RED);
                        afflictionTarget->RemoveAurasDueToSpell(CLASSIC_BWL_SPELL_BROODAF_BRONZE);
                        afflictionTarget->RemoveAurasDueToSpell(CLASSIC_BWL_SPELL_BROODAF_GREEN);

                        if (afflictionTarget->GetTypeId() == TYPEID_PLAYER) // Only players are mutated
                        {
                            me->AddAura(CLASSIC_BWL_SPELL_CHROMATIC_MUT_1, afflictionTarget);                                       // Main MC aura (caster: Chromaggus)
                            afflictionTarget->AddAura(CLASSIC_BWL_SPELL_CHROMATIC_MUTATION_ONE, afflictionTarget);                  // Mod DMG 500% + Mod Haste Melee 100 + Mod Haste Spell 300
                            afflictionTarget->AddAura(CLASSIC_BWL_SPELL_CHROMATIC_MUTATION_TWO, afflictionTarget);                  // Max Health 10000 + Mod healing 1000%
                            m_lChromaticPlayerGUID.push_back(afflictionTarget->GetGUID());
                        }
                        else    // Pets die instantly
                            afflictionTarget->KillSelf();   // VMaNGOS: DealDamage(self, GetHealth())
                    }
                }
            }
            m_uiAfflictionTimer = 7500;
        }
        else
            m_uiAfflictionTimer -= uiDiff;

        // If player dies while he had the aura SPELL_BROODAF_RED
        for (auto itr = m_lRedAfflictionPlayerGUID.begin(); itr != m_lRedAfflictionPlayerGUID.end();)
        {
            Player* pTarget = ObjectAccessor::GetPlayer(*me, *itr);
            if (pTarget && pTarget->IsAlive() && !pTarget->HasAura(CLASSIC_BWL_SPELL_BROODAF_RED))
            {
                itr = m_lRedAfflictionPlayerGUID.erase(itr);
                continue;
            }

            if (!pTarget || pTarget->isDead())
            {
                if (DoCastSelf(CLASSIC_BWL_SPELL_CHROMA_HEAL) == SPELL_CAST_OK) // Heal 150000 HP
                    m_lRedAfflictionPlayerGUID.erase(itr);
                break;
            }
            ++itr;
        }

        // If player dies while he had the aura SPELL_CHROMATIC_MUT_1
        for (auto itr = m_lChromaticPlayerGUID.begin(); itr != m_lChromaticPlayerGUID.end();)
        {
            if (Player* pTarget = ObjectAccessor::GetPlayer(*me, *itr))
            {
                if (pTarget->isDead())
                {
                    pTarget->RemoveAurasDueToSpell(CLASSIC_BWL_SPELL_CHROMATIC_MUTATION_ONE);
                    pTarget->RemoveAurasDueToSpell(CLASSIC_BWL_SPELL_CHROMATIC_MUTATION_TWO);
                    if (DoCastSelf(CLASSIC_BWL_SPELL_BROOD_AFFLICTION_RED) == SPELL_CAST_OK) // Heal 150000 HP
                        itr = m_lChromaticPlayerGUID.erase(itr);
                    break;
                }
            }
            ++itr;
        }

        // Frenzy Timer
        if (m_uiFrenzyTimer < uiDiff)
        {
            if (DoCastSelf(CLASSIC_BWL_SPELL_CHROM_FRENZY) == SPELL_CAST_OK)
            {
                ClassicScriptText(CLASSIC_BWL_EMOTE_CHROM_FRENZY, me);
                m_uiFrenzyTimer = 15000;
            }
        }
        else
            m_uiFrenzyTimer -= uiDiff;

        // Enrage if not already enraged and below 20%
        if (!m_bEnraged && me->GetHealthPct() < 20.0f)
        {
            if (DoCastSelf(CLASSIC_BWL_SPELL_CHROM_ENRAGE) == SPELL_CAST_OK)
                m_bEnraged = true;
        }

        // melee: TC master auto-melee
    }
};

void AddSC_classic_boss_chromaggus()
{
    RegisterCreatureAI(classic_boss_chromaggus);
}
