/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PLAYERBOT_V2_FLEET_WORLD_PVP_H
#define PLAYERBOT_V2_FLEET_WORLD_PVP_H

// World PvP of the world bots on a PvP or free-for-all realm (Karrz 2026-11-03: "some of the bots are marked as PvP, and some
// ... have got flagged PvP off ... a wee bit of randomness ... I'm walking down and all of a sudden I get cheap-shotted by a
// rogue that's been hiding, or a hunter will suddenly sting you"; free-for-all "a wee bit of hardcore ... bots fight each other,
// you fight the bots").
//
// Every world bot has a temper, the same for the bot every time (a hash of its id):
//   flagged   it keeps its PvP flag on (as a player who typed /pvp), so enemy players can attack it in its own zones too.
//             The rest leave the flag to the realm rules (on in hostile territory, off at home).
//   hunter    a flagged bot that sometimes starts a fight itself: for each enemy player (or enemy bot) it sees, one roll every
//             RollMinutes; on a hit it attacks. Any class can be a hunter, and each opens its own way (PvpOpeners below): a warrior
//             Charges, a mage opens with Frostbolt, a warlock Corruption, a priest Shadow Word: Pain, a druid Moonfire, a paladin
//             Judgement or Hammer of Justice, a shaman Earth Shock or Lightning Bolt, a hunter Serpent Sting, a rogue walks in
//             from stealth for Cheap Shot / Ambush. Everyone else never starts a fight with a player; it fights back.
// Normal realms (GameType 0 / 6): nothing here applies. Off: Playerbot.World.Pvp.Enable = 0.
// Pure (no server types), so the tests can check every rule.

#include <cstdint>

namespace Playerbot::V2::Fleet
{
    // GameType (worldserver.conf, the launcher's Realm type): 1 PvP, 8 RP-PvP, 16 free-for-all PvP; anything else is not a PvP realm.
    enum class RealmPvp : uint8_t { None, PvP, Ffa };

    inline RealmPvp RealmPvpOf(uint32_t game_type)
    {
        if (game_type == 16) return RealmPvp::Ffa;
        if (game_type == 1 || game_type == 8) return RealmPvp::PvP;
        return RealmPvp::None;
    }

    struct WorldPvpCfg
    {
        bool     enable = true;          // Playerbot.World.Pvp.Enable
        uint32_t flagged_pct = 50;       // Playerbot.World.Pvp.FlaggedPct      share of world bots that keep the PvP flag on
        uint32_t hunter_pct = 30;        // Playerbot.World.Pvp.HunterPct       share of world bots that may start a fight (all flagged)
        uint32_t attack_pct = 40;        // Playerbot.World.Pvp.AttackPct       chance per enemy and roll that a hunter attacks it
        uint32_t ffa_hunter_pct = 60;    // Playerbot.World.Pvp.FfaHunterPct    the same two on a free-for-all realm
        uint32_t ffa_attack_pct = 60;    // Playerbot.World.Pvp.FfaAttackPct
        uint32_t roll_minutes = 10;      // Playerbot.World.Pvp.RollMinutes     one roll per bot and enemy in this time
        uint32_t levels_below = 5;       // Playerbot.World.Pvp.LevelsBelow     a hunter picks enemies at most this many levels under it
        uint32_t levels_above = 2;       // Playerbot.World.Pvp.LevelsAbove     ... and at most this many over it
        bool     attack_bots = true;     // Playerbot.World.Pvp.AttackBots      hunters also jump enemy world bots, not only real players
        uint32_t border_friendly_pct = 85;      // Playerbot.World.Pvp.BorderFriendlyPct     on a PvP realm, the share of the player's
        uint32_t contested_friendly_pct = 70;   // Playerbot.World.Pvp.ContestedFriendlyPct  own faction at most (so enemies are about)
    };

    // A 0..99 number from the bot (and a salt): the same every time for the same bot.
    inline uint32_t PvpRoll(uint64_t bot, uint64_t salt)
    {
        uint64_t x = bot * 0x9E3779B97F4A7C15ull ^ (salt + 0x632BE59BD9B4E019ull + (bot << 6) + (bot >> 2));
        x ^= x >> 33; x *= 0xFF51AFD7ED558CCDull; x ^= x >> 33; x *= 0xC4CEB9FE1A85EC53ull; x ^= x >> 33;
        return uint32_t(x % 100u);
    }

    inline uint32_t HunterPct(WorldPvpCfg const& c, RealmPvp r) { return r == RealmPvp::Ffa ? c.ffa_hunter_pct : c.hunter_pct; }
    inline uint32_t AttackPct(WorldPvpCfg const& c, RealmPvp r) { return r == RealmPvp::Ffa ? c.ffa_attack_pct : c.attack_pct; }

