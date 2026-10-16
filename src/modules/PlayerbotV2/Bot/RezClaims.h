// RezClaims.h - resurrection spells and the record of rez casts the core refused.
//
// A healer "claims" a corpse while the group snapshot shows it casting a rez on it. When the core refuses that cast
// (SPELL_FAILED_AFFECTING_COMBAT, MC Garr 2026-10-08: Polonata / Elengie / Conquer / Brutus on 7328, Eriannah / Midandric on 2006),
// the claim must stop counting at once, or every other rezzer waits on a cast that never ran. The intent executor notes the
// refusal here; the rez pickers (ClassicHeal::RezTarget, RaidOocRez) skip a claim whose caster failed on that corpse recently.
#pragma once

#include "Define.h"
#include "ObjectGuid.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Log.h"
#include <fmt/format.h>
#include "SpellFamily.h"
#include <mutex>
#include <unordered_map>

namespace Playerbot::Combat::ClassicHeal {

// A resurrection spell of any rank: Resurrection, Ancestral Spirit, Redemption, Rebirth (vanilla), Revive / Resuscitate / Return /
// Raise Ally (retail). Matched on the first rank of the chain so a rank-5 Resurrection dedupes like rank 1.
inline bool IsRezSpell(uint32 spell)
{
    if (!spell) return false;
    // Matched by family: Resurrection 2006 has no rank chain in the Classic data, so a rank 2+ id must still count.
    for (uint32 const rez : { 2006u, 2008u, 7328u, 20484u, 50769u, 115178u, 361227u, 61999u })
        if (SpellFamily::Same(spell, rez))
            return true;
    return false;
}

// [raid_rez] effects (playtest, MC 2026-10-10: the rez casts spend their mana and never offer anything, and neither the SPELL_EFFECT_RESURRECT /
// RESURRECT_WITH_AURA cast-state log nor the core's rez logs ever printed, so the Classic rez spells carry another effect): once per
// process, the effect ids and targets of every vanilla rez rank as the core loaded them. Called by the first rez cast and by the first raid
// rez evaluation, so a scenario shows it without a death.
inline void LogRezEffectsOnce()
{
    static std::once_flag once;
    std::call_once(once, []
    {
        for (uint32 const id : { 2006u, 2010u, 10880u, 10881u, 20770u, 2008u, 20609u, 20610u, 20776u, 20777u, 7328u, 10322u, 10324u, 20772u, 20773u })
        {
            SpellInfo const* si = sSpellMgr->GetSpellInfo(id, DIFFICULTY_NONE);
            if (!si)
            {
                TC_LOG_INFO("playerbot.v2", "[raid_rez] effects: spell {} is not loaded", id);
                continue;
            }
            std::string fx;
            for (SpellEffectInfo const& e : si->GetEffects())
                if (e.IsEffect())
                    fx += fmt::format(" [#{} effect {} targets {}/{} value {}]", e.EffectIndex, uint32(e.Effect), uint32(e.TargetA.GetTarget()),
                                      uint32(e.TargetB.GetTarget()), e.CalcBaseValue(nullptr, nullptr, 0, 0));
            TC_LOG_INFO("playerbot.v2", "[raid_rez] effects: spell {} '{}' target mask {:#x}:{}", id, si->SpellName ? si->SpellName->Str[0] : "",
                        si->Targets, fx);
        }
    });
}

namespace RezClaims {

constexpr uint32 kFailWindowMs = 6000u;

inline uint64 Key(ObjectGuid caster, ObjectGuid corpse) { return (caster.GetCounter() << 24) ^ corpse.GetCounter(); }

struct Store
{
    std::mutex mu;
    std::unordered_map<uint64, uint32> failed_ms;
};
inline Store& Get() { static Store s; return s; }

// The core refused a rez cast by `caster` on `corpse` at `now_ms`.
inline void NoteFailure(ObjectGuid caster, ObjectGuid corpse, uint32 now_ms)
{
    Store& st = Get();
    std::lock_guard<std::mutex> lk(st.mu);
    st.failed_ms[Key(caster, corpse)] = now_ms ? now_ms : 1u;
    if (st.failed_ms.size() > 512)   // a handful of casters x corpses per raid; drop stale entries
        for (auto it = st.failed_ms.begin(); it != st.failed_ms.end();)
            it = (uint32(now_ms - it->second) > 60000u) ? st.failed_ms.erase(it) : std::next(it);
}

// True when `caster`'s rez on `corpse` was refused within the last kFailWindowMs: its in-flight claim no longer counts.
inline bool RecentlyFailed(ObjectGuid caster, ObjectGuid corpse, uint32 now_ms)
{
    Store& st = Get();
    std::lock_guard<std::mutex> lk(st.mu);
    auto it = st.failed_ms.find(Key(caster, corpse));
    return it != st.failed_ms.end() && uint32(now_ms - it->second) < kFailWindowMs;
}

} // namespace RezClaims
} // namespace Playerbot::Combat::ClassicHeal
