// Who counts as the caster of a spell that an aura triggers on its holder (Classic 1.60). VMaNGOS casts such a spell from the holder but with the aura's
// caster as the original caster, so damage, spell power, crit and threat are the caster's: Baron Geddon's Living Bomb explosion (20476) is Geddon's flat
// 3200 whoever carries the bomb. TrinityCore's periodic-trigger code casts it from the holder with no original caster, so the holder's own spell power
// and crit scaled it (a mage carrier did 5539). Pure logic, no server types.
#pragma once

namespace ClassicAuraCaster
{
    // True when the triggered spell should name the aura's caster as its original caster: the caster still exists and is in the world, and is not
    // the holder itself (a self-cast aura needs no original caster).
    constexpr bool UseAuraCaster(bool casterExists, bool casterInWorld, bool casterIsHolder)
    {
        return casterExists && casterInWorld && !casterIsHolder;
    }
}
