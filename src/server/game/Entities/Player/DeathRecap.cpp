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

#include "DeathRecap.h"
#include "CellImpl.h"
#include "CharacterCache.h"
#include "Chat.h"
#include "Config.h"
#include "Creature.h"
#include "DB2Stores.h"
#include "GameTime.h"
#include "GridNotifiersImpl.h"
#include "Guild.h"
#include "Item.h"
#include "Log.h"
#include "Map.h"
#include "MoveSplineInit.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "QueryPackets.h"
#include "ObjectMgr.h"
#include "Spell.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "SpellPackets.h"
#include "StringFormat.h"
#include "TemporarySummon.h"
#include "World.h"
#include "WorldSession.h"
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace
{
    constexpr uint32 ACTOR_ENTRY = 8100000;         // below 2^23: the Classic creature GUID holds 23 bits of entry (sql/custom/world/2026_10_01_02_world_death_recap_entries.sql)
    constexpr uint32 NAMED_ACTOR_FIRST = 8100001;   // 2000 copies: one per replayed player name, reused in turn
    constexpr uint32 NAMED_ACTOR_COUNT = 2000;
    constexpr size_t MAX_UNITS_PER_FRAME = 40;
    constexpr size_t MAX_RECAPS = 200;
    constexpr uint32 END_DELAY = 4000;              // keep watching the body for a moment after the death

    struct Config
    {
        bool Enabled = true;
        uint32 MinLevel = 10;
        uint32 Seconds = 20;
        uint32 Interval = 200;
        float Range = 40.0f;
        uint32 Announce = 3;                        // 0 nobody, 1 the player who died, 2 everyone on that map, 3 everyone
    } Cfg;

    enum class EventType : uint8
    {
        SpellStart,
        SpellGo,
        Melee,
        SpellDamage
    };

    struct Event
    {
        uint32 Time = 0;
        EventType Type = EventType::Melee;
        ObjectGuid Caster;
        ObjectGuid Target;
        uint32 SpellId = 0;
        int32 Visual = 0;
        int32 ScriptVisual = 0;
        uint32 CastTime = 0;
        uint32 CastFlags = 0;
        uint32 Damage = 0;
        uint32 Absorb = 0;
        uint32 Resist = 0;
        uint32 Blocked = 0;
        uint32 HitInfo = 0;
        uint32 School = 0;
        uint8 VictimState = 0;
    };

    struct UnitInfo
    {
        uint32 Entry = 0;
        bool IsPlayer = false;
        std::string Name;
        uint8 Level = 1;
        uint32 Faction = 35;
        float Scale = 1.0f;
        std::array<uint32, 3> Items = { };
        // players: appearance for the client's mirror image request (UNIT_FLAG2_MIRROR_IMAGE)
        uint8 Race = 0, Gender = 0, Class = 0;
        float DisplayScale = 1.0f;
        std::vector<UF::ChrCustomizationChoice> Customizations;
        std::array<int32, 11> ItemDisplays = { };
        ObjectGuid GuildGuid;
    };

    struct UnitSample
    {
        ObjectGuid Guid;
        float X = 0.0f, Y = 0.0f, Z = 0.0f, O = 0.0f;
        uint64 Health = 0;
        uint64 MaxHealth = 1;
        uint32 Display = 0;
        uint32 Mount = 0;
        bool Dead = false;
    };

    struct Frame
    {
        uint32 Time = 0;
        std::vector<UnitSample> Units;
    };

    struct Recorder
    {
        uint32 Timer = 0;
        std::deque<Frame> Frames;
        std::unordered_map<ObjectGuid, UnitInfo> Infos;
    };

    struct Recap
    {
        uint32 Id = 0;
        ObjectGuid Victim;
        std::string VictimName;
        uint32 MapId = 0;
        uint32 InstanceId = 0;
        uint32 DeathTime = 0;
        std::vector<Frame> Frames;
        std::vector<Event> Events;
        std::unordered_map<ObjectGuid, UnitInfo> Infos;
    };

    struct Viewing
    {
        std::shared_ptr<Recap const> Data;
        uint32 Start = 0;
        size_t NextFrame = 0;
        size_t NextEvent = 0;
        std::unordered_map<ObjectGuid, ObjectGuid> Actors;      // recorded unit -> actor
        std::unordered_map<ObjectGuid, ObjectGuid> CastIds;     // actor -> cast id of its last SPELL_START
        ObjectGuid Camera;
        bool Remote = false;                                    // viewer was taken to the recap's map
        bool WasNonAttackable = false;
    };

    struct PendingWatch
    {
        std::shared_ptr<Recap const> Data;
        uint32 Since = 0;
    };

    std::mutex Lock;
    std::unordered_map<ObjectGuid, Recorder> Recorders;
    std::map<std::pair<uint32, uint32>, std::deque<Event>> MapEvents;   // (map, instance) -> recent combat events
    std::map<uint32, std::shared_ptr<Recap const>> Recaps;
    std::unordered_map<ObjectGuid, Viewing> Viewings;
    std::unordered_map<ObjectGuid, PendingWatch> Pending;                 // viewer teleporting to the recap's map
    std::unordered_map<uint32, std::string> ActorNames;                   // named actor entry -> name
    std::unordered_map<ObjectGuid, UnitInfo> ActorLooks;                  // player actor -> recorded appearance
    uint32 NextRecapId = 0;
    uint32 NextNamedActor = 0;

    uint32 KeepMs() { return (Cfg.Seconds + 2) * IN_MILLISECONDS; }

    // Replay actors, and units the recorder should ignore: private objects (other replays, personal spawns).
    bool IsIgnored(WorldObject const* object)
    {
        return !object || !object->IsInWorld() || object->IsPrivateObject();
    }

    void PushEvent(WorldObject const* source, Event&& ev)
    {
        std::lock_guard lock(Lock);
        std::deque<Event>& events = MapEvents[{ source->GetMapId(), source->GetInstanceId() }];
        events.push_back(std::move(ev));
        uint32 const oldest = events.back().Time - std::min(events.back().Time, KeepMs());
        while (!events.empty() && events.front().Time < oldest)
            events.pop_front();
    }

    UnitInfo MakeInfo(Unit* unit)
    {
        UnitInfo info;
        info.Entry = unit->GetEntry();
        info.IsPlayer = unit->IsPlayer();
        info.Name = unit->GetName();
        if (unit->IsPlayer())
            if (std::string const surname = sCharacterCache->GetCharacterSurnameByGuid(unit->GetGUID()); !surname.empty())
                info.Name += " " + surname;
        info.Level = unit->GetLevel();
        info.Faction = unit->GetFaction();
        info.Scale = unit->GetObjectScale();
        if (Player* player = unit->ToPlayer())
        {
            uint8 const slots[3] = { EQUIPMENT_SLOT_MAINHAND, EQUIPMENT_SLOT_OFFHAND, EQUIPMENT_SLOT_RANGED };
            for (uint8 i = 0; i < 3; ++i)
                if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slots[i]))
                    info.Items[i] = item->GetEntry();

            info.Race = player->GetRace();
            info.Gender = player->GetGender();
            info.Class = player->GetClass();
            info.DisplayScale = player->GetDisplayScale();
            info.Customizations.assign(player->m_playerData->Customizations.begin(), player->m_playerData->Customizations.end());
            static constexpr EquipmentSlots lookSlots[11] =
            {
                EQUIPMENT_SLOT_HEAD, EQUIPMENT_SLOT_SHOULDERS, EQUIPMENT_SLOT_BODY, EQUIPMENT_SLOT_CHEST, EQUIPMENT_SLOT_WAIST, EQUIPMENT_SLOT_LEGS,
                EQUIPMENT_SLOT_FEET, EQUIPMENT_SLOT_WRISTS, EQUIPMENT_SLOT_HANDS, EQUIPMENT_SLOT_TABARD, EQUIPMENT_SLOT_BACK
            };
            for (uint8 i = 0; i < 11; ++i)
                if (Item const* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, lookSlots[i]))
                    info.ItemDisplays[i] = int32(item->GetDisplayId(player));
            if (Guild* guild = player->GetGuild())
                info.GuildGuid = guild->GetGUID();
        }
        else
            for (uint8 i = 0; i < 3; ++i)
                info.Items[i] = unit->GetVirtualItemId(i);
        return info;
    }

    UnitSample MakeSample(Unit* unit)
    {
        UnitSample s;
        s.Guid = unit->GetGUID();
        unit->GetPosition(s.X, s.Y, s.Z, s.O);
        s.Health = unit->GetHealth();
        s.MaxHealth = std::max<uint64>(unit->GetMaxHealth(), 1);
        s.Display = unit->GetDisplayId();
        s.Mount = unit->GetMountDisplayId();
        s.Dead = !unit->IsAlive();
        return s;
    }

    std::string ClassName(uint8 classId)
    {
        if (ChrClassesEntry const* entry = sChrClassesStore.LookupEntry(classId))
            return entry->Name[DEFAULT_LOCALE];
        return "";
    }

    // RAID_CLASS_COLORS
    char const* ClassColor(uint8 classId)
    {
        switch (classId)
        {
            case CLASS_WARRIOR:      return "c69b6d";
            case CLASS_PALADIN:      return "f48cba";
            case CLASS_HUNTER:       return "aad372";
            case CLASS_ROGUE:        return "fff468";
            case CLASS_PRIEST:       return "ffffff";
            case CLASS_DEATH_KNIGHT: return "c41e3a";
            case CLASS_SHAMAN:       return "0070dd";
            case CLASS_MAGE:         return "3fc7eb";
            case CLASS_WARLOCK:      return "8788ee";
            case CLASS_MONK:         return "00ff98";
            case CLASS_DRUID:        return "ff7c0a";
            case CLASS_DEMON_HUNTER: return "a330c9";
            case CLASS_EVOKER:       return "33937f";
            default:                 return "ffffff";
        }
    }

    std::string AreaName(uint32 areaId)
    {
        if (AreaTableEntry const* area = sAreaTableStore.LookupEntry(areaId))
            return area->AreaName[DEFAULT_LOCALE];
        return "";
    }

    void Announce(Player* victim, std::string const& text)
    {
        if (Cfg.Announce == 0)
            return;
        if (Cfg.Announce == 1)
        {
            ChatHandler(victim->GetSession()).SendSysMessage(text);
            return;
        }
        // A bot's death is never announced (playtest: the chat was flooded with every bot death, first in Arathi Basin, then in
        // Molten Core). The recap is still recorded and `.recap <n>` still works.
        if (victim->GetSession() && victim->GetSession()->IsBot())
            return;
        for (auto const& [id, session] : sWorld->GetAllSessions())
        {
            Player* player = session->GetPlayer();
            if (!player || !player->IsInWorld())
                continue;
            if (Cfg.Announce == 2 && player->GetMapId() != victim->GetMapId())
                continue;
            ChatHandler(session).SendSysMessage(text);
        }
    }

    // ------------------------------------------------------------------ replay helpers (viewer's map thread)
    Creature* GetActor(Player* viewer, Viewing& v, ObjectGuid const& recorded)
    {
        auto itr = v.Actors.find(recorded);
        return itr != v.Actors.end() ? viewer->GetMap()->GetCreature(itr->second) : nullptr;
    }

    uint32 NamedActorEntry(std::string const& name)
    {
        std::lock_guard lock(Lock);
        uint32 entry = NAMED_ACTOR_FIRST + (NextNamedActor++ % NAMED_ACTOR_COUNT);
        ActorNames[entry] = name;
        return entry;
    }

    Creature* SpawnActor(Player* viewer, Viewing& v, UnitSample const& s)
    {
        auto infoItr = v.Data->Infos.find(s.Guid);
        if (infoItr == v.Data->Infos.end())
            return nullptr;
        UnitInfo const& info = infoItr->second;

        uint32 const entry = info.IsPlayer ? NamedActorEntry(info.Name) : ACTOR_ENTRY;
        Milliseconds const lifetime(v.Data->Frames.back().Time - v.Data->Frames.front().Time + END_DELAY + 30 * IN_MILLISECONDS);
        TempSummon* actor = viewer->GetMap()->SummonCreature(entry, Position(s.X, s.Y, s.Z, s.O), nullptr, lifetime, viewer, 0, 0, viewer->GetGUID());
        if (!actor)
            return nullptr;

        // Creatures show under their own entry (name, model data); players under a pool entry named after them.
        if (!info.IsPlayer && sObjectMgr->GetCreatureTemplate(info.Entry))
            actor->SetEntry(info.Entry);
        actor->SetDisplayId(s.Display, true);
        actor->SetLevel(info.Level);
        actor->SetFaction(info.Faction);
        actor->SetObjectScale(info.Scale);
        for (uint8 i = 0; i < 3; ++i)
            actor->SetVirtualItem(i, info.Items[i]);
        actor->SetReactState(REACT_PASSIVE);
        if (info.IsPlayer)
        {
            // the client draws a character model only with its appearance: it asks for it (CMSG_GET_MIRROR_IMAGE_DATA)
            actor->SetRace(info.Race);
            actor->SetGender(Gender(info.Gender));
            actor->SetClass(info.Class);
            actor->SetUnitFlag2(UNIT_FLAG2_MIRROR_IMAGE);
            std::lock_guard lock(Lock);
            ActorLooks[actor->GetGUID()] = info;
        }
        actor->SetMaxHealth(s.MaxHealth);
        actor->SetHealth(s.Health);
        if (s.Mount)
            actor->Mount(s.Mount);

        v.Actors[s.Guid] = actor->GetGUID();
        return actor;
    }

    void ApplySample(Viewing& v, Creature* actor, UnitSample const& s, UnitSample const* next, uint32 frameTime)
    {
        if (!actor->IsAlive())
            return;

        if (actor->GetMaxHealth() != s.MaxHealth)
            actor->SetMaxHealth(s.MaxHealth);
        if (actor->GetHealth() != s.Health)
            actor->SetHealth(std::max<uint64>(s.Health, s.Dead ? 0 : 1));
        if (actor->GetDisplayId() != s.Display)
            actor->SetDisplayId(s.Display, true);
        if (actor->GetMountDisplayId() != s.Mount)
        {
            if (s.Mount)
                actor->Mount(s.Mount);
            else
                actor->Dismount();
        }

        if (s.Dead)
        {
            actor->StopMoving();
            actor->setDeathState(JUST_DIED);
            return;
        }

        if (!next)
            return;

        float const dist = std::sqrt((next->X - s.X) * (next->X - s.X) + (next->Y - s.Y) * (next->Y - s.Y) + (next->Z - s.Z) * (next->Z - s.Z));
        uint32 nextTime = frameTime;
        for (Frame const& f : v.Data->Frames)
            if (f.Time > frameTime) { nextTime = f.Time; break; }
        float const seconds = std::max<uint32>(nextTime - frameTime, 50) / 1000.0f;

        if (dist > 0.1f)
        {
            Movement::MoveSplineInit init(actor);
            init.MoveTo(next->X, next->Y, next->Z, false, true);
            init.SetVelocity(std::clamp(dist / seconds, 0.5f, 60.0f));
            init.SetFacing(next->O);
            init.Launch();
        }
        else if (std::fabs(Position::NormalizeOrientation(next->O - actor->GetOrientation())) > 0.1f)
            actor->SetFacingTo(next->O);
    }

    void PlayEvent(Player* viewer, Viewing& v, Event const& ev)
    {
        Creature* caster = GetActor(viewer, v, ev.Caster);
        if (!caster || !caster->IsAlive())
            return;
        Creature* target = ev.Target.IsEmpty() ? nullptr : GetActor(viewer, v, ev.Target);
        Map* map = viewer->GetMap();

        switch (ev.Type)
        {
            case EventType::SpellStart:
            case EventType::SpellGo:
            {
                ObjectGuid castId;
                if (ev.Type == EventType::SpellGo)
                {
                    auto itr = v.CastIds.find(caster->GetGUID());
                    if (itr != v.CastIds.end() && itr->second.GetEntry() == ev.SpellId)
                        castId = itr->second;
                    v.CastIds.erase(caster->GetGUID());
                }
                if (castId.IsEmpty())
                    castId = ObjectGuid::Create<HighGuid::Cast>(SPELL_CAST_SOURCE_NORMAL, map->GetId(), ev.SpellId, map->GenerateLowGuid<HighGuid::Cast>());

                WorldPackets::Spells::SpellCastData data;
                data.CasterGUID = caster->GetGUID();
                data.CasterUnit = caster->GetGUID();
                data.CastID = castId;
                data.SpellID = ev.SpellId;
                data.Visual.SpellXSpellVisualID = ev.Visual;
                data.Visual.ScriptVisualID = ev.ScriptVisual;
                data.CastFlags = ev.CastFlags & CAST_FLAG_PENDING;
                if (target)
                {
                    data.Target.Flags = TARGET_FLAG_UNIT;
                    data.Target.Unit = target->GetGUID();
                }

                if (ev.Type == EventType::SpellStart)
                {
                    WorldPackets::Spells::SpellStart packet;
                    data.CastFlags |= CAST_FLAG_HAS_TRAJECTORY;
                    data.CastTime = ev.CastTime;
                    packet.Cast = std::move(data);
                    caster->SendMessageToSet(packet.Write(), true);
                    v.CastIds[caster->GetGUID()] = castId;
                }
                else
                {
                    WorldPackets::Spells::SpellGo packet;
                    data.CastFlags |= CAST_FLAG_UNKNOWN_9;
                    data.CastTime = getMSTime();
                    if (target)
                    {
                        data.HitTargets.push_back(target->GetGUID());
                        data.HitStatus.emplace_back(SPELL_MISS_NONE);
                    }
                    packet.Cast = std::move(data);
                    packet.LogData.Initialize(caster);
                    caster->SendCombatLogMessage(&packet);
                }
                break;
            }
            case EventType::Melee:
                if (target)
                {
                    caster->SetFacingToObject(target, false);
                    caster->SendAttackStateUpdate(ev.HitInfo, target, 1, SpellSchoolMask(ev.School), ev.Damage + ev.Absorb + ev.Resist + ev.Blocked,
                        ev.Absorb, ev.Resist, VictimState(ev.VictimState), ev.Blocked, 0);
                }
                break;
            case EventType::SpellDamage:
                if (target)
                {
                    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(ev.SpellId, DIFFICULTY_NONE);
                    if (!spellInfo)
                        break;
                    SpellNonMeleeDamage log(caster, target, spellInfo, { uint32(ev.Visual), uint32(ev.ScriptVisual) }, ev.School);
                    log.damage = ev.Damage;
                    log.originalDamage = ev.Damage;
                    log.absorb = ev.Absorb;
                    log.resist = ev.Resist;
                    log.blocked = ev.Blocked;
                    log.HitInfo = ev.HitInfo;
                    log.preHitHealth = target->GetHealth();
                    caster->SendSpellNonMeleeDamageLog(&log);
                }
                break;
        }
    }

    void EndViewing(Player* viewer, Viewing& v)
    {
        Map* map = viewer->GetMap();
        if (Creature* camera = map->GetCreature(v.Camera))
            if (viewer->GetViewpoint() == camera)
                viewer->SetViewpoint(camera, false);
        viewer->SetWatchingDeathRecap(false);

        for (auto const& [recorded, actorGuid] : v.Actors)
            if (Creature* actor = map->GetCreature(actorGuid))
                actor->DespawnOrUnsummon();
        {
            std::lock_guard lock(Lock);
            for (auto const& [recorded, actorGuid] : v.Actors)
                ActorLooks.erase(actorGuid);
        }

        if (v.Remote)
        {
            viewer->SetVisible(true);
            viewer->SetControlled(false, UNIT_STATE_ROOT);
            if (!v.WasNonAttackable)
                viewer->RemoveUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
        }

        // show the live world again
        viewer->UpdateVisibilityForPlayer();
    }

    // Back to where the viewer was before being taken to the recap (not while logging out: then the character is
    // saved at m_deathRecapReturn instead, see Player::SaveToDB).
    void SendBack(Player* viewer, bool loggingOut)
    {
        if (!viewer->m_deathRecapReturn)
            return;
        if (loggingOut)
            return;
        WorldLocation const back = *viewer->m_deathRecapReturn;
        viewer->m_deathRecapReturn.reset();
        if (!viewer->IsBeingTeleportedFar())
            viewer->TeleportTo(back.GetMapId(), back.GetPositionX(), back.GetPositionY(), back.GetPositionZ(), back.GetOrientation());
    }

    std::string StartViewing(Player* viewer, std::shared_ptr<Recap const> recap, bool remote);
}

