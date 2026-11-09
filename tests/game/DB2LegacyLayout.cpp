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

#include "tc_catch2.h"
#include "DB2FileLoader.h"
#include "DB2FileSystemSource.h"
#include "DB2LegacyLayout.h"
#include "DB2LoadInfo.h"
#include <filesystem>
#include <fstream>

namespace
{
using DB2LegacyLayout::SpellDurationRow;

std::vector<SpellDurationRow> SampleRows()
{
    return {
        { 1, 10000, 0, 10000, 0 },
        { 2, 30000, 0, 30000, 0 },
        { 9, 10000, 0, 15000, 5 },
        { 28, -1, 0, 5000, 0 },
        { 1615, 600000, 0, 900000, 3 },
    };
}

void WriteFile(std::filesystem::path const& path, std::vector<uint8> const& bytes)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<char const*>(bytes.data()), std::streamsize(bytes.size()));
}

std::vector<uint8> ReadFile(std::filesystem::path const& path)
{
    std::ifstream in(path, std::ios::binary);
    return std::vector<uint8>(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}
}

// The 70707 client added DurationPerLevel to SpellDuration.db2; a server whose tables come from a 70685 client (or older) must still start.
TEST_CASE("DB2LegacyLayout: an old-layout SpellDuration.db2 is converted and then loads", "[DB2LegacyLayout]")
{
    std::filesystem::path const dir = std::filesystem::temp_directory_path() / "tc-db2-legacy-layout-test";
    std::filesystem::remove_all(dir);
    std::filesystem::path const file = dir / "enUS" / "SpellDuration.db2";
    std::filesystem::path const german = dir / "deDE" / "SpellDuration.db2";
    std::vector<SpellDurationRow> const rows = SampleRows();
    WriteFile(file, DB2LegacyLayout::BuildSpellDurationFile(rows, 0xB423FA7A, true));
    WriteFile(german, DB2LegacyLayout::BuildSpellDurationFile(rows, 0xB423FA7A, true));

    {
        // the server reads only the new layout: the old file is refused as it is
        DB2FileSystemSource source(file.string());
        DB2FileLoader loader;
        CHECK_THROWS_AS(loader.Load(&source, &SpellDurationLoadInfo::Instance), DB2FileLoadException);
    }

    CHECK(DB2LegacyLayout::UpgradeLegacyDB2Files(dir.string()) == 2);
    CHECK(std::filesystem::exists(dir / "enUS" / "SpellDuration.db2.legacy-layout"));

    for (std::filesystem::path const& p : { file, german })
    {
        DB2FileSystemSource source(p.string());
        DB2FileLoader loader;
        REQUIRE_NOTHROW(loader.Load(&source, &SpellDurationLoadInfo::Instance));
        REQUIRE(loader.GetRecordCount() == rows.size());
        for (uint32 i = 0; i < rows.size(); ++i)
        {
            DB2Record record = loader.GetRecord(i);
            CHECK(record.GetId() == rows[i].Id);
            CHECK(record.GetInt32(0u, 0u) == rows[i].Duration);
            CHECK(record.GetInt32(1u, 0u) == 0);   // DurationPerLevel: older clients had none
            CHECK(record.GetInt32(2u, 0u) == rows[i].MaxDuration);
            CHECK(record.GetInt32(3u, 0u) == rows[i].DurationPerResource);
        }
    }

    // nothing left to do the second time
    CHECK(DB2LegacyLayout::UpgradeLegacyDB2Files(dir.string()) == 0);

    std::filesystem::remove_all(dir);
}

TEST_CASE("DB2LegacyLayout: a 70707-layout SpellDuration.db2 is left alone", "[DB2LegacyLayout]")
{
    std::filesystem::path const dir = std::filesystem::temp_directory_path() / "tc-db2-legacy-layout-test2";
    std::filesystem::remove_all(dir);
    std::filesystem::path const file = dir / "enUS" / "SpellDuration.db2";
    std::vector<uint8> const bytes = DB2LegacyLayout::BuildSpellDurationFile(SampleRows(), 0xB423FA7A, false);
    WriteFile(file, bytes);

    CHECK(DB2LegacyLayout::UpgradeLegacyDB2Files(dir.string()) == 0);
    CHECK(ReadFile(file) == bytes);
    CHECK(!std::filesystem::exists(dir / "enUS" / "SpellDuration.db2.legacy-layout"));

    std::filesystem::remove_all(dir);
}
