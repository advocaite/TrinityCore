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

// Classic 1.60 port of VMaNGOS src/scripts/kalimdor/silithus/temple_of_ahnqiraj/instance_temple_of_ahnqiraj.cpp (ScriptDev2 lineage, GPL-2)
// Scripts: instance_temple_of_ahnqiraj, at_temple_ahnqiraj, mob_qiraji_mindslayer, spell_aq40_drain_mana

#include "ScriptMgr.h"
#include "Containers.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "CreatureData.h"
#include "DB2Stores.h"
#include "GameObject.h"
#include "InstanceScript.h"
#include "Map.h"
#include "MapReference.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "SpellInfo.h"
#include "SpellScript.h"
#include "TemporarySummon.h"
#include "classic_boss_pack.h"
#include "classic_script_text.h"
#include "classic_temple_of_ahnqiraj.h"
#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <unordered_map>
#include <utility>
#include <vector>

ClassicTempleOfAhnQirajInstanceScript* GetClassicTempleOfAhnQirajInstance(WorldObject const* obj)
{
    if (!obj)
        return nullptr;
    return dynamic_cast<ClassicTempleOfAhnQirajInstanceScript*>(obj->GetInstanceScript());
}

namespace
{
constexpr uint32 CTHUN_WHISPER_MUTE_DURATION = 60000 * 10;
constexpr uint32 CTHUN_FIRST_WHISPER         = 90000;
constexpr uint32 CTHUN_WHISPER_FREQ_MIN      = 30000;
constexpr uint32 CTHUN_WHISPER_FREQ_MAX      = 60000;

enum ClassicAQ40TwinsDeathTexts : int32
{
    SAY_VEKLOR_DEATH    = 11452, // my brother...nO!
    SAY_VEKNILASH_DEATH = 11454  // Vek'loor, i feel your pain!
};

enum ClassicAQ40TwinsDialogueEntries : int32
{
    EMOTE_EYE_INTRO         = 11700,
    EVENT_EYE_TURN_AROUND   = 1,
    EVENT_EMPERORS_RISE     = 2,
    SAY_EMPERORS_INTRO_1    = 11702, // Only flesh and bone. ..
    SAY_EMPERORS_INTRO_2    = 11706, // Where are your manners...
    SAY_EMPERORS_INTRO_3    = 11707, // There will be pain...
    SAY_EMPERORS_INTRO_4    = 11708, // Oh so much pain...
    SAY_EMPERORS_INTRO_5    = 11709, // Come, little ones...
    SAY_EMPERORS_INTRO_6    = 11710, // The feast of souls...
};

// VMaNGOS instance_temple_of_ahnqiraj::eStomachSpells
enum ClassicAQ40StomachSpells : uint32
{
    SPELL_PUNT_UPWARD               = 26224, // knocks up like craaayy everyone in range, maybe too big radius. Only works if thing is targeting player?
    SPELL_EXIT_STOMACH_KNOCKBACK    = 26230, // knocks well back, but must be cast probably an invisible trigger, if not by cthun outside
    SPELL_DIGESTIVE_ACID            = 26476, // Must be stacked and removed manually.
    EXIT_KNOCKBACK_CREATURE         = 15800, // Exit trigger creature used for stomach and cthun knockups/knockbacks
    PUNT_CREATURE                   = 15922, // Trigger for cthuns belly knockup spell
    SPELL_QUAKE                     = 26093, // used for its visual only with SendSpellGo. It deals damage if cast normally
    SPELL_PORT_OUT_STOMACH          = 26648, // Not yet used, was killing c'thun too. Maybe that's intended => a respawn?
};

// VMaNGOS SIDialogueEntry / DialogueHelper (one side only)
struct ClassicAQ40DialogueEntry
{
    int32 TextEntry;
    uint32 SayerEntry;
    uint32 Timer;
};

ClassicAQ40DialogueEntry const ClassicAQ40TwinsDeathDialogue[] =
{
    { SAY_VEKNILASH_DEATH,  NPC_VEKNILASH, 3000 }, // todo: 3000 is just a guess
    { SAY_VEKLOR_DEATH,     NPC_VEKLOR,    0 },
    { 0, 0, 0 }
};

// Sources:
// https://www.youtube.com/watch?v=anDqSl-_y9Y
// https://www.youtube.com/watch?v=drIsWEJkkHs
// Before start:  The eye should be turned away from the trigger, and emperors should be kneeling.
//           +0:  Trigger reached - emote happens
//           +2:  The eye turns around and the emperors rise up to standing state.
//           +7:  The eye despawns and emperors begins the dialogue
ClassicAQ40DialogueEntry const ClassicAQ40TwinsIntroDialogue[] =
{
    { EMOTE_EYE_INTRO,       NPC_MASTERS_EYE, 2000 }, // Trigger reached, emote happens. last 2 seconds
    { EVENT_EYE_TURN_AROUND, NPC_MASTERS_EYE, 1000 }, // Eye turns around,
    { EVENT_EMPERORS_RISE,   NPC_MASTERS_EYE, 5000 }, // emperors rise up one second later
    { SAY_EMPERORS_INTRO_1,  NPC_VEKLOR,      7000 }, // Eye despawns, first line of dialogue 5 seconds later
    { SAY_EMPERORS_INTRO_2,  NPC_VEKNILASH,   8000 },
    { SAY_EMPERORS_INTRO_3,  NPC_VEKLOR,      3000 },
    { SAY_EMPERORS_INTRO_4,  NPC_VEKNILASH,   3000 },
    { SAY_EMPERORS_INTRO_5,  NPC_VEKLOR,      3000 },
    { SAY_EMPERORS_INTRO_6,  NPC_VEKNILASH,   0 },
    { 0, 0, 0 }
};

// C'Thun whispers: VMaNGOS script_texts -1531033..-1531040 (no broadcast text exists), chat type whisper
struct ClassicAQ40CthunWhisper
{
    char const* Text;
    uint32 Sound;
};

ClassicAQ40CthunWhisper const ClassicAQ40CthunWhispers[] =
{
    { "Death is close...",              8580 }, // -1531033
    { "You are already dead.",          8581 }, // -1531034
    { "Your courage will fail.",        8582 }, // -1531035
    { "Your friends will abandon you.", 8583 }, // -1531036
    { "You will betray your friends.",  8584 }, // -1531037
    { "You will die.",                  8585 }, // -1531038
    { "You are weak.",                  8586 }, // -1531039
    { "Your heart will explode.",       8587 }, // -1531040
};

// DungeonEncounter.db2 ids used by TC for AQ40; only registered when present in the client data
struct ClassicAQ40EncounterId
{
    uint32 BossId;
    uint32 DungeonEncounterId;
};

ClassicAQ40EncounterId const ClassicAQ40EncounterIds[] =
{
    { TYPE_SKERAM,   709 },
    { TYPE_BUG_TRIO, 710 },
    { TYPE_SARTURA,  711 },
    { TYPE_FANKRISS, 712 },
    { TYPE_VISCIDUS, 713 },
    { TYPE_HUHURAN,  714 },
    { TYPE_TWINS,    715 },
    { TYPE_OURO,     716 },
    { TYPE_CTHUN,    717 }
};

// VMaNGOS opens these with DoOpenDoor()/UseDoorOrButton() on DONE and the twins script closes the enter door while
// the fight is in progress; TC door data gives the same result.
DoorData const ClassicAQ40DoorData[] =
{
    { GO_SKERAM_GATE,      TYPE_SKERAM,  EncounterDoorBehavior::OpenWhenDone },
    { GO_TWINS_ENTER_DOOR, TYPE_HUHURAN, EncounterDoorBehavior::OpenWhenDone },
    { GO_TWINS_ENTER_DOOR, TYPE_TWINS,   EncounterDoorBehavior::OpenWhenNotInProgress },
    { GO_TWINS_EXIT_DOOR,  TYPE_TWINS,   EncounterDoorBehavior::OpenWhenDone },
};

bool ClassicAQ40IsBossType(uint32 type)
{
    return type < MAX_ENCOUNTER;
}
}