void DeathRecap::LoadConfig()
{
    // a Hardcore realm feature: other realms neither record, announce nor play recaps
    Cfg.Enabled = sConfigMgr->GetBoolDefault("Recap.Enable", true) && sConfigMgr->GetBoolDefault("Classic.Hardcore", false);
    Cfg.MinLevel = sConfigMgr->GetIntDefault("Recap.MinLevel", 10);
    Cfg.Seconds = std::clamp<uint32>(sConfigMgr->GetIntDefault("Recap.Seconds", 20), 5, 60);
    Cfg.Interval = std::clamp<uint32>(sConfigMgr->GetIntDefault("Recap.Interval", 200), 100, 1000);
    Cfg.Range = std::clamp(sConfigMgr->GetFloatDefault("Recap.Range", 40.0f), 10.0f, 100.0f);
    Cfg.Announce = std::min<uint32>(sConfigMgr->GetIntDefault("Recap.Announce", 3), 3);
}

void DeathRecap::RecordFrame(Player* player, uint32 diff)
{
    if (!Cfg.Enabled || player->GetLevel() < Cfg.MinLevel || !player->IsAlive() || player->IsWatchingDeathRecap() || !player->IsInWorld())
        return;

    Recorder* rec;
    {
        std::lock_guard lock(Lock);
        rec = &Recorders[player->GetGUID()];
    }

    rec->Timer += diff;
    if (rec->Timer < Cfg.Interval)
        return;
    rec->Timer = 0;

    std::list<Unit*> units;
    Trinity::AnyUnitInObjectRangeCheck check(player, Cfg.Range, true, false);
    Trinity::UnitListSearcher<Trinity::AnyUnitInObjectRangeCheck> searcher(player, units, check);
    Cell::VisitAllObjects(player, searcher, Cfg.Range);

    Frame frame;
    frame.Time = GameTime::GetGameTimeMS();
    frame.Units.push_back(MakeSample(player));

    std::lock_guard lock(Lock);
    rec->Infos[player->GetGUID()] = MakeInfo(player);
    for (Unit* unit : units)
    {
        if (frame.Units.size() >= MAX_UNITS_PER_FRAME)
            break;
        if (unit == player || IsIgnored(unit) || !player->CanSeeOrDetect(unit))
            continue;
        if (Creature const* creature = unit->ToCreature(); creature && creature->IsTrigger())
            continue;
        frame.Units.push_back(MakeSample(unit));
        auto infoItr = rec->Infos.find(unit->GetGUID());
        if (infoItr == rec->Infos.end() || infoItr->second.Level != unit->GetLevel())
            rec->Infos[unit->GetGUID()] = MakeInfo(unit);
    }

    rec->Frames.push_back(std::move(frame));
    uint32 const oldest = rec->Frames.back().Time - std::min(rec->Frames.back().Time, KeepMs());
    while (!rec->Frames.empty() && rec->Frames.front().Time < oldest)
        rec->Frames.pop_front();

    // forget units not seen in the kept frames
    if (rec->Infos.size() > MAX_UNITS_PER_FRAME * 4)
    {
        std::unordered_set<ObjectGuid> seen;
        for (Frame const& f : rec->Frames)
            for (UnitSample const& s : f.Units)
                seen.insert(s.Guid);
        std::erase_if(rec->Infos, [&](auto const& pair) { return !seen.contains(pair.first); });
    }
}

