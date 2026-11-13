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

// Classic 1.60 port of VMaNGOS src/scripts/kalimdor/the_barrens/razorfen_downs/instance_razorfen_downs.cpp (GPL-2)

#include "ScriptMgr.h"
#include "Creature.h"
#include "GameObject.h"
#include "InstanceScript.h"
#include "Map.h"
#include "MotionMaster.h"
#include "Random.h"
#include "TemporarySummon.h"
#include "classic_dungeon_encounter_tracker.h"
#include "classic_razorfen_downs.h"

namespace
{
uint32 const CLASSIC_RFD_MAX_ENCOUNTER = 1;
}

namespace
{
// Classic 1.60: TC boss states for the DungeonEncounter.db2 encounters of Razorfen Downs (map 129), driven by
// ClassicDungeonEncounterTracker (boss kill credit, encounter start / end, instance lock)
enum ClassicRazorfenDownsEncounterBosses : uint32
{
    CLASSIC_RFD_ENC_TUTENKASH = 0,
    CLASSIC_RFD_ENC_PLAGUEMAW = 1,
    CLASSIC_RFD_ENC_MORDRESH = 2,
    CLASSIC_RFD_ENC_RAGGLESNOUT = 3,
    CLASSIC_RFD_ENC_GLUTTON = 4,
    CLASSIC_RFD_ENC_AMNENNAR = 5,
    CLASSIC_RFD_ENC_MAX
};

constexpr ClassicDungeonBossEntry ClassicRazorfenDownsEncounterBossEntries[] =
{
    { CLASSIC_RFD_ENC_TUTENKASH, 7355 }, // Tuten'kash
    { CLASSIC_RFD_ENC_PLAGUEMAW, 7356 }, // Plaguemaw the Rotting
    { CLASSIC_RFD_ENC_MORDRESH, 7357 }, // Mordresh Fire Eye
    { CLASSIC_RFD_ENC_RAGGLESNOUT, 7354 }, // Ragglesnout
    { CLASSIC_RFD_ENC_GLUTTON, 8567 }, // Glutton
    { CLASSIC_RFD_ENC_AMNENNAR, 7358 } // Amnennar the Coldbringer
};
}

class classic_instance_razorfen_downs : public InstanceMapScript
{
public:
    classic_instance_razorfen_downs() : InstanceMapScript(ClassicRFDScriptName, CLASSIC_RFD_MAP_ID) { }

    struct classic_instance_razorfen_downs_InstanceScript : public InstanceScript
    {
        classic_instance_razorfen_downs_InstanceScript(InstanceMap* map) : InstanceScript(map),
            _gongWaves(0), _savedTutenKash(*this, "TutenKash", NOT_STARTED)
        {
            SetHeaders(ClassicRFDDataHeader);
            SetBossNumber(CLASSIC_RFD_ENC_MAX);
            static constexpr DungeonEncounterData encounters[] =
            {
                { CLASSIC_RFD_ENC_TUTENKASH, {{ 2780 }} },
                { CLASSIC_RFD_ENC_PLAGUEMAW, {{ 2781 }} },
                { CLASSIC_RFD_ENC_MORDRESH, {{ 2782 }} },
                { CLASSIC_RFD_ENC_RAGGLESNOUT, {{ 2783 }} },
                { CLASSIC_RFD_ENC_GLUTTON, {{ 2784 }} },
                { CLASSIC_RFD_ENC_AMNENNAR, {{ 2785 }} }
            };
            LoadDungeonEncounterData(encounters);
            for (uint32& encounter : _encounter)
                encounter = NOT_STARTED;
        }

        ClassicDungeonEncounterTracker _encounterTracker{ *this, ClassicRazorfenDownsEncounterBossEntries };

        void OnCreatureCreate(Creature* creature) override
        {
            InstanceScript::OnCreatureCreate(creature);
            _encounterTracker.OnCreatureCreate(creature);
        }

        void OnCreatureRemove(Creature* creature) override
        {
            InstanceScript::OnCreatureRemove(creature);
            _encounterTracker.OnCreatureRemove(creature);
            FinishGongSummon(creature->GetGUID(), !creature->IsAlive());
        }

        void OnUnitDeath(Unit* unit) override
        {
            _encounterTracker.OnUnitDeath(unit);
            FinishGongSummon(unit->GetGUID(), true);
        }

        void Update(uint32 diff) override
        {
            _encounterTracker.Update(diff);
        }

        // VMaNGOS Load(): saved states, IN_PROGRESS -> NOT_STARTED
        void AfterDataLoad() override
        {
            _encounter[0] = _savedTutenKash;
            for (uint32& encounter : _encounter)
                if (encounter == IN_PROGRESS)
                    encounter = NOT_STARTED;
        }

