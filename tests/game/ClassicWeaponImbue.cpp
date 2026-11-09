#include "tc_catch2.h"
#include "ClassicWeaponImbue.h"

TEST_CASE("Every Flametongue Weapon enchantment casts its own rank's proc", "[ClassicWeaponImbue]")
{
    // enchantment ids from the imbue spells' enchant effects (8024, 8027, 8030, 16339, 16341, 16342, 461634) in 70532
    CHECK(ClassicWeaponImbue::FlametongueProcForEnchant(5) == 8026);
    CHECK(ClassicWeaponImbue::FlametongueProcForEnchant(4) == 8028);
    CHECK(ClassicWeaponImbue::FlametongueProcForEnchant(3) == 8029);
    CHECK(ClassicWeaponImbue::FlametongueProcForEnchant(523) == 10445);
    CHECK(ClassicWeaponImbue::FlametongueProcForEnchant(1665) == 16343);
    CHECK(ClassicWeaponImbue::FlametongueProcForEnchant(1666) == 16344);
    CHECK(ClassicWeaponImbue::FlametongueProcForEnchant(7567) == 16344);
}

TEST_CASE("Windfury and other enchantments are not Flametongue", "[ClassicWeaponImbue]")
{
    for (uint32 enchantId : { 0u, 283u, 284u, 525u, 1669u, 7569u, 8293u, 2506u })
        CHECK(ClassicWeaponImbue::FlametongueProcForEnchant(enchantId) == 0);
}

TEST_CASE("Every Windfury Weapon enchantment grants its own rank's attack", "[ClassicWeaponImbue]")
{
    CHECK(ClassicWeaponImbue::WindfuryAttackForEnchant(283) == 8233);
    CHECK(ClassicWeaponImbue::WindfuryAttackForEnchant(284) == 8236);
    CHECK(ClassicWeaponImbue::WindfuryAttackForEnchant(525) == 10484);
    CHECK(ClassicWeaponImbue::WindfuryAttackForEnchant(1669) == 16361);
    for (uint32 enchantId : { 0u, 5u, 4u, 3u, 523u, 1665u, 1666u, 7567u })
        CHECK(ClassicWeaponImbue::WindfuryAttackForEnchant(enchantId) == 0);
}