void DeathRecap::RecordCast(WorldObject const* caster, bool start, uint32 spellId, int32 spellXSpellVisualId, int32 scriptVisualId,
    uint32 castTime, uint32 castFlags, ObjectGuid const& target)
{
    if (!Cfg.Enabled || !caster->IsUnit() || IsIgnored(caster))
        return;

    Event ev;
    ev.Time = GameTime::GetGameTimeMS();
    ev.Type = start ? EventType::SpellStart : EventType::SpellGo;
    ev.Caster = caster->GetGUID();
    ev.Target = target;
    ev.SpellId = spellId;
    ev.Visual = spellXSpellVisualId;
    ev.ScriptVisual = scriptVisualId;
    ev.CastTime = castTime;
    ev.CastFlags = castFlags;
    PushEvent(caster, std::move(ev));
}

void DeathRecap::RecordMelee(CalcDamageInfo const* damageInfo)
{
    if (!Cfg.Enabled || IsIgnored(damageInfo->Attacker) || !damageInfo->Target)
        return;

    Event ev;
    ev.Time = GameTime::GetGameTimeMS();
    ev.Type = EventType::Melee;
    ev.Caster = damageInfo->Attacker->GetGUID();
    ev.Target = damageInfo->Target->GetGUID();
    ev.Damage = damageInfo->Damage;
    ev.Absorb = damageInfo->Absorb;
    ev.Resist = damageInfo->Resist;
    ev.Blocked = damageInfo->Blocked;
    ev.HitInfo = damageInfo->HitInfo;
    ev.School = damageInfo->DamageSchoolMask;
    ev.VictimState = damageInfo->TargetState;
    PushEvent(damageInfo->Attacker, std::move(ev));
}