class classic_instance_temple_of_ahnqiraj : public InstanceMapScript
{
public:
    classic_instance_temple_of_ahnqiraj() : InstanceMapScript(ClassicTempleOfAhnQirajScriptName, CLASSIC_AQ40_MAP_ID) { }

    struct classic_instance_temple_of_ahnqiraj_InstanceScript : public ClassicTempleOfAhnQirajInstanceScript
    {
        // VMaNGOS DialogueHelper
        struct Dialogue
        {
            Dialogue(classic_instance_temple_of_ahnqiraj_InstanceScript* owner, ClassicAQ40DialogueEntry const* entries)
                : Owner(owner), Entries(entries), Current(nullptr), Timer(0) { }

            classic_instance_temple_of_ahnqiraj_InstanceScript* Owner;
            ClassicAQ40DialogueEntry const* Entries;
            ClassicAQ40DialogueEntry const* Current;
            uint32 Timer;

            void StartNextDialogueText(int32 textEntry)
            {
                for (ClassicAQ40DialogueEntry const* entry = Entries; entry->TextEntry; ++entry)
                {
                    if (entry->TextEntry == textEntry)
                    {
                        Current = entry;
                        DoNextDialogueStep();
                        return;
                    }
                }
            }

            void DoNextDialogueStep()
            {
                if (Current && !Current->TextEntry)
                {
                    Timer = 0;
                    return;
                }

                Timer = Current->Timer;

                // Entries below 100 are event ids of the dialogue, not texts (VMaNGOS would try to say them as broadcast texts)
                if (Current->SayerEntry && Current->TextEntry >= 100)
                    if (Creature* speaker = Owner->GetSingleCreatureFromStorage(Current->SayerEntry))
                        ClassicScriptText(uint32(Current->TextEntry), speaker);

                Owner->JustDidDialogueStep(Entries, Current->TextEntry);
                ++Current;
            }

            void DialogueUpdate(uint32 diff)
            {
                if (Timer)
                {
                    if (Timer <= diff)
                        DoNextDialogueStep();
                    else
                        Timer -= diff;
                }
            }
        };

        // VMaNGOS instance_temple_of_ahnqiraj::StomachTimers
        struct StomachTimers
        {
            uint32 acidDebuff;
            uint32 timeSincePortedFromStomach;
            uint32 timeSincePortedToStomach;
            bool didKnockback;
            StomachTimers() :
                acidDebuff(StomachTimers::ACID_REFRESH_RATE),
                timeSincePortedFromStomach(0),
                timeSincePortedToStomach(0),
                didKnockback(false)
            {}
            static uint32 const PUNT_CAST_TIME = 3000;
            static uint32 const ACID_REFRESH_RATE = 5000;
        };
        using CThunStomachList = std::vector<std::pair<ObjectGuid, StomachTimers>>;

        explicit classic_instance_temple_of_ahnqiraj_InstanceScript(InstanceMap* map) : ClassicTempleOfAhnQirajInstanceScript(map),
            m_uiBugTrioDeathCount(0),
            m_twinsIntroDialogue(this, ClassicAQ40TwinsIntroDialogue),
            m_twinsIntroStartedOrDone(false),
            m_twinsDeadDialogue(this, ClassicAQ40TwinsDeathDialogue),
            m_uiCthunWhisperTimer(CTHUN_FIRST_WHISPER),
            m_uiCthunPrevWhisperTimer(CTHUN_FIRST_WHISPER),
            quakeTimer(0),
            puntCountdown(0)
        {
            SetHeaders(ClassicTempleOfAhnQirajDataHeader);
            SetBossNumber(MAX_ENCOUNTER);
            LoadDoorData(ClassicAQ40DoorData);

            std::vector<DungeonEncounterData> encounters;
            for (ClassicAQ40EncounterId const& enc : ClassicAQ40EncounterIds)
                if (sDungeonEncounterStore.LookupEntry(enc.DungeonEncounterId))
                    encounters.push_back({ enc.BossId, {{ enc.DungeonEncounterId }} });
            LoadDungeonEncounterData(encounters);
        }

