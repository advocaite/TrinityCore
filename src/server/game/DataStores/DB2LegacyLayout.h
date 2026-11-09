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

#ifndef DB2LegacyLayout_h__
#define DB2LegacyLayout_h__

#include "Define.h"
#include <cstddef>
#include <string>
#include <vector>

// Client 1.60.1.70707 added a column to SpellDuration.db2 (DurationPerLevel, between Duration and MaxDuration). A server whose game tables come
// from an older client (70685 and before) has the old layout. The server reads the 70707 layout, so an old SpellDuration.db2 is rewritten into
// the new layout (DurationPerLevel = 0, which is what every older client implied); the old file is kept next to it as SpellDuration.db2.legacy-layout.
namespace DB2LegacyLayout
{
    constexpr uint32 SpellDurationLegacyHash = 0xA931BD2B;
    constexpr uint32 SpellDurationHash = 0x4FDF0951;

    struct SpellDurationRow
    {
        uint32 Id;
        int32 Duration;
        int32 DurationPerLevel;
        int32 MaxDuration;
        int32 DurationPerResource;
    };

    // The bytes of a SpellDuration.db2 in the 70707 layout (all columns stored as plain 32 bit numbers) or, with legacyLayout, in the old
    // layout (no DurationPerLevel column); rows must be in ascending id order. Used by the upgrade below and by the tests.
    TC_GAME_API std::vector<uint8> BuildSpellDurationFile(std::vector<SpellDurationRow> const& rows, uint32 tableHash, bool legacyLayout);

    // Rewrites path when it is an old-layout SpellDuration.db2. Returns true when the file was rewritten, false when it needed nothing
    // (new layout, not found, or not readable); errors are logged and leave the file as it was.
    TC_GAME_API bool UpgradeSpellDurationFile(std::string const& path);

    // Runs UpgradeSpellDurationFile on SpellDuration.db2 in every locale folder of db2Path (the "dbc/" folder). Returns how many were rewritten.
    TC_GAME_API uint32 UpgradeLegacyDB2Files(std::string const& db2Path);
}

#endif // DB2LegacyLayout_h__