void DeathRecap::RecordSpellDamage(SpellNonMeleeDamage const* log)
{
    if (!Cfg.Enabled || IsIgnored(log->attacker) || !log->target || !log->Spell)
        return;

    Event ev;
    ev.Time = GameTime::GetGameTimeMS();
    ev.Type = EventType::SpellDamage;
    ev.Caster = log->attacker->GetGUID();
    ev.Target = log->target->GetGUID();
    ev.SpellId = log->Spell->Id;
    ev.Visual = int32(log->SpellVisual.SpellXSpellVisualID);
    ev.ScriptVisual = int32(log->SpellVisual.ScriptVisualID);
    ev.Damage = log->damage;
    ev.Absorb = log->absorb;
    ev.Resist = log->resist;
    ev.Blocked = log->blocked;
    ev.HitInfo = log->HitInfo;
    ev.School = log->schoolMask;
    PushEvent(log->attacker, std::move(ev));
}

void DeathRecap::OnPlayerDeath(Player* player)
{
    if (!Cfg.Enabled || player->GetLevel() < Cfg.MinLevel || !player->IsInWorld())
        return;

    uint32 const now = GameTime::GetGameTimeMS();
    auto recap = std::make_shared<Recap>();
    std::string killer, killerSpell;
    {
        std::lock_guard lock(Lock);
        auto recItr = Recorders.find(player->GetGUID());
        if (recItr == Recorders.end() || recItr->second.Frames.size() < 3)
            return;
        Recorder& rec = recItr->second;

        uint32 const from = now - std::min(now, Cfg.Seconds * IN_MILLISECONDS);
        for (Frame const& f : rec.Frames)
            if (f.Time >= from)
                recap->Frames.push_back(f);
        if (recap->Frames.size() < 3)
            return;

        // the moment of death, with the victim dead
        Frame last = recap->Frames.back();
        last.Time = now;
        for (UnitSample& s : last.Units)
            if (s.Guid == player->GetGUID())
                s = MakeSample(player);
        recap->Frames.push_back(std::move(last));

        std::unordered_set<ObjectGuid> actors;
        for (Frame const& f : recap->Frames)
            for (UnitSample const& s : f.Units)
                actors.insert(s.Guid);
        for (ObjectGuid const& guid : actors)
        {
            auto infoItr = rec.Infos.find(guid);
            if (infoItr != rec.Infos.end())
                recap->Infos[guid] = infoItr->second;
        }

        auto eventsItr = MapEvents.find({ player->GetMapId(), player->GetInstanceId() });
        if (eventsItr != MapEvents.end())
        {
            for (Event const& ev : eventsItr->second)
            {
                if (ev.Time < recap->Frames.front().Time || ev.Time > now || !actors.contains(ev.Caster))
                    continue;
                recap->Events.push_back(ev);
                if (ev.Target == player->GetGUID() && (ev.Type == EventType::Melee || ev.Type == EventType::SpellDamage) && ev.Damage > 0)
                {
                    auto infoItr = recap->Infos.find(ev.Caster);
                    killer = infoItr != recap->Infos.end() ? infoItr->second.Name : "";
                    killerSpell.clear();
                    if (ev.Type == EventType::SpellDamage)
                        if (SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(ev.SpellId, DIFFICULTY_NONE))
                            killerSpell = (*spellInfo->SpellName)[DEFAULT_LOCALE];
                }
            }
        }

        recap->Id = ++NextRecapId;
        recap->Victim = player->GetGUID();
        recap->VictimName = player->GetName();
        recap->MapId = player->GetMapId();
        recap->InstanceId = player->GetInstanceId();
        recap->DeathTime = now;
        Recaps[recap->Id] = recap;
        while (Recaps.size() > MAX_RECAPS)
            Recaps.erase(Recaps.begin());
        rec.Frames.clear();
    }

    // [Recap #n] in light purple, "Name Surname" in the class color, the rest in red
    std::string name = player->GetName();
    std::string const surname = sCharacterCache->GetCharacterSurnameByGuid(player->GetGUID());
    if (!surname.empty())
        name += " " + surname;
    std::string text = Trinity::StringFormat("|cffc9a0ff[Recap #{}]|r |cff{}{}|r|cffff4040 (level {} {}) ", recap->Id, ClassColor(player->GetClass()), name,
        player->GetLevel(), ClassName(player->GetClass()));
    if (!killer.empty() && killer != name)
        text += "was slain by " + killer + (killerSpell.empty() ? "" : " (" + killerSpell + ")");
    else
        text += "died";
    std::string const zone = AreaName(player->GetZoneId());
    if (!zone.empty())
        text += " in " + zone;
    text += Trinity::StringFormat(". Type .recap {} to watch.|r", recap->Id);
    Announce(player, text);
}