        uint32 m_uiBugTrioDeathCount;
        GuidList m_lRoyalGuardGUIDList;

        Dialogue m_twinsIntroDialogue;
        bool m_twinsIntroStartedOrDone;
        Dialogue m_twinsDeadDialogue;
        std::vector<ObjectGuid> graspsOfCthun;

        // Ouro server crash handling
        uint32 m_uiRestoreOuroSpawnTriggerTimer = 0;

        std::vector<std::pair<ObjectGuid, uint32>> cthunWhisperMutes;
        uint32 m_uiCthunWhisperTimer;
        uint32 m_uiCthunPrevWhisperTimer;

        ObjectGuid puntCreatureGuid;
        uint32 quakeTimer;
        uint32 puntCountdown;
        CThunStomachList playersInStomach;

        std::unordered_map<uint32, ObjectGuid> m_mNpcEntryGuidStore;
        std::unordered_map<uint32, ObjectGuid> m_mGoEntryGuidStore;

        // ---- ScriptedInstance helpers ----
        Creature* GetSingleCreatureFromStorage(uint32 entry) const override
        {
            auto itr = m_mNpcEntryGuidStore.find(entry);
            if (itr == m_mNpcEntryGuidStore.end())
                return nullptr;
            return instance->GetCreature(itr->second);
        }

        GameObject* GetSingleGameObjectFromStorage(uint32 entry) const override
        {
            auto itr = m_mGoEntryGuidStore.find(entry);
            if (itr == m_mGoEntryGuidStore.end())
                return nullptr;
            return instance->GetGameObject(itr->second);
        }

        Creature* GetCreatureByGuid(ObjectGuid guid) const override { return instance->GetCreature(guid); }
        GameObject* GetGameObjectByGuid(ObjectGuid guid) const override { return instance->GetGameObject(guid); }
        Player* GetPlayerByGuid(ObjectGuid guid) const override { return instance->GetPlayer(guid); }

        Player* GetPlayerInMap(bool onlyAlive, bool canBeGamemaster) const override
        {
            for (MapReference const& ref : instance->GetPlayers())
            {
                Player* player = ref.GetSource();
                if (player && (!onlyAlive || player->IsAlive()) && (canBeGamemaster || !player->IsGameMaster()))
                    return player;
            }
            return nullptr;
        }

        void GetRoyalGuardGUIDList(GuidList& lList) const override { lList = m_lRoyalGuardGUIDList; }

        bool TwinsDialogueStartedOrDone() const override { return m_twinsIntroStartedOrDone; }

        // ---- Encounter state ----
        bool IsEncounterInProgress() const override
        {
            for (uint32 i = 0; i < MAX_ENCOUNTER; ++i)
                if (GetBossState(i) == IN_PROGRESS || GetBossState(i) == SPECIAL)
                    return true;

            return false;
        }

        // VMaNGOS AddObjectToRemoveList(): the creature disappears whenever it (re)spawns
        static void RemoveTrash(Creature* creature)
        {
            creature->DespawnOrUnsummon(0s, Seconds(7 * DAY));
        }

        // VMaNGOS randomizes the C'thun trash packs on create (SetEntry/UpdateEntry). TC picks the entry before
        // the creature is created instead.
        uint32 GetCreatureEntry(ObjectGuid::LowType /*spawnId*/, CreatureData const* data) override
        {
            switch (data->id)
            {
                case NPC_QIRAJI_SLAYER:
                    return urand(0, 1) ? uint32(NPC_QIRAJI_MINDSLAYER) : uint32(NPC_QIRAJI_SLAYER);
                case NPC_QIRAJI_MINDSLAYER:
                    return urand(0, 1) ? uint32(NPC_QIRAJI_SLAYER) : uint32(NPC_QIRAJI_MINDSLAYER);
                default:
                    break;
            }
            return data->id;
        }

        void OnGameObjectCreate(GameObject* pGo) override
        {
            // Door states (VMaNGOS UseDoorOrButton on create when the boss is DONE) come from the door data
            InstanceScript::OnGameObjectCreate(pGo);

            switch (pGo->GetEntry())
            {
                case GO_SKERAM_GATE:
                case GO_TWINS_ENTER_DOOR:
                case GO_TWINS_EXIT_DOOR:
                    break;
                case GO_GRASP_OF_CTHUN:
                    // VMaNGOS hides the grasps (SetVisible(false)) once C'thun is dead; they are despawned in Update()
                    graspsOfCthun.push_back(pGo->GetGUID());
                    break;
                default:
                    return;
            }
            m_mGoEntryGuidStore[pGo->GetEntry()] = pGo->GetGUID();
        }

