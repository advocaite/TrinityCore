// Alterac Valley objective rules imported from ike3's bots (Karrz1/wow-1.12 raid10-revamp bcc8b9fe70c42a8fa5d50fa83a472da9e9d19546,
// BattleGroundTacticsAV.cpp): the mine-boss objective and the captain gathering threshold. Pure functions, no game state, so they can be
// tested offline (tests/game/BgAvPlan.cpp). The mid-field fighting tactics are ours and do not live here.
#pragma once

#include <cstdint>
#include <vector>

namespace Playerbot::BgAvPlan
{
    // The captain squad goes in once this many of the team stand at the waiting spot (ike3: attackers near the waiting spot >= 5).
    constexpr int kCaptainGatherCount = 5;
    // Time cap (playtest, AV 2026-10-11: a pile of bots waited at the Stonehearth staging spot for a quorum): after kCaptainGatherMaxMs of waiting the squad
    // goes with whoever is there (at least one), so bots arriving one at a time never park for good.
    constexpr uint32_t kCaptainGatherMaxMs = 75000;
    constexpr bool CaptainGather(int at_stage, bool latched, uint32_t waited_ms = 0)
    {
        return latched || at_stage >= kCaptainGatherCount || (at_stage >= 1 && waited_ms >= kCaptainGatherMaxMs);
    }

    struct MineTarget { float x, y, z; uint32_t entry; char const* name; };

    // Mine owner world states (801 Irondeep = north, 804 Coldtooth = south): 0 neutral, 1 Alliance, 2 Horde, 255 unread.
    // `team` is 1 = Alliance, 2 = Horde. Killing the boss of a mine the team does not own takes it: a neutral mine is held by its trogg /
    // kobold boss (Morloch / Taskmaster Snivvle), an enemy-held one by that team's boss (Alliance Umi Thorson / Agi Rumblestomp, Horde
    // Keetar / Masha Swiftcut). Positions are world.creature spawns; both sides' bosses of a mine spawn at the same spot.
    inline std::vector<MineTarget> MineTargets(uint8_t team, uint8_t north, uint8_t south)
    {
        std::vector<MineTarget> out;
        if (team != 1 && team != 2) return out;
        if (north != 255 && north != team && north <= 2)
            out.push_back({ 863.838f, -445.468f, 50.846f, north == 0 ? 11657u : (team == 1 ? 13079u : 13078u), "Irondeep Mine boss" });
        if (south != 255 && south != team && south <= 2)
            out.push_back({ -848.902f, -92.931f, 68.633f, south == 0 ? 11677u : (team == 1 ? 13088u : 13086u), "Coldtooth Mine boss" });
        return out;
    }

    // ---- AV quests (ike3 BattleGroundTacticsAV.cpp: the AV quester role; quest data from the VMaNGOS import, NPC spawns from
    // world.creature) ----
    // A small share of the team (IsQuester) works the Armor Scraps quest and the two mine-supply quests instead of walking the front the
    // whole match. Turn in what is complete, accept what can be done, fetch mine supplies from a mine the team owns. Armor scraps drop
    // from kills (the quest collects them while the bot fights); riders, wing commanders, beacons and the world-boss turn-in are not
    // here.
    constexpr uint32_t kScrapsItem = 17422, kIrondeepSupplies = 17522, kColdtoothSupplies = 17542;
    constexpr uint32_t kSuppliesNeeded = 10;
    constexpr uint32_t kMineGoNorth = 178785, kMineGoSouth = 178784;

    // One bot in eight is a quester (ike3: guid % 5 for the 51-59 bracket).
    constexpr bool IsQuester(uint64_t guid_low) { return guid_low % 8u == 0; }

    struct AvPlace { uint32_t entry; float x, y, z; };
    struct TeamQuests
    {
        uint32_t scraps_first, scraps_repeat, mine_north, mine_south;   // quest ids
        AvPlace armorer, quartermaster;                                    // quest NPCs (give and take every quest above)
        AvPlace go_north, go_south;                                        // the mine supply objects (Irondeep N, Coldtooth S)
    };
    // Alliance: scraps 7223 then 6781 (Murgot Deepforge), Irondeep 5892 / Coldtooth 6982 (Stormpike Quartermaster). Horde: scraps 7224
    // then 6741 (Smith Regzar), Coldtooth 5893 / Irondeep 6985 (Frostwolf Quartermaster).
    inline TeamQuests const& QuestsFor(uint8_t team)
    {
        static TeamQuests const alliance = { 7223, 6781, 5892, 6982, { 13257, 647.61f, -61.1548f, 41.7405f }, { 12096, 587.633f, -45.9816f, 37.5438f },
                                             { kMineGoNorth, 881.273f, -442.002f, 54.664f }, { kMineGoSouth, -853.671f, -91.427f, 68.569f } };
        static TeamQuests const horde    = { 7224, 6741, 6985, 5893, { 13176, -1251.5f, -316.327f, 62.6565f }, { 12097, -1293.79f, -194.407f, 72.4398f },
                                             { kMineGoNorth, 881.273f, -442.002f, 54.664f }, { kMineGoSouth, -853.671f, -91.427f, 68.569f } };
        return team == 2 ? horde : alliance;
    }
    // Every quest id the planner looks at, for the snapshot's rewarded mask (bit i = kQuestIds[i] was handed in once).
    constexpr uint32_t kQuestIds[8] = { 7223, 6781, 5892, 6982, 7224, 6741, 5893, 6985 };
    constexpr uint32_t QuestBit(uint32_t quest_id)
    {
        for (uint32_t i = 0; i < 8; ++i) if (kQuestIds[i] == quest_id) return 1u << i;
        return 0;
    }