namespace
{
    std::string StartViewing(Player* viewer, std::shared_ptr<Recap const> recap, bool remote)
    {
        Viewing v;
        v.Data = recap;
        v.Start = GameTime::GetGameTimeMS();
        v.Remote = remote;

        // the camera: the actor of the player who died, spawned right away where they stood
        for (UnitSample const& s : recap->Frames.front().Units)
        {
            if (s.Guid != recap->Victim)
                continue;
            if (Creature* camera = SpawnActor(viewer, v, s))
                v.Camera = camera->GetGUID();
        }
        Creature* camera = viewer->GetMap()->GetCreature(v.Camera);
        if (!camera)
        {
            for (auto const& [recorded, actorGuid] : v.Actors)
                if (Creature* actor = viewer->GetMap()->GetCreature(actorGuid))
                    actor->DespawnOrUnsummon();
            SendBack(viewer, false);
            return "The recap couldn't be started.";
        }

        if (remote)
        {
            // the character stands at the death spot meanwhile: hidden, rooted and out of reach
            v.WasNonAttackable = viewer->HasUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
            viewer->SetVisible(false);
            viewer->SetControlled(true, UNIT_STATE_ROOT);
            viewer->SetUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
        }

        viewer->SetWatchingDeathRecap(true);
        viewer->SetViewpoint(camera, true);
        viewer->UpdateVisibilityForPlayer();
        {
            std::lock_guard lock(Lock);
            Viewings[viewer->GetGUID()] = std::move(v);
        }

        ChatHandler(viewer->GetSession()).SendSysMessage(Trinity::StringFormat("Watching recap #{} ({}). Type .recap stop to stop.", recap->Id, recap->VictimName));
        return "";
    }
}