        void OnGameObjectCreate(GameObject* go) override
        {
            InstanceScript::OnGameObjectCreate(go);

            switch (go->GetEntry())
            {
                case GO_GONG:
                    _gongGUID = go->GetGUID();
                    if (GongCompleted() || !_gongSummons.empty())
                        go->SetFlag(GO_FLAG_NOT_SELECTABLE);    // VMaNGOS GO_FLAG_NO_INTERACT (0x10)
                    break;
                case GO_IDOL_CUP_FIRE:
                    if (!_cupFire1GUID.IsEmpty())
                        _cupFire2GUID = go->GetGUID();
                    else
                        _cupFire1GUID = go->GetGUID();
                    break;
                default:
                    break;
            }
        }

        void SetData(uint32 type, uint32 data) override
        {
            if (type == DATA_GONG_WAVES)
            {
                GameObject* gong = instance->GetGameObject(_gongGUID);
                if (!gong || GongCompleted() || !_gongSummons.empty() || data != _gongWaves + 1 || data > 3)
                    return;

                static constexpr uint32 entries[] = { CREATURE_TOMB_FIEND, CREATURE_TOMB_REAVER, CREATURE_TUTEN_KASH };
                static constexpr uint8 counts[] = { 8, 4, 1 };
                gong->SetFlag(GO_FLAG_NOT_SELECTABLE);
                _gongWaveLost = false;
                for (uint8 i = 0; i < counts[_gongWaves]; ++i)
                {
                    bool const east = i % 2 != 0;
                    float x = east ? 2546.33f : 2502.635f;
                    float y = east ? 887.455f : 844.140f;
                    if (i)
                    {
                        x += float(irand(-5, 5));
                        y += float(irand(-5, 5));
                    }
                    if (Creature* summon = gong->SummonCreature(entries[_gongWaves], x, y,
                        east ? 47.69f : 46.896f, 0.633f, TEMPSUMMON_DEAD_DESPAWN))
                    {
                        _gongSummons.insert(summon->GetGUID());
                        summon->SetWalk(false);
                        summon->GetMotionMaster()->MovePoint(0, 2533.479f + float(irand(-5, 5)),
                            870.020f + float(irand(-5, 5)), 47.678f);
                    }
                }
                if (_gongSummons.empty())
                    gong->RemoveFlag(GO_FLAG_NOT_SELECTABLE);
                return;
            }

            if (type == BOSS_TUTEN_KASH)
                _encounter[0] = data;

            if (type == EXTINGUISH_FIRES)
            {
                if (GameObject* cupFire1 = instance->GetGameObject(_cupFire1GUID))
                    cupFire1->SetLootState(GO_JUST_DEACTIVATED);
                if (GameObject* cupFire2 = instance->GetGameObject(_cupFire2GUID))
                    cupFire2->SetLootState(GO_JUST_DEACTIVATED);
            }

            // Persist the completed boss event.
            if (data == DONE)
                _savedTutenKash = _encounter[0];
        }

        uint32 GetData(uint32 type) const override
        {
            switch (type)
            {
                case DATA_GONG_WAVES:
                    return _gongWaves;
                default:
                    break;
            }

            return 0;
        }

        ObjectGuid GetGuidData(uint32 type) const override
        {
            switch (type)
            {
                case DATA_GONG:
                    return _gongGUID;
                default:
                    break;
            }

            return ObjectGuid::Empty;
        }

    private:
        bool GongCompleted() const
        {
            return _encounter[0] == DONE || GetBossState(CLASSIC_RFD_ENC_TUTENKASH) == DONE;
        }

        // The wave's creatures used to count themselves through their stock scripts; those became plain SmartAI
        // (world 2026_10_16_10), so after the first strike the gong never unlocked. The instance tracks them now: a
        // wave killed to the last creature is cleared, a wave with a creature gone alive is struck again.
        void FinishGongSummon(ObjectGuid guid, bool killed)
        {
            if (!_gongSummons.erase(guid))
                return;
            if (!killed)
                _gongWaveLost = true;
            if (!_gongSummons.empty())
                return;
            if (!_gongWaveLost && ++_gongWaves == 3)
            {
                SetData(BOSS_TUTEN_KASH, DONE);
                return;
            }
            if (GameObject* gong = instance->GetGameObject(_gongGUID))
                gong->RemoveFlag(GO_FLAG_NOT_SELECTABLE);
        }

        GuidSet _gongSummons;                          // creatures of the running gong wave
        bool _gongWaveLost = false;
        ObjectGuid _gongGUID;
        ObjectGuid _cupFire1GUID;
        ObjectGuid _cupFire2GUID;
        uint32 _encounter[CLASSIC_RFD_MAX_ENCOUNTER];
        uint8 _gongWaves;                              // gong waves cleared, 3 = Tuten'kash dead
        PersistentInstanceScriptValue<uint32> _savedTutenKash;
    };

    InstanceScript* GetInstanceScript(InstanceMap* map) const override
    {
        return new classic_instance_razorfen_downs_InstanceScript(map);
    }
};

void AddSC_classic_instance_razorfen_downs()
{
    new classic_instance_razorfen_downs();
}