    enum class AvQStatus : uint8_t { None, Incomplete, Complete, Rewarded };
    struct QuestInput
    {
        uint8_t team = 1;                        // 1 Alliance, 2 Horde
        uint8_t mine_owner_north = 255, mine_owner_south = 255;
        AvQStatus scraps_first = AvQStatus::None, scraps_repeat = AvQStatus::None, mine_north = AvQStatus::None, mine_south = AvQStatus::None;
        uint32_t supplies_north = 0, supplies_south = 0;   // Irondeep / Coldtooth supply items in the bags
    };
    enum class StepKind : uint8_t { None, TurnIn, Accept, UseMineGo };
    struct AvStep { StepKind kind = StepKind::None; uint32_t quest_id = 0; AvPlace place{0, 0, 0, 0}; };

    // The next thing the quester does; None = back to the fight. Mine quests are only taken and worked while the team owns that mine
    // (a mine the enemy holds gives no supplies); the scraps quest is kept running all match (first quest once, then the repeat one).
    inline AvStep NextStep(QuestInput const& in)
    {
        TeamQuests const& q = QuestsFor(in.team);
        // turn in
        if (in.scraps_first == AvQStatus::Complete)  return { StepKind::TurnIn, q.scraps_first,  q.armorer };
        if (in.scraps_repeat == AvQStatus::Complete) return { StepKind::TurnIn, q.scraps_repeat, q.armorer };
        if (in.mine_north == AvQStatus::Complete)    return { StepKind::TurnIn, q.mine_north,    q.quartermaster };
        if (in.mine_south == AvQStatus::Complete)    return { StepKind::TurnIn, q.mine_south,    q.quartermaster };
        // accept
        if (in.scraps_first == AvQStatus::None)
            return { StepKind::Accept, q.scraps_first, q.armorer };
        if (in.scraps_first == AvQStatus::Rewarded && in.scraps_repeat == AvQStatus::None)
            return { StepKind::Accept, q.scraps_repeat, q.armorer };
        bool const own_n = in.mine_owner_north == in.team, own_s = in.mine_owner_south == in.team;
        if (own_n && in.mine_north == AvQStatus::None) return { StepKind::Accept, q.mine_north, q.quartermaster };
        if (own_s && in.mine_south == AvQStatus::None) return { StepKind::Accept, q.mine_south, q.quartermaster };
        // fetch supplies: the quest is open, the bags are short, the mine is ours
        if (own_n && in.mine_north == AvQStatus::Incomplete && in.supplies_north < kSuppliesNeeded)
            return { StepKind::UseMineGo, q.mine_north, q.go_north };
        if (own_s && in.mine_south == AvQStatus::Incomplete && in.supplies_south < kSuppliesNeeded)
            return { StepKind::UseMineGo, q.mine_south, q.go_south };
        return {};
    }

    // ---- Attack order (ike3 BattleGroundTacticsAV.cpp attack lists; mod-playerbots AV_AttackObjectives_*: "first N non-owned, non-destroyed
    // entries", N = 2 balanced) ----
    // Both reference bots walk an ordered list of the enemy's nodes, graveyards INCLUDED: a graveyard held by the enemy sends every dead
    // bot of the team back to the far end of the valley, so taking it early matters as much as a tower. We had only towers and bunkers.
    struct AvListNode { float x, y; char const* name; };
    // Horde: Stonehearth GY, Stonehearth Bunker, Icewing Bunker, Stormpike GY, Dun Baldar South, Dun Baldar North, Stormpike Aid Station.
    inline std::vector<AvListNode> const& AttackList(uint8_t team)
    {
        static std::vector<AvListNode> const horde = {
            { 73.7f, -426.0f, "Stonehearth GY" }, { -154.0f, -446.0f, "Stonehearth Bunker" }, { 203.0f, -361.0f, "Icewing Bunker" },
            { 669.0f, -294.0f, "Stormpike GY" }, { 556.0f, -84.0f, "Dun Baldar South Bunker" }, { 678.0f, -139.0f, "Dun Baldar North Bunker" },
            { 638.5f, -31.5f, "Stormpike Aid Station" } };
        // Alliance: Iceblood GY, Iceblood Tower, Tower Point, Frostwolf GY, East / West Frostwolf Tower, Frostwolf Relief Hut.
        static std::vector<AvListNode> const alliance = {
            { -612.5f, -397.0f, "Iceblood GY" }, { -572.0f, -263.0f, "Iceblood Tower" }, { -768.0f, -363.0f, "Tower Point" },
            { -1082.5f, -347.0f, "Frostwolf GY" }, { -1303.0f, -317.0f, "East Frostwolf Tower" }, { -1298.0f, -267.0f, "West Frostwolf Tower" },
            { -1402.0f, -307.0f, "Frostwolf Relief Hut" } };
        return team == 2 ? horde : alliance;
    }