std::string DeathRecap::Watch(Player* viewer, uint32 recapId)
{
    if (!Cfg.Enabled)
        return "The death recap is disabled on this realm.";

    std::shared_ptr<Recap const> recap;
    {
        std::lock_guard lock(Lock);
        auto itr = Recaps.find(recapId);
        if (itr != Recaps.end())
            recap = itr->second;
        if (Pending.contains(viewer->GetGUID()))
            return "You are already on your way to a recap.";
    }
    if (!recap)
        return Trinity::StringFormat("Recap #{} doesn't exist (anymore).", recapId);
    if (viewer->IsInCombat())
        return "You can't watch a recap while in combat.";
    if (viewer->IsInFlight())
        return "You can't watch a recap while flying.";
    if (viewer->IsBeingTeleported())
        return "You can't watch a recap while teleporting.";

    // a new recap right after another one: stay where we are (the return point is kept)
    bool const wasAway = viewer->m_deathRecapReturn.has_value();
    {
        Viewing old;
        bool had = false;
        {
            std::lock_guard lock(Lock);
            auto itr = Viewings.find(viewer->GetGUID());
            if (itr != Viewings.end())
            {
                old = std::move(itr->second);
                Viewings.erase(itr);
                had = true;
            }
        }
        if (had)
            EndViewing(viewer, old);
    }
    if (viewer->GetViewpoint())
        return "You are already looking through something else.";

    if (viewer->GetMapId() == recap->MapId && viewer->GetInstanceId() == recap->InstanceId)
        return StartViewing(viewer, recap, wasAway);

    // another map: take the character there (hidden) and back afterwards; not in or into instances
    MapEntry const* recapMap = sMapStore.LookupEntry(recap->MapId);
    if (!recapMap || recapMap->Instanceable())
    {
        SendBack(viewer, false);
        return "That death happened inside an instance: only players in that instance can watch it.";
    }
    if (viewer->GetMap()->Instanceable())
        return "Leave the instance or battleground to watch deaths elsewhere.";

    UnitSample const* start = nullptr;
    for (UnitSample const& s : recap->Frames.front().Units)
        if (s.Guid == recap->Victim)
            start = &s;
    if (!start)
        return "The recap couldn't be started.";

    if (!viewer->m_deathRecapReturn)
        viewer->m_deathRecapReturn = viewer->GetWorldLocation();
    {
        std::lock_guard lock(Lock);
        Pending[viewer->GetGUID()] = { recap, GameTime::GetGameTimeMS() };
    }
    if (!viewer->TeleportTo(recap->MapId, start->X, start->Y, start->Z, start->O))
    {
        {
            std::lock_guard lock(Lock);
            Pending.erase(viewer->GetGUID());
        }
        SendBack(viewer, false);
        return "You can't be taken there right now.";
    }

    ChatHandler(viewer->GetSession()).SendSysMessage(Trinity::StringFormat("Taking you to where {} died...", recap->VictimName));
    return "";
}