        // VMaNGOS OnCreatureRespawn (TC calls OnCreatureCreate for every (re)spawned object)
        void OnCreatureRespawn(Creature* pCreature)
        {
            switch (pCreature->GetEntry())
            {
                case NPC_ANUBISATH_SENTINEL:
                case NPC_OBSIDIAN_ERADICATOR:
                    if (GetBossState(TYPE_SKERAM) == DONE)
                        RemoveTrash(pCreature);
                    break;
                case NPC_QIRAJI_BRAINWASHER:
                case NPC_VEKNISS_GUARDIAN:
                case NPC_VEKNISS_WARRIOR:
                case NPC_SARTURA_S_ROYAL_GUARD:
                    if (GetBossState(TYPE_SARTURA) == DONE)
                        RemoveTrash(pCreature);
                    break;
                case NPC_VEKNISS_DRONE:
                case NPC_VEKNISS_SOLDIER:
                    if (GetBossState(TYPE_FANKRISS) == DONE)
                        RemoveTrash(pCreature);
                    break;
                case NPC_VEKNISS_HIVE_CRAWLER:
                case NPC_VEKNISS_WASP:
                case NPC_QIRAJI_LASHER:
                case NPC_VEKNISS_STINGER:
                    if (GetBossState(TYPE_HUHURAN) == DONE)
                        RemoveTrash(pCreature);
                    break;
                case NPC_VEKLOR:
                case NPC_VEKNILASH:
                case NPC_ANUBISATH_DEFENDER:
                case NPC_QIRAJI_SCARAB:
                case NPC_QIRAJI_SCORPION:
                    if (GetBossState(TYPE_TWINS) == DONE)
                        RemoveTrash(pCreature);
                    break;
                case NPC_OURO_SCARAB:
                    if (GetBossState(TYPE_OURO) != IN_PROGRESS)
                        RemoveTrash(pCreature);
                    break;
                case NPC_QIRAJI_MINDSLAYER:
                case NPC_QIRAJI_SLAYER:
                case NPC_QIRAJI_CHAMPION:
                case NPC_ANUBISATH_WARDER:
                case NPC_OBSIDIAN_NULLIFIER:
                    if (GetBossState(TYPE_CTHUN) == DONE)
                        RemoveTrash(pCreature);
                    break;
                case NPC_MASTERS_EYE:
                    // Despawn C'thun eye at twins if twins are already dead
                    if (TwinsDialogueStartedOrDone())
                        RemoveTrash(pCreature);
                    break;
                default:
                    break;
            }
        }

        void OnCreatureCreate(Creature* pCreature) override
        {
            InstanceScript::OnCreatureCreate(pCreature);

            switch (pCreature->GetEntry())
            {
                case NPC_PRINCESS_YAUJ:
                case NPC_VEM:
                case NPC_KRI:
                case NPC_BATTLEGUARD_SARTURA:
                case NPC_VEKLOR:
                case NPC_VEKNILASH:
                case NPC_EYE_OF_C_THUN:
                case NPC_CTHUN:
                case NPC_MASTERS_EYE:
                case NPC_OURO_SPAWNER:
                case NPC_CTHUN_PORTAL:
                    m_mNpcEntryGuidStore[pCreature->GetEntry()] = pCreature->GetGUID();
                    break;
                case NPC_SARTURA_S_ROYAL_GUARD:
                    m_lRoyalGuardGUIDList.push_back(pCreature->GetGUID());
                    break;
                case NPC_CAELESTRASZ:
                    pCreature->SetNpcFlag(UNIT_NPC_FLAG_GOSSIP);
                    break;
                default:
                    break;
            }
            // Delete some creatures
            OnCreatureRespawn(pCreature);
        }

        void SetData(uint32 uiType, uint32 uiData) override
        {
            switch (uiType)
            {
                case TYPE_SKERAM:
                    // Skeram gate opens through the door data
                    SetBossState(uiType, EncounterState(uiData));
                    break;
                case TYPE_BUG_TRIO:
                    if (uiData == SPECIAL)
                    {
                        ++m_uiBugTrioDeathCount;
                        if (m_uiBugTrioDeathCount >= 3)
                            SetData(TYPE_BUG_TRIO, DONE);
                        // don't store any special data
                        break;
                    }
                    if (uiData == IN_PROGRESS)
                        m_uiBugTrioDeathCount = 0;
                    SetBossState(uiType, EncounterState(uiData));
                    break;
                case TYPE_SARTURA:
                {
                    // a Sartura that resets alive brings back the Royal Guards killed in the failed attempt
                    bool const wasFighting = GetBossState(uiType) == IN_PROGRESS;
                    SetBossState(uiType, EncounterState(uiData));
                    if (wasFighting && (uiData == NOT_STARTED || uiData == FAIL))
                        ClassicRespawnBossPack(instance, NPC_BATTLEGUARD_SARTURA, { NPC_SARTURA_S_ROYAL_GUARD });
                    break;
                }
                case TYPE_FANKRISS:
                case TYPE_VISCIDUS:
                    SetBossState(uiType, EncounterState(uiData));
                    break;
                case TYPE_HUHURAN:
                    // Twins enter door opens through the door data
                    SetBossState(uiType, EncounterState(uiData));
                    break;
                case TYPE_TWINS:
                    // Either of the twins can set data, so return to avoid double changing
                    if (GetBossState(uiType) == uiData)
                        return;
                    // Enter door (open unless IN_PROGRESS) and exit door (open on DONE) follow the door data
                    SetBossState(uiType, EncounterState(uiData));
                    if (uiData == DONE)
                        m_twinsDeadDialogue.StartNextDialogueText(SAY_VEKNILASH_DEATH);
                    break;
                case TYPE_OURO:
                    if (uiData == FAIL)
                    {
                        // Respawn the Ouro spawner on fail
                        if (Creature* pSpawner = GetSingleCreatureFromStorage(NPC_OURO_SPAWNER))
                            pSpawner->Respawn();
                    }
                    SetBossState(uiType, EncounterState(uiData));
                    break;
                case TYPE_CTHUN:
                    SetBossState(uiType, EncounterState(uiData));
                    // Grasps of C'thun are hidden in Update() once C'thun is DONE
                    break;
                default:
                    break;
            }
            // VMaNGOS saves on DONE: TC saves boss states itself
        }

        uint32 GetData(uint32 uiType) const override
        {
            if (ClassicAQ40IsBossType(uiType))
                return GetBossState(uiType);
            return 0;
        }

