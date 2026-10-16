// Per-healer mana ledger of the current raid pull, printed as [mana_ledger] lines at the pull end next to the "healers that cast" line.
// What each healer paid per spell (the core's own cost, taken when the cast completes), what the heals it sent were estimated at against
// what landed (overheal), the mana that came back in and out of the five second rule (Classic 1.60: spirit regeneration stops for 5 s
// after mana is paid, ClassicManaRegen.h) and the potions, runes and Innervate used.
//
// Written from the world thread by the core hooks (OnSpellCast, OnHeal) and the raid brain's sampler; the heal emitter adds its estimate
// from the bot workers, so the table is locked.
#pragma once

#include "Define.h"
#include <algorithm>
#include <map>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace Playerbot::Raid {

namespace detail {

struct ManaSpellRow { uint32 casts = 0; int64 mana = 0; };
struct ManaRestoreRow { uint32 uses = 0; int64 nominal = 0; };

struct ManaEntry
{
    std::string name;
    uint32 max_mana = 0;
    uint32 first_mana = 0;
    uint32 min_mana = 0;
    uint32 last_mana = 0;
    uint32 last_sample_ms = 0;
    uint32 last_spend_ms = 0;        // game ms of the last paid cast
    bool   sampled = false;
    int64  pending_spent = 0;        // paid since the last sample
    int64  pending_restore = 0;      // nominal item / spell restores since the last sample
    int64  regen_in = 0;             // mana gained while the five second rule ran
    int64  regen_out = 0;            // mana gained outside it
    uint32 ms_in = 0;
    uint32 ms_out = 0;
    int64  heal_est = 0;             // sum of the estimates of the heals sent
    uint32 heals_sent = 0;
    int64  heal_effective = 0;       // healing that landed (OnHeal, effective gain only)
    std::map<uint32, ManaSpellRow> spells;
    std::map<uint32, ManaRestoreRow> restores;
};

inline std::mutex g_manaMu;
inline std::unordered_map<uint64, ManaEntry> g_mana;
inline std::unordered_map<uint64, bool> g_manaFreeSeen;   // healers whose free cast was already explained in this pull

} // namespace detail

// A group member that can heal is watched from now on (the raid brain calls this every tick of a pull).
// <cur>/<max> are its mana, <now_ms> the game time in ms.
inline void ManaLedgerSample(uint64 guid_low, std::string const& name, uint32 cur, uint32 max, uint32 now_ms)
{
    if (max == 0)
        return;
    std::lock_guard<std::mutex> lk(detail::g_manaMu);
    auto& e = detail::g_mana[guid_low];
    if (!e.sampled)
    {
        e.name = name;
        e.max_mana = max;
        e.first_mana = e.min_mana = e.last_mana = cur;
        e.last_sample_ms = now_ms;
        e.sampled = true;
        e.pending_spent = e.pending_restore = 0;
        return;
    }
    uint32 const dt = now_ms - e.last_sample_ms;
    int64 const gained = std::max<int64>(0, int64(cur) - int64(e.last_mana) + e.pending_spent - e.pending_restore);
    bool const inside = now_ms - e.last_spend_ms < 5000u;
    if (inside) { e.regen_in += gained; e.ms_in += dt; }
    else        { e.regen_out += gained; e.ms_out += dt; }
    e.min_mana = std::min(e.min_mana, cur);
    e.last_mana = cur;
    e.last_sample_ms = now_ms;
    e.pending_spent = e.pending_restore = 0;
}

// One spell of a watched healer completed and cost <cost> mana (0 for a free spell).
inline void ManaLedgerCast(uint64 guid_low, uint32 spell, int32 cost, uint32 now_ms)
{
    std::lock_guard<std::mutex> lk(detail::g_manaMu);
    auto it = detail::g_mana.find(guid_low);
    if (it == detail::g_mana.end() || !it->second.sampled)
        return;
    auto& row = it->second.spells[spell];
    ++row.casts;
    if (cost > 0)
    {
        row.mana += cost;
        it->second.pending_spent += cost;
        it->second.last_spend_ms = now_ms;
    }
}