void DeathRecap::Stop(Player* viewer, char const* reason)
{
    Viewing v;
    {
        std::lock_guard lock(Lock);
        auto itr = Viewings.find(viewer->GetGUID());
        if (itr == Viewings.end())
            return;
        v = std::move(itr->second);
        Viewings.erase(itr);
    }

    EndViewing(viewer, v);
    SendBack(viewer, false);
    if (reason)
        ChatHandler(viewer->GetSession()).SendSysMessage(reason);
}

void DeathRecap::UpdateViewer(Player* viewer)
{
    // taken to another map: start once there
    std::shared_ptr<Recap const> arrived;
    bool pendingTimeout = false;
    {
        std::lock_guard lock(Lock);
        auto pending = Pending.find(viewer->GetGUID());
        if (pending != Pending.end())
        {
            if (!viewer->IsBeingTeleported() && viewer->GetMapId() == pending->second.Data->MapId)
                arrived = pending->second.Data;
            else if (GameTime::GetGameTimeMS() - pending->second.Since > 60 * IN_MILLISECONDS)
                pendingTimeout = true;
            if (arrived || pendingTimeout)
                Pending.erase(pending);
        }
    }
    if (pendingTimeout)
        SendBack(viewer, false);
    if (arrived)
    {
        std::string const error = StartViewing(viewer, arrived, true);
        if (!error.empty())
            ChatHandler(viewer->GetSession()).SendSysMessage(error);
        return;
    }

    Viewing* v;
    {
        std::lock_guard lock(Lock);
        auto itr = Viewings.find(viewer->GetGUID());
        if (itr == Viewings.end())
            return;
        v = &itr->second;   // only this viewer's map thread touches its entry
    }

    if (viewer->IsInCombat())
    {
        Stop(viewer, "Recap stopped: you are in combat.");
        return;
    }

    std::vector<Frame> const& frames = v->Data->Frames;
    uint32 const now = GameTime::GetGameTimeMS();
    uint32 const replayTime = frames.front().Time + (now - v->Start);

    while (v->NextFrame < frames.size() && frames[v->NextFrame].Time <= replayTime)
    {
        Frame const& frame = frames[v->NextFrame];
        Frame const* next = v->NextFrame + 1 < frames.size() ? &frames[v->NextFrame + 1] : nullptr;
        for (UnitSample const& s : frame.Units)
        {
            Creature* actor = GetActor(viewer, *v, s.Guid);
            if (!actor && !v->Actors.contains(s.Guid))
                actor = SpawnActor(viewer, *v, s);
            if (!actor)
                continue;

            UnitSample const* nextSample = nullptr;
            if (next)
                for (UnitSample const& n : next->Units)
                    if (n.Guid == s.Guid) { nextSample = &n; break; }
            ApplySample(*v, actor, s, nextSample, frame.Time);
        }
        ++v->NextFrame;
    }

    std::vector<Event> const& events = v->Data->Events;
    while (v->NextEvent < events.size() && events[v->NextEvent].Time <= replayTime)
        PlayEvent(viewer, *v, events[v->NextEvent++]);

    if (replayTime >= v->Data->DeathTime + END_DELAY)
        Stop(viewer, "Recap finished.");
}