        void AfterDataLoad() override
        {
            // VMaNGOS Load(): IN_PROGRESS -> NOT_STARTED is done by TC
            if (GetBossState(TYPE_TWINS) == DONE)
                m_twinsIntroStartedOrDone = true;

            // Fix Ouro after sever crash / restart
            if (GetBossState(TYPE_OURO) != DONE)
                m_uiRestoreOuroSpawnTriggerTimer = 3000;
        }

        // TODO(classic): VMaNGOS CheckConditionCriteriaMeet(instance_condition_id < MAX_ENCOUNTER -> encounter DONE) is used by
        // VMaNGOS DB conditions (CONDITION_INSTANCE_DATA style); TC has no equivalent hook.

        // ---- Twins intro (VMaNGOS TwinsIntroDialogue) ----
        void StartTwinsIntro()
        {
            m_twinsIntroStartedOrDone = true;

            Creature* pEye = GetSingleCreatureFromStorage(NPC_MASTERS_EYE);
            Creature* pVL = GetSingleCreatureFromStorage(NPC_VEKLOR);
            Creature* pVN = GetSingleCreatureFromStorage(NPC_VEKNILASH);

            // If we're missing one of the creatures needed in this little event, we just skip it.
            if (!pEye || !pVL || !pVN)
            {
                if (pEye)
                    pEye->DespawnOrUnsummon(1s);
                return;
            }
            m_twinsIntroDialogue.StartNextDialogueText(EMOTE_EYE_INTRO);
        }

        void JustDidDialogueStep(ClassicAQ40DialogueEntry const* dialogue, int32 iEntry)
        {
            if (dialogue != ClassicAQ40TwinsIntroDialogue)
                return;

            Creature* pEye = GetSingleCreatureFromStorage(NPC_MASTERS_EYE);
            Creature* pVL = GetSingleCreatureFromStorage(NPC_VEKLOR);
            Creature* pVN = GetSingleCreatureFromStorage(NPC_VEKNILASH);
            // If we at any point are missing one of the creatures we skip to the end and stop.
            if (!pEye || !pVL || !pVN)
                return;

            switch (iEntry)
            {
                case EVENT_EYE_TURN_AROUND:
                    pEye->SetFacingTo(1.57f);
                    break;
                case EVENT_EMPERORS_RISE:
                    pVL->SetStandState(UNIT_STAND_STATE_STAND);
                    pVN->SetStandState(UNIT_STAND_STATE_STAND);
                    break;
                case SAY_EMPERORS_INTRO_1:
                    pEye->DespawnOrUnsummon(1ms); // Will look like a death to gm, but fade out for players
                    break;
                default:
                    break;
            }
        }

        void DoHandleTempleAreaTrigger(uint32 uiTriggerId) override
        {
            if (uiTriggerId == AREATRIGGER_TWIN_EMPERORS && !TwinsDialogueStartedOrDone())
            {
                // Current assumption is the event only start once every soft reset.
                // May need to tweak if-statement to make sure this is the case.
                StartTwinsIntro();
            }
            else if (uiTriggerId == AREATRIGGER_SARTURA)
            {
                if (GetData(TYPE_SARTURA) == NOT_STARTED || GetData(TYPE_SARTURA) == FAIL)
                {
                    if (Creature* pSartura = GetSingleCreatureFromStorage(NPC_BATTLEGUARD_SARTURA))
                        if (pSartura->IsAlive() && pSartura->IsAIEnabled())
                            pSartura->AI()->DoZoneInCombat();
                }
            }
        }

        // ---- C'thun whispers ----
        void UpdateCThunWhisper(uint32 diff)
        {
            uint32 cthunStatus = GetData(TYPE_CTHUN);
            if (cthunStatus == DONE)
                return;

            if (m_uiCthunWhisperTimer >= diff)
            {
                m_uiCthunWhisperTimer -= diff;
                return;
            }
            m_uiCthunWhisperTimer = urand(CTHUN_WHISPER_FREQ_MIN, CTHUN_WHISPER_FREQ_MAX);

            // Updating muted players
            for (auto it = cthunWhisperMutes.begin(); it != cthunWhisperMutes.end(); )
            {
                it->second -= std::min(m_uiCthunPrevWhisperTimer, it->second);

                if (it->second < CTHUN_WHISPER_FREQ_MAX)
                    it = cthunWhisperMutes.erase(it);
                else
                    ++it;
            }
            m_uiCthunPrevWhisperTimer = m_uiCthunWhisperTimer;

            if (cthunStatus == IN_PROGRESS)
                return;

            Creature* pCthun = GetSingleCreatureFromStorage(NPC_CTHUN);
            if (!pCthun)
                return;

            std::vector<Player*> candidates;
            for (MapReference const& ref : instance->GetPlayers())
            {
                if (Player* player = ref.GetSource())
                {
                    if (player->IsAlive())
                    {
                        auto find_it = std::find_if(cthunWhisperMutes.begin(), cthunWhisperMutes.end(),
                            [player](std::pair<ObjectGuid, uint32> const& e) { return e.first == player->GetGUID(); });
                        if (find_it == cthunWhisperMutes.end())
                            candidates.push_back(player);
                    }
                }
            }

            if (candidates.empty())
                return;

            Player* targetPlayer = Trinity::Containers::SelectRandomContainerElement(candidates);

            // ToDo: also cast the C'thun Whispering charm spell - requires additional research
            // VMaNGOS DoScriptText(irand(SAY_CTHUN_WHISPER_8, SAY_CTHUN_WHISPER_1), pCthun, targetPlayer): script_texts, whisper + sound
            ClassicAQ40CthunWhisper const& whisper = ClassicAQ40CthunWhispers[urand(0, uint32(std::size(ClassicAQ40CthunWhispers) - 1))];
            pCthun->Whisper(whisper.Text, LANG_UNIVERSAL, targetPlayer);
            pCthun->PlayDirectSound(whisper.Sound, targetPlayer);

            cthunWhisperMutes.emplace_back(targetPlayer->GetGUID(), CTHUN_WHISPER_MUTE_DURATION);
        }