// A potion, rune or Innervate (<spell>) restored about <nominal> mana to a watched healer.
inline void ManaLedgerRestore(uint64 guid_low, uint32 spell, int32 nominal)
{
    std::lock_guard<std::mutex> lk(detail::g_manaMu);
    auto it = detail::g_mana.find(guid_low);
    if (it == detail::g_mana.end() || !it->second.sampled)
        return;
    auto& row = it->second.restores[spell];
    ++row.uses;
    row.nominal += nominal;
    it->second.pending_restore += nominal;
}

// The heal emitter sent a heal it estimated at <est>.
inline void ManaLedgerHealSent(uint64 guid_low, float est)
{
    std::lock_guard<std::mutex> lk(detail::g_manaMu);
    auto it = detail::g_mana.find(guid_low);
    if (it == detail::g_mana.end() || !it->second.sampled)
        return;
    ++it->second.heals_sent;
    it->second.heal_est += int64(est);
}

// A heal of a watched healer landed for <gain> (effective, overheal already removed by the core).
inline void ManaLedgerHealLanded(uint64 guid_low, uint32 gain)
{
    std::lock_guard<std::mutex> lk(detail::g_manaMu);
    auto it = detail::g_mana.find(guid_low);
    if (it == detail::g_mana.end() || !it->second.sampled)
        return;
    it->second.heal_effective += gain;
}

inline bool ManaLedgerWatching(uint64 guid_low)
{
    std::lock_guard<std::mutex> lk(detail::g_manaMu);
    auto it = detail::g_mana.find(guid_low);
    return it != detail::g_mana.end() && it->second.sampled;
}

inline void ManaLedgerReset()
{
    std::lock_guard<std::mutex> lk(detail::g_manaMu);
    detail::g_mana.clear();
    detail::g_manaFreeSeen.clear();
}

// True the first time per pull for a healer: the caller then logs why a spell that has a mana cost was charged nothing.
inline bool ManaLedgerFirstFree(uint64 guid_low)
{
    std::lock_guard<std::mutex> lk(detail::g_manaMu);
    return detail::g_manaFreeSeen.try_emplace(guid_low, true).second;
}

// One line per healer that sent a heal, cleared on return. <spell_name> turns a spell id into text.
template <typename NameFn>
inline std::vector<std::string> ManaLedgerTake(uint32 duration_s, NameFn&& spell_name)
{
    std::lock_guard<std::mutex> lk(detail::g_manaMu);
    std::vector<detail::ManaEntry const*> rows;
    for (auto const& [g, e] : detail::g_mana)
        if (e.heals_sent > 0 || !e.spells.empty())
            rows.push_back(&e);
    std::sort(rows.begin(), rows.end(), [](detail::ManaEntry const* a, detail::ManaEntry const* b) { return a->name < b->name; });
    std::vector<std::string> out;
    uint32 const secs = std::max<uint32>(duration_s, 1u);
    for (detail::ManaEntry const* e : rows)
    {
        int64 spent = 0;
        for (auto const& [id, r] : e->spells)
            spent += r.mana;
        int64 restored = 0;
        for (auto const& [id, r] : e->restores)
            restored += r.nominal;
        int const over = e->heal_est > 0 ? int(100 - std::min<int64>(100, e->heal_effective * 100 / e->heal_est)) : 0;
        std::string line = e->name + " mana " + std::to_string(e->first_mana * 100u / e->max_mana) + "%->" +
            std::to_string(e->last_mana * 100u / e->max_mana) + "% (low " + std::to_string(e->min_mana * 100u / e->max_mana) +
            "%) spent " + std::to_string(spent) + " (" + std::to_string(spent / secs) + "/s) heals " + std::to_string(e->heals_sent) +
            " est " + std::to_string(e->heal_est) + " landed " + std::to_string(e->heal_effective) + " overheal~" + std::to_string(over) +
            "% | regen in 5sr " + std::to_string(e->regen_in) + " over " + std::to_string(e->ms_in / 1000u) + "s, out " +
            std::to_string(e->regen_out) + " over " + std::to_string(e->ms_out / 1000u) + "s | items+innervate " +
            std::to_string(restored) + " |";
        for (auto const& [id, r] : e->spells)
            line += " " + spell_name(id) + " x" + std::to_string(r.casts) + "=" + std::to_string(r.mana);
        for (auto const& [id, r] : e->restores)
            line += " +" + spell_name(id) + " x" + std::to_string(r.uses) + "=" + std::to_string(r.nominal);
        out.push_back(std::move(line));
    }
    detail::g_mana.clear();
    return out;
}

} // namespace Playerbot::Raid