void DeathRecap::OnRemoveFromWorld(Player* player)
{
    bool const teleporting = player->IsBeingTeleportedFar();
    Viewing v;
    bool watching = false;
    {
        std::lock_guard lock(Lock);
        Recorders.erase(player->GetGUID());
        if (!teleporting)
            Pending.erase(player->GetGUID());
        auto itr = Viewings.find(player->GetGUID());
        if (itr != Viewings.end())
        {
            v = std::move(itr->second);
            Viewings.erase(itr);
            watching = true;
        }
    }

    if (watching)
    {
        EndViewing(player, v);
        // teleported away by something else mid-replay: that is where the character goes now. When logging out the
        // return point stays set, so the character is saved back where it came from.
        if (teleporting)
            player->m_deathRecapReturn.reset();
    }
}

bool DeathRecap::BuildMirrorImage(ObjectGuid const& actor, WorldPackets::Spells::MirrorImageComponentedData& data)
{
    std::lock_guard lock(Lock);
    auto itr = ActorLooks.find(actor);
    if (itr == ActorLooks.end())
        return false;

    UnitInfo const& look = itr->second;
    data.UnitGUID = actor;
    if (ChrModelEntry const* chrModel = sDB2Manager.GetChrModel(look.Race, look.Gender))
        data.ChrModelID = chrModel->ID;
    data.DisplayScale = look.DisplayScale;
    data.RaceID = look.Race;
    data.Gender = look.Gender;
    data.ClassID = look.Class;
    data.Customizations = look.Customizations;
    data.GuildGUID = look.GuildGuid;
    data.ItemDisplayID.assign(look.ItemDisplays.begin(), look.ItemDisplays.end());
    return true;
}

bool DeathRecap::BuildCreatureQuery(uint32 entry, WorldPackets::Query::QueryCreatureResponse& response)
{
    if (entry < NAMED_ACTOR_FIRST || entry >= NAMED_ACTOR_FIRST + NAMED_ACTOR_COUNT)
        return false;

    std::string name;
    {
        std::lock_guard lock(Lock);
        auto itr = ActorNames.find(entry);
        if (itr == ActorNames.end())
            return false;
        name = itr->second;
    }

    response.CreatureID = entry;
    response.Allow = true;
    response.Stats.Name[0] = name;
    response.Stats.NameAlt[0] = name;
    response.Stats.Title = "Death Recap";
    response.Stats.CreatureType = CREATURE_TYPE_HUMANOID;
    response.Stats.Display.TotalProbability = 1.0f;
    response.Stats.Display.CreatureDisplay.push_back({ 49, 1.0f, 1.0f });
    response.Stats.HpMulti = 1.0f;
    response.Stats.EnergyMulti = 1.0f;
    return true;
}
