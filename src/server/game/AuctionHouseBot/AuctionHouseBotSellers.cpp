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

#include "AuctionHouseBotSellers.h"
#include "CharacterCache.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "Log.h"
#include "ObjectGuid.h"
#include "ObjectMgr.h"
#include "RaceMask.h"
#include "Random.h"
#include "SharedDefines.h"
#include "StringFormat.h"
#include <array>
#include <string>

namespace AuctionBotSellers
{
namespace
{
    constexpr std::array<char const*, 24> Prefixes = {
        "Bar", "Dun", "Gor", "Hal", "Kel", "Mor", "Nor", "Ral", "Tor", "Vel", "Wen", "Zul",
        "Ael", "Bran", "Cor", "Dar", "Eld", "Fen", "Gar", "Har", "Jor", "Lor", "Mal", "Thal" };
    constexpr std::array<char const*, 20> Suffixes = {
        "dan", "dor", "gar", "ian", "las", "mar", "nik", "ron", "sil", "tam",
        "wyn", "bek", "dris", "gan", "lin", "mus", "rik", "tor", "vin", "zan" };

    struct Look { uint8 Race; uint8 Class; };
    // Alliance and Horde mixed, vanilla races only.
    constexpr std::array<Look, 12> Looks = { {
        { RACE_HUMAN, CLASS_WARRIOR }, { RACE_ORC, CLASS_WARRIOR }, { RACE_DWARF, CLASS_PALADIN },
        { RACE_TAUREN, CLASS_DRUID }, { RACE_NIGHTELF, CLASS_HUNTER }, { RACE_UNDEAD_PLAYER, CLASS_MAGE },
        { RACE_GNOME, CLASS_WARLOCK }, { RACE_TROLL, CLASS_ROGUE }, { RACE_HUMAN, CLASS_PRIEST },
        { RACE_ORC, CLASS_SHAMAN }, { RACE_DWARF, CLASS_HUNTER }, { RACE_UNDEAD_PLAYER, CLASS_PRIEST } } };

    bool NameTaken(std::string const& name)
    {
        if (!sCharacterCache->GetCharacterGuidByName(name).IsEmpty())
            return true;
        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHECK_NAME);
        stmt->setString(0, name);
        return bool(CharacterDatabase.Query(stmt));
    }

    std::string MakeName()
    {
        for (uint32 attempt = 0; attempt < 200; ++attempt)
        {
            std::string name = std::string(Prefixes[urand(0, Prefixes.size() - 1)]) + Suffixes[urand(0, Suffixes.size() - 1)];
            if (name.size() > 12)
                continue;
            if (!NameTaken(name))
                return name;
        }
        return {};
    }
}

uint32 EnsureCharacters(uint32 accountId)
{
    uint32 const wanted = sConfigMgr->GetIntDefault("AuctionHouseBot.Sellers.Count", 0);
    if (!wanted || !accountId)
        return 0;

    uint32 have = 0;
    {
        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARS_BY_ACCOUNT_ID);
        stmt->setUInt32(0, accountId);
        if (PreparedQueryResult result = CharacterDatabase.Query(stmt))
            have = uint32(result->GetRowCount());
    }

    uint32 created = 0;
    for (uint32 i = have; i < wanted; ++i)
    {
        std::string const name = MakeName();
        if (name.empty())
        {
            TC_LOG_WARN("ahbot", "AuctionHouseBot sellers: could not find a free name, stopping at {} characters", i);
            break;
        }

        Look const& look = Looks[i % Looks.size()];
        ObjectGuid::LowType const low = sObjectMgr->GetGenerator<HighGuid::Player>().Generate();
        ObjectGuid const guid = ObjectGuid::Create<HighGuid::Player>(low);
        uint8 const gender = urand(0, 1);

        // Only columns that exist in the base schema; everything else keeps its table default.
        std::string const sql = Trinity::StringFormat(
            "INSERT INTO `characters` (`guid`, `account`, `name`, `race`, `class`, `gender`, `level`, `taximask`, `createTime`, `logout_time`) "
            "VALUES ({}, {}, '{}', {}, {}, {}, 60, '', {}, {})",
            low, accountId, name, uint32(look.Race), uint32(look.Class), uint32(gender),
            uint64(GameTime::GetGameTime()), uint64(GameTime::GetGameTime()));
        CharacterDatabase.DirectExecute(sql.c_str());

        sCharacterCache->AddCharacterCacheEntry(guid, accountId, name, gender, look.Race, look.Class, 60, false);
        ++created;
    }

    if (created)
        TC_LOG_INFO("ahbot", "AuctionHouseBot created {} seller characters on account {}", created, accountId);
    return created;
}
}
