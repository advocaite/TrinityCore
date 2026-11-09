/* This file is part of TrinityCore; licensed under GPL version 2 or later. */
#ifndef TRINITY_CLASSIC_WEAPON_IMBUE_H
#define TRINITY_CLASSIC_WEAPON_IMBUE_H

#include "Define.h"

// Shaman weapon imbues. Forever points the enchantment of every Windfury and Flametongue Weapon rank at one dummy aura
// (Windfury Weapon 439431, Flametongue Weapon 436519) that nothing scripts, so Player::CastItemCombatSpell picks the rank's
// own spell from the enchantment id (client SpellItemEnchantment and the imbue spells' enchant effects, 70532).
namespace ClassicWeaponImbue
{
// Windfury Attack the rank grants on a 20% melee hit
constexpr uint32 WindfuryAttackForEnchant(uint32 enchantId)
{
    switch (enchantId)
    {
        case 283:  return 8233;     // Windfury Weapon rank 1 (spell 8232)
        case 284:  return 8236;     // rank 2 (8235)
        case 525:  return 10484;    // rank 3 (10486)
        case 1669: return 16361;    // rank 4 (16362)
        default:   return 0;
    }
}

// Flametongue Weapon Proc cast on the target of every hit; its dummy (classic_spell_sha_flametongue_proc) deals the Fire
// damage by the weapon's speed
constexpr uint32 FlametongueProcForEnchant(uint32 enchantId)
{
    switch (enchantId)
    {
        case 5:    return 8026;     // Flametongue Weapon rank 1 (spell 8024)
        case 4:    return 8028;     // rank 2 (8027)
        case 3:    return 8029;     // rank 3 (8030)
        case 523:  return 10445;    // rank 4 (16339)
        case 1665: return 16343;    // rank 5 (16341)
        case 1666:                  // rank 6 (16342)
        case 7567: return 16344;    // Forever's own rank 6 enchantment (spell 461634)
        default:   return 0;
    }
}
}

#endif