        void Update(uint32 uiDiff) override
        {
            m_twinsIntroDialogue.DialogueUpdate(uiDiff);
            m_twinsDeadDialogue.DialogueUpdate(uiDiff);

            UpdateCThunWhisper(uiDiff);

            UpdateStomachOfCthun(uiDiff);

            // Fix Ouro after sever crash / restart
            if (m_uiRestoreOuroSpawnTriggerTimer)
            {
                if (m_uiRestoreOuroSpawnTriggerTimer < uiDiff)
                    RestoreOuroSpawnTrigger();
                else
                    m_uiRestoreOuroSpawnTriggerTimer -= uiDiff;
            }

            // VMaNGOS SetData(TYPE_CTHUN) / OnObjectCreate: pGo->SetVisible(uiData != DONE) on every Grasp of C'thun
            if (!graspsOfCthun.empty() && GetBossState(TYPE_CTHUN) == DONE)
            {
                for (ObjectGuid const& guid : graspsOfCthun)
                    if (GameObject* pGo = instance->GetGameObject(guid))
                        pGo->DespawnOrUnsummon(0s, Seconds(7 * DAY));
                graspsOfCthun.clear();
            }
        }

        // ---- C'thun stomach ----
        void AddPlayerToStomach(Unit* p) override
        {
            if (!p)
                return;
            if (Creature* pCthun = GetSingleCreatureFromStorage(NPC_CTHUN))
                pCthun->CastSpell(p, SPELL_DIGESTIVE_ACID, true);
            playersInStomach.emplace_back(p->GetGUID(), StomachTimers());
        }

        CThunStomachList::iterator PlayerInStomachIter(Unit* unit)
        {
            if (!unit)
                return playersInStomach.end();

            return std::find_if(playersInStomach.begin(), playersInStomach.end(),
                [unit](std::pair<ObjectGuid, StomachTimers> const& e) { return unit->GetGUID() == e.first; });
        }

        void TeleportPlayerToCThun(Player* pPlayer)
        {
            // Player is ported to center of c'thun with a small, random, offset to knock the player in a random direction.
            if (AreaTriggerEntry const* cthunAreaTrigger = sAreaTriggerStore.LookupEntry(AREATRIGGER_CTHUN_KNOCKBACK))
            {
                float x = cthunAreaTrigger->Pos.X + std::cos(frand(0.0f, float(M_PI) * 2.f)) * 0.1f;
                float y = cthunAreaTrigger->Pos.Y + std::sin(frand(0.0f, float(M_PI) * 2.f)) * 0.1f;
                pPlayer->NearTeleportTo(x, y, cthunAreaTrigger->Pos.Z, pPlayer->GetOrientation());
            }
            else
            {
                float x = -8578.0f + std::cos(frand(0.0f, float(M_PI) * 2.f)) * 0.1f;
                float y = 1986.8f + std::sin(frand(0.0f, float(M_PI) * 2.f)) * 0.1f;
                pPlayer->NearTeleportTo(x, y, 100.4f, pPlayer->GetOrientation());
            }
        }

        void PerformCthunKnockback()
        {
            float x, y, z;
            if (AreaTriggerEntry const* pAt = sAreaTriggerStore.LookupEntry(AREATRIGGER_CTHUN_KNOCKBACK))
            {
                x = pAt->Pos.X;
                y = pAt->Pos.Y;
                z = pAt->Pos.Z;
            }
            else
            {
                x = -8578.0f;
                y = 1986.8f;
                z = 100.22f;
            }
            if (Creature* kbCreature = instance->SummonCreature(EXIT_KNOCKBACK_CREATURE, Position(x, y, z, 0.0f), nullptr, 1000ms))
                kbCreature->CastSpell(kbCreature, SPELL_EXIT_STOMACH_KNOCKBACK, false);
        }

        bool PlayerInStomach(Unit* unit) override
        {
            if (!unit)
                return false;

            return PlayerInStomachIter(unit) != playersInStomach.end();
        }

        void HandleStomachTriggers(Player* pPlayer, AreaTriggerEntry const* pAt) override
        {
            if (!pPlayer || !pAt)
                return;
            if (pPlayer->IsGameMaster() || !pPlayer->IsAlive())
                return;

            if (pAt->ID == AREATRIGGER_STOMACH_GROUND)
            {
                if (puntCreatureGuid.IsEmpty())
                {
                    if (Creature* pc = instance->SummonCreature(PUNT_CREATURE, Position(pAt->Pos.X, pAt->Pos.Y, pAt->Pos.Z, 0.0f), nullptr, 4000ms))
                    {
                        puntCreatureGuid = pc->GetGUID();
                        quakeTimer = 1000;
                        puntCountdown = StomachTimers::PUNT_CAST_TIME;
                        // Since this is the wrong spell, and it deals damage, we send the visual only, instead of casting it.
                        // TODO(classic): VMaNGOS pc->SendSpellGo(pc, SPELL_QUAKE) (visual only) has no TC equivalent; no quake visual.
                    }
                }
            }
            else if (pAt->ID == AREATRIGGER_STOMACH_AIR)
            {
                TeleportPlayerToCThun(pPlayer);
            }
            else if (pAt->ID == AREATRIGGER_CTHUN_KNOCKBACK)
            {
                // "Disable" the knockback if c'thun is killed
                if (GetData(TYPE_CTHUN) != DONE)
                {
                    PerformCthunKnockback();
                    // If the areatrigger was triggered by a player who was ported from stomach
                    // we find his timer object and set didKnockback to true, to avoid double-knockback
                    // in the UpdateStomachOfCthun function
                    auto it = PlayerInStomachIter(pPlayer);
                    if (it != playersInStomach.end() && !it->second.didKnockback)
                        it->second.didKnockback = true;
                }
            }
        }