    struct AvNodeView { float x, y; bool ours; bool destroyed; bool enemy_flipping; };

    // Indices into `nodes` of the first `window` list entries that are still worth attacking: present, not razed, not ours, and not
    // being flipped by the enemy (the defence pass owns those). Matches a list entry to a live node by position (30 yd).
    inline std::vector<int> PickAttackNodes(uint8_t team, std::vector<AvNodeView> const& nodes, int window)
    {
        std::vector<int> out;
        for (AvListNode const& e : AttackList(team))
        {
            if (int(out.size()) >= window) break;
            for (int i = 0; i < int(nodes.size()); ++i)
            {
                float const dx = nodes[i].x - e.x, dy = nodes[i].y - e.y;
                if (dx * dx + dy * dy > 30.0f * 30.0f) continue;
                if (nodes[i].destroyed || nodes[i].ours || nodes[i].enemy_flipping) break;
                out.push_back(i);
                break;
            }
        }
        return out;
    }

    // Snowfall Graveyard (the neutral one in the middle of the valley, -202.5,-113): ike3 sends its "supporters" there while it is not
    // ours, 40 % of the team in the first ten minutes ("strife") and 20 % afterwards; mod-playerbots sends roles under 6 (Alliance) / 5
    // (Horde) of 10. Returns how many of `free_bots` go.
    constexpr float kSnowfallX = -202.5f, kSnowfallY = -113.0f;
    inline int SnowfallDetail(int free_bots, uint32_t in_progress_ms)
    {
        if (free_bots < 3) return 0;
        int const share = in_progress_ms < 10u * 60u * 1000u ? 40 : 20;
        int const n = (free_bots * share) / 100;
        return n < 1 ? 1 : n;
    }

    // ---- Rules from ike3 / mod-playerbots AV that the planner uses (Karrz1/wow-1.12 raid10-revamp BattleGroundTacticsAV.cpp, mod-playerbots
    // BattleGroundTactics.cpp selectObjective) ----
    // A defender already deep in the enemy half becomes an attacker: x >= -62 for the Horde, x <= -462 for the Alliance (AV runs from the
    // Frostwolf end at x -1400 to Dun Baldar at x +700); with chance 20 + 15 per own node razed (mod-playerbots BGT 1919-1931).
    constexpr bool DefenderPastLine(uint8_t team, float x) { return team == 2 ? x >= -62.0f : x <= -462.0f; }
    constexpr int  DefenderFlipChance(int own_razed) { return 20 + 15 * own_razed; }

    // The general (Drek'Thar / Vanndar). ike3: only once every enemy tower and bunker is down. mod-playerbots: two or more down, and either
    // the team holds its last graveyard (Frostwolf Relief Hut / Stormpike Aid Station) or 20 or more of its players stand within 200 yd of
    // the boss. Either one opens the push.
    constexpr int kBossFriendsNeeded = 20;
    constexpr bool GeneralPushOk(int enemy_towers_up, int enemy_towers_razed, bool own_final_gy, int friends_near_boss)
    {
        return enemy_towers_up == 0 || (enemy_towers_razed >= 2 && (own_final_gy || friends_near_boss >= kBossFriendsNeeded));
    }
    // Our last graveyard (the one an AV team keeps until the end).
    struct AvPoint { float x, y, z; };
    constexpr AvPoint FinalGraveyard(uint8_t team) { return team == 2 ? AvPoint{ -1402.0f, -307.0f, 89.0f } : AvPoint{ 638.5f, -31.5f, 46.0f }; }

    // Nothing left to attack: wait next to the enemy general (mod-playerbots AV_BOSS_WAIT_H / AV_BOSS_WAIT_A).
    constexpr AvPoint BossWait(uint8_t team) { return team == 2 ? AvPoint{ 689.21f, -20.173f, 50.62f } : AvPoint{ -1361.773f, -248.977f, 99.369f }; }

    // How far an AV bot looks for a fight (ike3 EnemyPlayerValue::GetMaxAttackDistance): 40 yd during the first 20 minutes, 10 yd after, so
    // late in the match the bots keep running to the objective instead of chasing every enemy in sight.
    constexpr float AggroRange(uint32_t in_progress_ms) { return in_progress_ms < 20u * 60u * 1000u ? 40.0f : 10.0f; }
}
