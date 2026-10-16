// BossWarnings — runtime side: finds the bosses the raid is fighting and sends the warnings. See BossWarnings.h.

#include "BossWarnings.h"
#include "BossWarningData_MC.h"
#include "../../Services.h"
#include "../../Fleet/BotIdentityRegistry.h"
#include "../../Util/ConfigReader.h"

#include "ChatPackets.h"
#include "Creature.h"
#include "Group.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Spell.h"
#include "SpellInfo.h"

#include <map>
#include <mutex>
#include <set>
#include <shared_mutex>
#include <unordered_map>

namespace Playerbot::Raid {

std::vector<BossWarning> const& BossWarningsFor(uint32_t bossEntry)
{
    static std::unordered_map<uint32_t, std::vector<BossWarning>> const byBoss = []
    {
        std::unordered_map<uint32_t, std::vector<BossWarning>> m;
        for (BossWarning const& w : MoltenCoreBossWarnings())
            m[w.boss].push_back(w);
        return m;
    }();
    static std::vector<BossWarning> const none;
    auto it = byBoss.find(bossEntry);
    return it == byBoss.end() ? none : it->second;
}

namespace {

constexpr uint32_t kTickMs = 250;
constexpr uint32_t kNewFightGapMs = 5000;   // not seen this long = a new fight (wipe and re-pull) for the same creature
constexpr uint32_t kForgetMs = 60000;

struct Fight
{
    explicit Fight(uint32_t entry) : tracker(&BossWarningsFor(entry)) {}
    BossWarnTracker tracker;
    uint32_t lastSeen = 0;
    std::set<std::pair<size_t, ObjectGuid>> carriers;   // (rule, member) already named while the aura is on them
};

std::unordered_map<ObjectGuid, Fight> g_fights;
uint32_t g_lastTick = 0;

} // namespace

void UpdateBossWarnings(uint32_t nowMs)
{
    if (nowMs - g_lastTick < kTickMs)
        return;
    g_lastTick = nowMs;

    if (!Services::Config().boss_warnings_enabled())
    {
        g_fights.clear();
        return;
    }

    // Humans in a raid-instance group, one entry per group.
    std::map<uint64_t, Player*> groups;
    {
        std::shared_lock<std::shared_mutex> lock(*HashMapHolder<Player>::GetLock());
        for (auto const& kv : ObjectAccessor::GetPlayers())
        {
            Player* p = kv.second;
            if (!p || !p->IsInWorld() || !p->GetGroup() || Services::Lifecycle().is_bot(p->GetGUID().GetCounter()))
                continue;
            Map* map = p->GetMap();
            if (!map || !map->IsRaid())
                continue;
            groups.emplace(p->GetGroup()->GetGUID().GetCounter(), p);
        }
    }

    for (auto const& [gid, human] : groups)
    {
        Group* group = human->GetGroup();
        if (!group)
            continue;

        // The bosses anyone in the group is in combat with.
        std::unordered_map<ObjectGuid, Creature*> bosses;
        for (GroupReference const& ref : group->GetMembers())
        {
            Player* m = ref.GetSource();
            if (!m || !m->IsInWorld() || m->GetMap() != human->GetMap())
                continue;
            for (auto const& [guid, combatRef] : m->GetCombatManager().GetPvECombatRefs())
            {
                if (bosses.count(guid))
                    continue;
                Creature* c = ObjectAccessor::GetCreature(*m, guid);
                if (c && c->IsAlive() && !BossWarningsFor(c->GetEntry()).empty())
                    bosses.emplace(guid, c);
            }
        }

        for (auto const& [guid, boss] : bosses)
        {
            std::vector<BossWarning> const& rules = BossWarningsFor(boss->GetEntry());
            auto it = g_fights.find(guid);
            if (it == g_fights.end())
                it = g_fights.emplace(guid, Fight(boss->GetEntry())).first;
            Fight& f = it->second;
            if (!f.lastSeen || nowMs - f.lastSeen > kNewFightGapMs)
                f.tracker.Start(nowMs);
            f.lastSeen = nowMs;

            uint32_t castingSpell = 0, castLeft = 0;
            for (CurrentSpellTypes t : { CURRENT_GENERIC_SPELL, CURRENT_CHANNELED_SPELL })
                if (Spell const* s = boss->GetCurrentSpell(t))
                {
                    castingSpell = s->GetSpellInfo()->Id;
                    castLeft = s->GetTimer() > 0 ? uint32_t(s->GetTimer()) : 0;
                    break;
                }

            std::vector<uint8_t> auraPresent(rules.size(), 0);
            for (size_t i = 0; i < rules.size(); ++i)
            {
                uint32_t const aura = rules[i].resyncAura;
                if (!aura)
                    continue;
                if (boss->HasAura(aura))
                    auraPresent[i] = 1;
                else
                    for (GroupReference const& ref : group->GetMembers())
                    {
                        Player* m = ref.GetSource();
                        if (m && m->IsInWorld() && m->GetMap() == boss->GetMap() && m->HasAura(aura))
                        {
                            auraPresent[i] = 1;
                            break;
                        }
                    }
            }

            auto announce = [&](std::string const& text)
            {
                for (GroupReference const& ref : group->GetMembers())
                {
                    Player* m = ref.GetSource();
                    if (!m || !m->IsInWorld() || m->GetMap() != boss->GetMap() || Services::Lifecycle().is_bot(m->GetGUID().GetCounter()))
                        continue;
                    WorldPackets::Chat::Chat packet;
                    packet.Initialize(CHAT_MSG_RAID_BOSS_EMOTE, LANG_UNIVERSAL, boss, m, text);
                    m->SendDirectMessage(packet.Write());
                }
            };

            // A member who has just gained a carrier aura (Living Bomb) is named to the humans the moment it lands, so they know who to
            // keep away from; the timed warning above only says one is coming.
            std::set<std::pair<size_t, ObjectGuid>> carrying;
            for (size_t i = 0; i < rules.size(); ++i)
            {
                if (!rules[i].carrierLabel || !rules[i].resyncAura)
                    continue;
                for (GroupReference const& ref : group->GetMembers())
                {
                    Player* m = ref.GetSource();
                    if (!m || !m->IsInWorld() || !m->IsAlive() || m->GetMap() != boss->GetMap() || !m->HasAura(rules[i].resyncAura))
                        continue;
                    auto const key = std::make_pair(i, m->GetGUID());
                    carrying.insert(key);
                    if (!f.carriers.count(key))
                        announce(m->GetName() + " " + rules[i].carrierLabel);
                }
            }
            f.carriers = std::move(carrying);

            for (BossWarnEvent const& e : f.tracker.Tick(nowMs, castingSpell, castLeft, auraPresent))
                announce(FormatBossWarning(boss->GetName(), rules[e.rule], e));
        }
    }

    for (auto it = g_fights.begin(); it != g_fights.end();)
        it = nowMs - it->second.lastSeen > kForgetMs ? g_fights.erase(it) : std::next(it);
}

} // namespace Playerbot::Raid