        bool KillPlayersInStomach() override
        {
            for (auto iter = playersInStomach.begin(); iter != playersInStomach.end();)
            {
                if (Player* p = instance->GetPlayer(iter->first))
                {
                    // Not killing people with god on, makes debugging easier
                    if (p->GetCommandStatus(CHEAT_GOD))
                    {
                        ++iter;
                        continue;
                    }

                    if (p->IsAlive())
                        Unit::DealDamage(p, p, uint32(p->GetHealth()), nullptr, DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, nullptr, false);
                    if (p->HasAura(SPELL_DIGESTIVE_ACID))
                        p->RemoveAurasDueToSpell(SPELL_DIGESTIVE_ACID);
                }
                iter = playersInStomach.erase(iter);
            }

            return playersInStomach.empty();
        }

        void UpdateStomachOfCthun(uint32 diff)
        {
            // Update the punt creature
            if (Creature* pc = puntCreatureGuid.IsEmpty() ? nullptr : instance->GetCreature(puntCreatureGuid))
            {
                // Updating animation
                if (quakeTimer < diff)
                {
                    // TODO(classic): VMaNGOS pc->SendSpellGo(pc, SPELL_QUAKE) visual
                    quakeTimer = 1000;
                }
                else
                    quakeTimer -= diff;

                // Checking if it's time to punt
                if (puntCountdown < diff)
                {
                    pc->CastSpell(pc, SPELL_PUNT_UPWARD, true);
                    puntCountdown = std::numeric_limits<uint32>::max();
                    puntCreatureGuid.Clear();
                }
                else
                    puntCountdown -= diff;
            }
            else
                puntCreatureGuid.Clear();

            // Update the players in the stomach
            if (playersInStomach.empty())
                return;

            AreaTriggerEntry const* pot = sAreaTriggerStore.LookupEntry(AREATRIGGER_STOMACH_AIR);
            for (auto it = playersInStomach.begin(); it != playersInStomach.end();)
            {
                Player* player = instance->GetPlayer(it->first);
                // Player has left instance or something and we remove him from the list.
                if (!player)
                {
                    it = playersInStomach.erase(it);
                    continue;
                }

                StomachTimers& timers = it->second;
                timers.timeSincePortedToStomach += diff;

                // playerPositionZ > 0.0 (stomach teleport-out trigger is at ~-30.0f, c'thun is at ~100.f)
                // means the player is outside the stomach as far as the server is concerned.
                if (player->GetPositionZ() > 0.0f)
                {
                    // timeSincePortedToStomach prevents the player from being removed from stomach on first
                    // update after being TPed to the stomach, as the server might not realize the player
                    // has been teleported during the same update that TeleportPlayerToCThun is called
                    if (timers.timeSincePortedToStomach > 4000)
                    {
                        if (player->HasAura(SPELL_DIGESTIVE_ACID))
                            player->RemoveAurasDueToSpell(SPELL_DIGESTIVE_ACID);

                        // HandleStomachTriggers() might be first to the party and perform the knockback,
                        // in which case we skip it here. We also use timeSincePortedFromStomach > 0 to delay
                        // the knockback one server-update, as it seems the knockback might not always hit
                        // if it's cast on the same update as the player is ported.
                        if (!timers.didKnockback && timers.timeSincePortedFromStomach > 0)
                        {
                            PerformCthunKnockback();
                            timers.didKnockback = true;
                        }
                        else
                            timers.timeSincePortedFromStomach += diff;

                        // ~1.5 sec after being registered as outside the stomach, we remove the player from the list.
                        // The delay will prevent tentacles to spawn in the center of c'thun through extremely bad luck, should
                        // c'thun attempt to spawn a tentacle on the player just as he is TPed out, before the knockback.
                        if (it->second.timeSincePortedFromStomach > 1500)
                        {
                            it = playersInStomach.erase(it);
                            continue;
                        }
                    }
                }
                else
                {
                    if (timers.acidDebuff < diff)
                    {
                        if (Creature* pCthun = GetSingleCreatureFromStorage(NPC_CTHUN))
                            pCthun->CastSpell(player, SPELL_DIGESTIVE_ACID, true);
                        timers.acidDebuff += StomachTimers::ACID_REFRESH_RATE;
                    }
                    else
                        timers.acidDebuff -= diff;

                    // Crude hack for teleporting players from the stomach if the areatrigger
                    // in the air above knockback area in stomach of c'thun did not trigger.
                    // (VMaNGOS also requires IsLaunched(); TC has no such state)
                    if (pot && player->IsFalling())
                        if (player->GetDistance(pot->Pos.X, pot->Pos.Y, pot->Pos.Z) <= pot->Radius)
                            TeleportPlayerToCThun(player);
                }

                ++it;
            }
        }

        void RestoreOuroSpawnTrigger()
        {
            if (Creature* pOuroSpawnTrigger = GetSingleCreatureFromStorage(NPC_OURO_SPAWNER))
            {
                // restore home coordinates from db spawn
                pOuroSpawnTrigger->SetHomePosition(-9188.45f, 2091.56f, -64.17f, 6.01f);
                pOuroSpawnTrigger->Respawn();
            }
            m_uiRestoreOuroSpawnTriggerTimer = 0;
        }
    };

    InstanceScript* GetInstanceScript(InstanceMap* map) const override
    {
        return new classic_instance_temple_of_ahnqiraj_InstanceScript(map);
    }
};

/*######
## at_temple_ahnqiraj
######*/

class classic_at_temple_ahnqiraj : public AreaTriggerScript
{
public:
    classic_at_temple_ahnqiraj() : AreaTriggerScript("classic_at_temple_ahnqiraj") { }