    // May start fights. Only on a PvP or free-for-all realm with the feature on.
    inline bool IsPvpHunter(WorldPvpCfg const& c, RealmPvp r, uint64_t bot)
    {
        return c.enable && r != RealmPvp::None && PvpRoll(bot, 0x68756E74ull) < HunterPct(c, r);
    }

    // Keeps the PvP flag on: every hunter, and FlaggedPct of all world bots (at least the hunters).
    inline bool IsPvpFlagged(WorldPvpCfg const& c, RealmPvp r, uint64_t bot)
    {
        if (!c.enable || r == RealmPvp::None) return false;
        return IsPvpHunter(c, r, bot) || PvpRoll(bot, 0x666C6167ull) < c.flagged_pct;
    }

    // The roll of one hunter against one enemy in the current window (now_s = game time in seconds): the same answer for the
    // whole window, a new one in the next. A hunter that passed an enemy by does not change its mind every tick.
    inline bool AttackRoll(WorldPvpCfg const& c, RealmPvp r, uint64_t bot, uint64_t enemy, uint64_t now_s)
    {
        uint64_t const window = uint64_t(c.roll_minutes ? c.roll_minutes : 1u) * 60u;
        return PvpRoll(bot, enemy * 1000003ull + now_s / window) < AttackPct(c, r);
    }

    inline bool LevelOk(WorldPvpCfg const& c, uint32_t bot_level, uint32_t enemy_level)
    {
        return enemy_level + c.levels_below >= bot_level && enemy_level <= bot_level + c.levels_above;
    }

    // The first hit of an ambush, by class (classic class ids): tried in order, the first one the bot knows, has ready and is in
    // range for is cast at the enemy (the server casts the highest rank the bot knows). None fits: the bot just attacks (walks or
    // runs in and its normal fight takes over). A rogue has none on purpose: its own fight stealths and opens with Cheap Shot or
    // Ambush on a target nobody fights yet. A druid in cat or bear form has none either (no caster spells in form).
    struct PvpOpener { uint32_t spell; float min_yards; float max_yards; bool needs_seal; };

    inline int PvpOpeners(uint8_t cls, bool druid_in_form, PvpOpener const*& out)
    {
        static constexpr PvpOpener kWarrior[] = { { 100, 8.f, 25.f, false } };                                    // Charge
        static constexpr PvpOpener kPaladin[] = { { 20271, 0.f, 10.f, true }, { 853, 0.f, 10.f, false } };        // Judgement, Hammer of Justice
        static constexpr PvpOpener kHunter[]  = { { 1978, 8.f, 35.f, false } };                                   // Serpent Sting
        static constexpr PvpOpener kPriest[]  = { { 589, 0.f, 30.f, false }, { 585, 0.f, 30.f, false } };         // Shadow Word: Pain, Smite
        static constexpr PvpOpener kShaman[]  = { { 8042, 0.f, 20.f, false }, { 403, 0.f, 30.f, false } };        // Earth Shock, Lightning Bolt
        static constexpr PvpOpener kMage[]    = { { 116, 0.f, 30.f, false }, { 133, 0.f, 35.f, false } };         // Frostbolt, Fireball
        static constexpr PvpOpener kWarlock[] = { { 172, 0.f, 30.f, false }, { 686, 0.f, 30.f, false } };         // Corruption, Shadow Bolt
        static constexpr PvpOpener kDruid[]   = { { 8921, 0.f, 30.f, false }, { 5176, 0.f, 30.f, false } };       // Moonfire, Wrath
        switch (cls)
        {
            case 1:  out = kWarrior; return 1;
            case 2:  out = kPaladin; return 2;
            case 3:  out = kHunter;  return 1;
            case 5:  out = kPriest;  return 2;
            case 7:  out = kShaman;  return 2;
            case 8:  out = kMage;    return 2;
            case 9:  out = kWarlock; return 2;
            case 11: if (druid_in_form) break; out = kDruid; return 2;
            default: break;   // rogue (4) and anything else: no opener spell
        }
        out = nullptr;
        return 0;
    }

    // On a PvP realm (either kind) with the feature on: the border and contested shares of the player's own faction are at most the
    // PvP numbers, so enemy bots are about. Home zones and capitals stay as the realm files say.
    inline uint32_t PvpBorderPct(WorldPvpCfg const& c, RealmPvp r, uint32_t pct)
    {
        return c.enable && r != RealmPvp::None && c.border_friendly_pct < pct ? c.border_friendly_pct : pct;
    }
    inline uint32_t PvpContestedPct(WorldPvpCfg const& c, RealmPvp r, uint32_t pct)
    {
        return c.enable && r != RealmPvp::None && c.contested_friendly_pct < pct ? c.contested_friendly_pct : pct;
    }
} // namespace Playerbot::V2::Fleet

#endif
