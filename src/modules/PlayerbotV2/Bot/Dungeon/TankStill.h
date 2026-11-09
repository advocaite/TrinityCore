// TankStill - when a bot tank in a dungeon or raid stands in one place out of combat, say why, once every 30 s ([tank_still] in
// Playerbot.log). The advance gate's own diagnostic ([tank_advance] blocked) names the first member that holds the pull and only
// when that gate is the one holding; a tank can also stand for a rule that never reaches the gate (a drink, a pull-back wait, a
// walk that leads nowhere). This watches the tank's position instead and lists everything about the group a tank would wait for:
// its own health and mana, every member that is dead, outside the instance, far, hurt or low on mana, and the last rule that held it.
//
// Pure logic on plain values, tested in tests/game/ClassicTankStill.cpp. The caller (BotAI::dispatch_layers) fills the inputs from
// the snapshot and writes the line.

#pragma once

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace Playerbot::TankStill {

constexpr float kStillYards = 2.0f;             // less than this from where it stopped is still standing
constexpr std::uint32_t kNoteEveryMs = 30000u;  // the first line after 30 s standing, then one every 30 s

// The marks a tank waits for, the same as the dungeon advance gate (State_Idle compute_group_ready_base, normal zone).
struct Limits
{
    int low_hp_pct = 70;            // a member under this is waited for to be healed
    float healer_far_yards = 40.f;  // a healer beyond this is too far to heal the pull
    float other_far_yards = 50.f;   // any other member beyond this has fallen behind
    int healer_mana_pct = 80;       // Classic::PullReadyHealerManaPct
    int other_mana_pct = 60;        // Classic::kPullReadyDpsManaPct
};

struct Self
{
    int hp_pct = 100;
    int mana_pct = -1;  // -1: no mana (warrior, bear)
};

struct Member
{
    std::string name;
    bool online = true;
    bool alive = true;
    bool ghost = false;        // released and running back
    bool other_map = false;    // not inside this instance
    bool healer = false;
    float yards = 0.f;         // from the tank
    int hp_pct = 100;
    int mana_pct = -1;         // -1: no mana
};

struct Watch
{
    std::uint32_t map = 0;
    float x = 0.f, y = 0.f, z = 0.f;  // where it stopped
    std::uint32_t since_ms = 0;       // since when it stands there; 0 = not watching
    std::uint32_t noted_ms = 0;       // the last line
};

// Call every tick. `watching` = a living tank in an instance, out of combat, its group's dungeon run on. Returns how long it has
// stood (ms) when a line is due, else 0.
inline std::uint32_t Due(Watch& w, bool watching, std::uint32_t map, float x, float y, float z, std::uint32_t now_ms)
{
    if (!watching)
    {
        w.since_ms = 0;
        return 0;
    }
    float const dx = x - w.x, dy = y - w.y, dz = z - w.z;
    if (w.since_ms == 0 || map != w.map || dx * dx + dy * dy + dz * dz >= kStillYards * kStillYards)
    {
        w.map = map;
        w.x = x; w.y = y; w.z = z;
        w.since_ms = now_ms ? now_ms : 1u;
        w.noted_ms = w.since_ms;
        return 0;
    }
    if (now_ms - w.noted_ms < kNoteEveryMs)
        return 0;
    w.noted_ms = now_ms;
    return now_ms - w.since_ms;
}

// Everything about the tank and its group that a tank waits for, comma separated; "" when nothing is wrong.
inline std::string Reasons(Self const& self, std::vector<Member> const& members, Limits const& lim)
{
    std::string out;
    auto add = [&out](std::string const& part)
    {
        if (!out.empty())
            out += ", ";
        out += part;
    };
    char buf[96];
    if (self.hp_pct < lim.low_hp_pct)
    {
        std::snprintf(buf, sizeof(buf), "own health %d%%", self.hp_pct);
        add(buf);
    }
    if (self.mana_pct >= 0 && self.mana_pct < lim.other_mana_pct)
    {
        std::snprintf(buf, sizeof(buf), "own mana %d%%", self.mana_pct);
        add(buf);
    }
    for (Member const& m : members)
    {
        if (!m.online)
            continue;
        if (!m.alive)
        {
            add(m.name + (m.ghost ? " dead (running back)" : " dead"));
            continue;
        }
        if (m.other_map)
        {
            add(m.name + " outside the instance");
            continue;
        }
        float const far = m.healer ? lim.healer_far_yards : lim.other_far_yards;
        if (m.yards > far)
        {
            std::snprintf(buf, sizeof(buf), "%s far %.0f y", m.healer ? " (healer)" : "", m.yards);
            add(m.name + buf);
        }
        if (m.hp_pct < lim.low_hp_pct)
        {
            std::snprintf(buf, sizeof(buf), " health %d%%", m.hp_pct);
            add(m.name + buf);
        }
        int const mark = m.healer ? lim.healer_mana_pct : lim.other_mana_pct;
        if (m.mana_pct >= 0 && m.mana_pct < mark)
        {
            std::snprintf(buf, sizeof(buf), " %smana %d%%", m.healer ? "(healer) " : "", m.mana_pct);
            add(m.name + buf);
        }
    }
    return out;
}

}  // namespace Playerbot::TankStill