    bool OnTrigger(Player* pPlayer, AreaTriggerEntry const* pAt) override
    {
        if (pAt->ID == AREATRIGGER_TWIN_EMPERORS || pAt->ID == AREATRIGGER_SARTURA)
        {
            if (pPlayer->IsGameMaster() || !pPlayer->IsAlive())
                return false;

            if (ClassicTempleOfAhnQirajInstanceScript* pInstance = GetClassicTempleOfAhnQirajInstance(pPlayer))
                pInstance->DoHandleTempleAreaTrigger(pAt->ID);
        }
        if (ClassicTempleOfAhnQirajInstanceScript* pInstance = GetClassicTempleOfAhnQirajInstance(pPlayer))
            pInstance->HandleStomachTriggers(pPlayer, pAt);

        return false;
    }
};

/*######
## mob_qiraji_mindslayer
######*/

namespace
{
enum ClassicAQ40Mindslayer : uint32
{
    SPELL_AQ40_MINDSLAYER_MIND_FLAY      = 26044,
    SPELL_AQ40_MINDSLAYER_MIND_BLAST     = 26048,
    SPELL_AQ40_MINDSLAYER_MANA_BURN      = 26049,
    SPELL_AQ40_MINDSLAYER_CAUSE_INSANITY = 26079
};
}

struct classic_mob_qiraji_mindslayer : public ScriptedAI
{
    uint32 insanityTimer;
    uint32 mindBlastTimer;
    uint32 mindFlayTimer;

    classic_mob_qiraji_mindslayer(Creature* pCreature) : ScriptedAI(pCreature)
    {
        Reset();
    }

    void Reset() override
    {
        insanityTimer = 10000;
        mindBlastTimer = 5000;
        mindFlayTimer = urand(12000, 15000);
    }

    void JustDied(Unit* /*pWho*/) override
    {
        if (!me->GetInstanceScript())
            return;

        // finding closest player and casting manaburn on that target.
        // todo: should we add a player->GetPowerType() == POWER_MANA check too when choosing valid target?
        Player* closestPlayer = nullptr;
        float closestDist = std::numeric_limits<float>::max();
        for (MapReference const& ref : me->GetMap()->GetPlayers())
        {
            if (Player* player = ref.GetSource())
            {
                if (player->IsAlive())
                {
                    float dist = me->GetDistance(player);
                    if (dist < closestDist)
                    {
                        closestPlayer = player;
                        closestDist = dist;
                    }
                }
            }
        }
        if (closestPlayer)
            me->CastSpell(closestPlayer, SPELL_AQ40_MINDSLAYER_MANA_BURN, CastSpellExtraArgs(TRIGGERED_FULL_MASK));
    }

    // VMaNGOS SELECT_FLAG_PLAYER | SELECT_FLAG_IN_LOS
    bool IsPlayerInLos(Unit const* target) const
    {
        return target->IsPlayer() && me->IsWithinLOSInMap(target);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!me->IsNonMeleeSpellCast(false)) // prevents re-targetting of topaggro from happening while channeling mindflay
        {
            if (!UpdateVictim())
                return;
        }
        else if (!me->IsInCombat())
            return;

        if (mindFlayTimer < diff)
        {
            if (Unit* pU = SelectTarget(SelectTargetMethod::Random, 0, [this](Unit* u) { return IsPlayerInLos(u); }))
                if (DoCast(pU, SPELL_AQ40_MINDSLAYER_MIND_FLAY) == SPELL_CAST_OK)
                    mindFlayTimer = urand(12000, 15000);
        }
        else
            mindFlayTimer -= diff;

        if (mindBlastTimer < diff)
        {
            if (Unit* pU = SelectTarget(SelectTargetMethod::MaxThreat, 0, [this](Unit* u) { return IsPlayerInLos(u); }))
                if (DoCast(pU, SPELL_AQ40_MINDSLAYER_MIND_BLAST) == SPELL_CAST_OK)
                    mindBlastTimer = urand(9000, 12000);
        }
        else
            mindBlastTimer -= diff;

        if (insanityTimer < diff)
        {
            if (Unit* pU = SelectTarget(SelectTargetMethod::Random, 0, [this](Unit* u) { return IsPlayerInLos(u); }))
                if (DoCast(pU, SPELL_AQ40_MINDSLAYER_CAUSE_INSANITY) == SPELL_CAST_OK)
                    insanityTimer = 10000;
        }
        else
            insanityTimer -= diff;
    }
};

/*######
## spell_aq40_drain_mana
######*/

// 26457 - Drain Mana (Obsidian Eradicator)
// 26559 - Drain Mana (Obsidian Nullifier)
class classic_spell_aq40_drain_mana : public SpellScript
{
    void FilterTargets(std::list<WorldObject*>& targets)
    {
        // Avoid targeting players with no mana
        targets.remove_if([](WorldObject* obj)
        {
            Unit* target = obj->ToUnit();
            return !target || target->GetPowerType() != POWER_MANA || target->GetPowerPct(POWER_MANA) < 1.0f;
        });

        // VMaNGOS OnSetTargetMap: unMaxTargets = 12
        // TODO(classic): TC still applies the spell's own MaxAffectedTargets afterwards if the client data sets one.
        Trinity::Containers::RandomResize(targets, 12);
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(classic_spell_aq40_drain_mana::FilterTargets, EFFECT_ALL, TARGET_UNIT_SRC_AREA_ENEMY);
    }
};

void AddSC_classic_instance_temple_of_ahnqiraj()
{
    new classic_instance_temple_of_ahnqiraj();
    new classic_at_temple_ahnqiraj();
    RegisterCreatureAI(classic_mob_qiraji_mindslayer);
    RegisterSpellScript(classic_spell_aq40_drain_mana);
}
