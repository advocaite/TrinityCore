#include "tc_catch2.h"
#include "SpellInfo.h"

// Classic 1.60 Revive (437138, also 1237948-1237951) and Reintegration (430318) resurrect through TARGET_CORPSE_TARGET_ALLY and,
// unlike the other resurrection spells, have no SpellTargetRestrictions row carrying TARGET_FLAG_CORPSE_ALLY. The target type alone
// must ask for a corpse (or an unreleased dead player): with an empty explicit mask the cast dropped its target and the finished cast
// hit the ASSERT in Spell::SelectImplicitTargetObjectTargets (player report 2026-11-12, worldserver crash on every Revive).
TEST_CASE("TARGET_CORPSE_TARGET_ALLY asks for an explicit ally corpse", "[CorpseTargetExplicitMask]")
{
    bool srcSet = false;
    bool dstSet = false;
    CHECK(SpellImplicitTargetInfo(TARGET_CORPSE_TARGET_ALLY).GetExplicitTargetMask(srcSet, dstSet) == TARGET_FLAG_CORPSE_ALLY);
    CHECK_FALSE(srcSet);
    CHECK_FALSE(dstSet);
}

TEST_CASE("A corpse area around the caster still needs no explicit target", "[CorpseTargetExplicitMask]")
{
    bool srcSet = false;
    bool dstSet = false;
    CHECK(SpellImplicitTargetInfo(TARGET_CORPSE_SRC_AREA_RAID).GetExplicitTargetMask(srcSet, dstSet) == 0);
}
