#include "tc_catch2.h"

#include "../../src/server/game/Spells/ClassicAuraCaster.h"

TEST_CASE("A bomb's explosion names the creature that cast the bomb, while it is still there", "[ClassicAuraCaster]")
{
    CHECK(ClassicAuraCaster::UseAuraCaster(true, true, false));       // Geddon alive, a player carries the bomb: Geddon is the original caster
}

TEST_CASE("Without the aura's caster the explosion falls back to the holder", "[ClassicAuraCaster]")
{
    CHECK_FALSE(ClassicAuraCaster::UseAuraCaster(false, false, false));    // caster gone (despawned after a wipe)
    CHECK_FALSE(ClassicAuraCaster::UseAuraCaster(true, false, false));     // caster pointer exists but is not in the world
}

TEST_CASE("A self-cast aura needs no original caster", "[ClassicAuraCaster]")
{
    CHECK_FALSE(ClassicAuraCaster::UseAuraCaster(true, true, true));
}
