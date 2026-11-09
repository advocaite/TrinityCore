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

#include "DB2LegacyLayout.h"
#include "DB2FileLoader.h"
#include "DB2FileSystemSource.h"
#include "DB2Meta.h"
#include "Log.h"
#include <algorithm>
#include <array>
#include <boost/filesystem.hpp>
#include <cstring>
#include <map>

namespace DB2LegacyLayout
{
namespace
{
// the SpellDuration.db2 of clients up to 70685: Duration, MaxDuration, DurationPerResource (the id is in the id list, not in the records)
constexpr DB2MetaField LegacyFields[3] =
{
    { .Type = FT_INT, .ArraySize = 1, .IsSigned = true },
    { .Type = FT_INT, .ArraySize = 1, .IsSigned = true },
    { .Type = FT_INT, .ArraySize = 1, .IsSigned = true },
};

constexpr DB2Meta LegacyMeta =
{
    .FileDataId = 1137828,
    .IndexField = -1,
    .ParentIndexField = -1,
    .FieldCount = 3,
    .FileFieldCount = 3,
    .LayoutHash = SpellDurationLegacyHash,
    .Fields = LegacyFields
};

constexpr DB2FieldMeta LegacyLoadFields[4] =
{
    { .IsSigned = false, .Type = FT_INT, .Name = "ID" },
    { .IsSigned = true, .Type = FT_INT, .Name = "Duration" },
    { .IsSigned = true, .Type = FT_INT, .Name = "MaxDuration" },
    { .IsSigned = true, .Type = FT_INT, .Name = "DurationPerResource" },
};

constexpr DB2FileLoadInfo LegacyLoadInfo{ LegacyLoadFields, 4, &LegacyMeta };

void Put(std::vector<uint8>& out, void const* data, std::size_t size)
{
    uint8 const* p = static_cast<uint8 const*>(data);
    out.insert(out.end(), p, p + size);
}

void Put32(std::vector<uint8>& out, uint32 value) { Put(out, &value, sizeof(value)); }
void Put16(std::vector<uint8>& out, uint16 value) { Put(out, &value, sizeof(value)); }
}

std::vector<uint8> BuildSpellDurationFile(std::vector<SpellDurationRow> const& rows, uint32 tableHash, bool legacyLayout)
{
    uint32 const fieldCount = legacyLayout ? 3 : 4;
    uint32 const recordSize = fieldCount * 4;
    uint32 const recordCount = uint32(rows.size());
    uint32 const stringTableSize = 2;
    uint32 const columnMetaSize = fieldCount * 24;
    uint32 const minId = rows.empty() ? 0 : rows.front().Id;
    uint32 const maxId = rows.empty() ? 0 : rows.back().Id;

    std::vector<uint8> out;
    out.reserve(300 + recordCount * (recordSize + 4));

    // header (WDC5)
    Put32(out, 0x35434457);
    Put32(out, 5);
    std::array<char, 128> schema = {};
    std::strncpy(schema.data(), "WOWSTATIC_1_60_1_70707", schema.size() - 1);
    Put(out, schema.data(), schema.size());
    Put32(out, recordCount);
    Put32(out, fieldCount);
    Put32(out, recordSize);
    Put32(out, stringTableSize);
    Put32(out, tableHash);
    Put32(out, legacyLayout ? SpellDurationLegacyHash : SpellDurationHash);
    Put32(out, minId);
    Put32(out, maxId);
    Put32(out, 0xFFFFFFFFu);        // locale
    Put16(out, 4);                  // flags: the ids are in an id list after the records
    Put16(out, 0);                  // index field
    Put32(out, fieldCount);         // total field count
    Put32(out, 0);                  // packed data offset
    Put32(out, 0);                  // parent lookup count
    Put32(out, columnMetaSize);
    Put32(out, 0);                  // common data size
    Put32(out, 0);                  // pallet data size
    Put32(out, 1);                  // section count

    // the one section
    uint32 const recordsOffset = uint32(out.size()) + 40 + fieldCount * 4 + columnMetaSize;
    Put32(out, 0); Put32(out, 0);   // tact id
    Put32(out, recordsOffset);
    Put32(out, recordCount);
    Put32(out, stringTableSize);
    Put32(out, 0);                  // catalog data offset
    Put32(out, recordCount * 4);    // id list size
    Put32(out, 0);                  // parent lookup data size
    Put32(out, 0);                  // catalog data count
    Put32(out, 0);                  // copy table count

    // field structure, then the column meta: every column is a plain signed 32 bit number
    for (uint32 i = 0; i < fieldCount; ++i)
    {
        Put16(out, 32);
        Put16(out, 0);
    }
    for (uint32 i = 0; i < fieldCount; ++i)
    {
        Put16(out, uint16(i * 32));  // bit offset
        Put16(out, 32);              // bits
        Put32(out, 0);               // additional data size
        Put32(out, 5);               // signed bit packed
        Put32(out, i * 32);          // bit packing offset
        Put32(out, 32);              // bit packing size
        Put32(out, 0);               // flags
    }

    for (SpellDurationRow const& row : rows)
    {
        Put32(out, uint32(row.Duration));
        if (!legacyLayout)
            Put32(out, uint32(row.DurationPerLevel));
        Put32(out, uint32(row.MaxDuration));
        Put32(out, uint32(row.DurationPerResource));
    }
    out.push_back(0);
    out.push_back(0);               // string table
    for (SpellDurationRow const& row : rows)
        Put32(out, row.Id);
    return out;
}

bool UpgradeSpellDurationFile(std::string const& path)
{
    try
    {
        uint32 tableHash = 0;
        {
            DB2FileSystemSource source(path);
            if (!source.IsOpen())
                return false;
            DB2FileLoader headers;
            headers.LoadHeaders(&source, nullptr);
            if (headers.GetLayoutHash() != SpellDurationLegacyHash)
                return false;   // the 70707 layout (or something this code does not know): nothing to do
            tableHash = headers.GetTableHash();
        }

        // the file has to be closed again before it is replaced: Windows refuses to rename over a file that is open
        std::vector<SpellDurationRow> rows;
        {
        DB2FileSystemSource source(path);
        DB2FileLoader loader;
        loader.Load(&source, &LegacyLoadInfo);

        std::map<uint32, SpellDurationRow> byId;
        for (uint32 i = 0; i < loader.GetRecordCount(); ++i)
        {
            DB2Record record = loader.GetRecord(i);
            SpellDurationRow row;
            row.Id = record.GetId();
            row.Duration = record.GetInt32(0u, 0u);
            row.DurationPerLevel = 0;
            row.MaxDuration = record.GetInt32(1u, 0u);
            row.DurationPerResource = record.GetInt32(2u, 0u);
            byId[row.Id] = row;
        }
        for (uint32 i = 0; i < loader.GetRecordCopyCount(); ++i)
        {
            DB2RecordCopy copy = loader.GetRecordCopy(i);
            auto source_ = byId.find(copy.SourceRowId);
            if (source_ == byId.end())
                continue;
            SpellDurationRow row = source_->second;
            row.Id = copy.NewRowId;
            byId[row.Id] = row;
        }

        rows.reserve(byId.size());
        for (auto const& [id, row] : byId)
            rows.push_back(row);
        }

        std::vector<uint8> bytes = BuildSpellDurationFile(rows, tableHash, false);

        boost::filesystem::path const target(path);
        boost::filesystem::path const temp = target.string() + ".new-layout";
        boost::filesystem::path const backup = target.string() + ".legacy-layout";
        {
            FILE* f = fopen(temp.string().c_str(), "wb");
            if (!f)
            {
                TC_LOG_ERROR("server.loading", "DB2: cannot write {}: SpellDuration.db2 stays in the old layout", temp.string());
                return false;
            }
            bool const ok = fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size();
            fclose(f);
            if (!ok)
            {
                boost::filesystem::remove(temp);
                TC_LOG_ERROR("server.loading", "DB2: writing {} failed: SpellDuration.db2 stays in the old layout", temp.string());
                return false;
            }
        }
        if (!boost::filesystem::exists(backup))
            boost::filesystem::copy_file(target, backup);
        boost::system::error_code renameError;
        boost::filesystem::rename(temp, target, renameError);    // replaces the old file
        if (renameError)
        {
            boost::system::error_code ignored;
            boost::filesystem::remove(temp, ignored);
            TC_LOG_ERROR("server.loading", "DB2: cannot replace {} ({}): SpellDuration.db2 stays in the old layout", path, renameError.message());
            return false;
        }
        TC_LOG_INFO("server.loading", "DB2: {} was in the layout of game builds up to 70685 and was converted to the 70707 layout ({} rows); the old file is kept as {}",
            path, rows.size(), backup.filename().string());
        return true;
    }
    catch (std::exception const& e)
    {
        TC_LOG_ERROR("server.loading", "DB2: could not check {} for the old layout: {}", path, e.what());
        return false;
    }
}

uint32 UpgradeLegacyDB2Files(std::string const& db2Path)
{
    uint32 converted = 0;
    boost::system::error_code ec;
    boost::filesystem::directory_iterator itr(db2Path, ec), end;
    while (!ec && itr != end)
    {
        boost::filesystem::path const file = itr->path() / "SpellDuration.db2";
        if (boost::filesystem::is_directory(itr->path(), ec) && boost::filesystem::exists(file, ec))
            if (UpgradeSpellDurationFile(file.string()))
                ++converted;
        itr.increment(ec);
    }
    return converted;
}
}
