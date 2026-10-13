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

#include "ClassicOpcodes.h"
#include "Config.h"
#include "Opcodes.h"
#include "Realm.h"
#include "RealmList.h"
#include <unordered_map>
#include <unordered_set>

namespace
{
// Opcodes are 0xGGIIII: group byte + index. The connection-level groups appear shifted by one in Classic:
//  - client group 0x45 (retail 0x44): 0x450007 carried a single uint32 (3) = CMSG_LOG_DISCONNECT (retail 0x440007)
//  - server group 0x4D (retail 0x4C, assumed by symmetry)
constexpr uint32 ClassicClientConnectionGroup = 0x45;
constexpr uint32 RetailClientConnectionGroup = 0x44;
constexpr uint32 ClassicServerConnectionGroup = 0x4D;
constexpr uint32 RetailServerConnectionGroup = 0x4C;

// explicit Classic 1.60.1.70009 client opcode -> retail core opcode overrides
std::unordered_map<uint32, uint32> const ClientOpcodes =
{
#include "ClassicClientOpcodes.inc"
};

// explicit retail core opcode -> Classic 1.60.1.70009 client opcode overrides
// Client protocol (0x46): Classic removed one message somewhere before 0x1B5, so around 0x1B5-0x1C0 Classic index = retail index - 1
// (decoder fixed sizes: Classic 0x1B6/0x1B7 = 16 bytes = retail GAME_TIME_UPDATE/GAME_TIME_SET, Classic 0x1B8 = 20 = retail LOGIN_SET_TIME_SPEED).
// Indices from 0x268 on match retail again, so a Classic-only message is inserted somewhere between 0x1C0 and 0x268.
std::unordered_map<uint32, uint32> const ServerOpcodes =
{
    { SMSG_ACCOUNT_DATA_TIMES,      0x4601B5 },  // same layout as retail (guid, int64 server time, 20 x int64), decoder 0x81A300
    { SMSG_GAME_TIME_UPDATE,        0x4601B6 },
    { SMSG_GAME_TIME_SET,           0x4601B7 },
    { SMSG_LOGIN_SET_TIME_SPEED,    0x4601B8 },
    { SMSG_SERVER_TIME_OFFSET,      0x4601BF },  // decoder asserts datasize == 8; 0x1BD = CORPSE_TRANSPORT_QUERY, 0x1BE = ENCHANTMENT_LOG (both retail - 1)
    { SMSG_MIRROR_VARS,             0x460371 },  // decoder 0x83E330: count, then 1 bit + 2 x 24-bit sized strings (0x46036F reads 16-byte records)
    { SMSG_LAST_CATALOG_FETCH_RESPONSE, 0x460382 },  // +2 region like MIRROR_VARS (decoder 0x83FCC0)
    { SMSG_QUERY_GUILD_FOLLOW_INFO_RESPONSE, 0x52002E },  // retail index - 1 (70058 guild switch reader)
};

// sent with the shifted number these hit a different Classic message (client JamClient size asserts)
std::unordered_set<uint32> const BlockedServerOpcodes =
{
    SMSG_HEALTH_UPDATE,     // does not exist in Classic (health is sent through UnitData)
    SMSG_GUILD_CHALLENGE_COMPLETED, // does not exist in Classic (guild switch has one case less, see TranslateServerOpcode)
    SMSG_CHANNEL_LIST,      // Classic chat index 0x1D (reader rva 0xA10E70), layout not verified yet
};

// Classic layout differs; the client waits for the message, so send a placeholder of the expected size
std::unordered_map<uint32, std::vector<uint8>> const StubServerPayloads =
{
    { SMSG_ACCOUNT_ITEM_COLLECTION_DATA, std::vector<uint8>(4, 0) },  // retail 10 bytes, Classic 0x460360 expects 4 (JamClient.cpp:167052)
    { SMSG_DAILY_QUESTS_RESET, {} },  // retail int32 count; Classic expects no data (JamClientQuest.cpp:3723 asserts datasize == 0 at the daily reset)
};

constexpr uint32 OpcodeGroup(uint32 opcode) { return opcode >> 16; }
constexpr uint32 WithOpcodeGroup(uint32 opcode, uint32 group) { return (group << 16) | (opcode & 0xFFFF); }
}

// Hypothesis under test: every opcode group is shifted by +1 in Classic (verified for the connection groups 0x44/0x4C)
constexpr uint32 FirstRetailClientGroup = 0x2A;
constexpr uint32 LastRetailClientGroup = RetailClientConnectionGroup;
constexpr uint32 FirstRetailServerGroup = 0x45;
constexpr uint32 LastRetailServerGroup = 0x6A;
constexpr uint32 ChatRetailServerGroup = 0x4A;
constexpr uint32 GuildRetailServerGroup = 0x51;

// Client 1.60.1.70291 (2026-10-09), the tables above stay in 70245 numbers:
//  - client: only the connection group moved (CMSG_LOG_DISCONNECT 0x450007 -> 0x460007), CMSG_ENUM_CHARACTERS stayed 0x440014
//  - server: every group is one higher (SMSG_AUTH_CHALLENGE 0x4D0000 -> 0x4E0000; with the main group left at 0x46 the client ignores
//    AUTH_RESPONSE and never asks for the characters), and the main group got one more message: the official 70291 sniff has
//    70245 index 0x92 (BATTLE_PET_JOURNAL) unchanged and 0xAD (START_ELAPSED_TIMERS) at 0xAE, SET_TIME_ZONE_INFORMATION 0x124,
//    ACCOUNT_DATA_TIMES 0x1B6, SERVER_TIME_OFFSET 0x1C0, TUTORIAL_FLAGS 0x269, MIRROR_VARS 0x372, BATTLENET_RESPONSE 0x2B0;
//    the inserted message is index 0x98: the official 70338 sniff has PARTY_UPDATE (0x97) unchanged at 0x470097
// Classic.OpcodeGroupShift: -1 (default) = from the realm's game build (realmlist.gamebuild >= 70291 -> 1), or 0 / 1.
static uint32 ExtraGroupShift()
{
    static uint32 const shift = []() -> uint32
    {
        int32 configured = sConfigMgr->GetIntDefault("Classic.OpcodeGroupShift", -1);
        if (configured >= 0)
            return uint32(configured);
        std::shared_ptr<Realm const> realm = sRealmList->GetCurrentRealm();
        return realm && realm->Build >= 70291 ? 1 : 0;
    }();
    return shift;
}

bool ClassicOpcodes::IsBuild70291OrLater()
{
    return ExtraGroupShift() != 0;
}

uint32 ClassicOpcodes::TranslateClientOpcode(uint32 classicOpcode)
{
    if (uint32 shift = ExtraGroupShift())
    {
        // client: the connection group (70245 0x45) moved to 0x46
        if (OpcodeGroup(classicOpcode) == ClassicClientConnectionGroup + shift)
            classicOpcode = WithOpcodeGroup(classicOpcode, ClassicClientConnectionGroup);
        // and the main client group 0x44 got one more message between 70245 index 0x13A and 0x150 (sniffs 2026-10-09: 0x13A
        // BATTLE_PAY_OPEN_CHECKOUT unchanged, 0x150 -> 0x151; RECENT_ALLY_REQUEST_DATA, ACCEPT_SOCIAL_CONTRACT, club finder are
        // above it). Classic.ClientMainGroupInsertIndex = the 70291 index of the new message, everything after it is one lower.
        static uint32 const clientInsertIndex = uint32(sConfigMgr->GetIntDefault("Classic.ClientMainGroupInsertIndex", 0x13B));
        if (OpcodeGroup(classicOpcode) == ClassicClientConnectionGroup - 1 && (classicOpcode & 0xFFFF) > clientInsertIndex)
            --classicOpcode;
    }

    return TranslateClientOpcode70245(classicOpcode);
}

uint32 ClassicOpcodes::TranslateServerOpcode(uint32 coreOpcode)
{
    uint32 classicOpcode = TranslateServerOpcode70245(coreOpcode);
    if (uint32 shift = ExtraGroupShift())
    {
        uint32 group = OpcodeGroup(classicOpcode);
        // Classic.MainGroupInsertIndex = the first 70245 main-group index that moved (0x98, see above)
        static uint32 const insertIndex = uint32(sConfigMgr->GetIntDefault("Classic.MainGroupInsertIndex", 0x98));
        if (group == FirstRetailServerGroup + 1 && (classicOpcode & 0xFFFF) >= insertIndex)
            ++classicOpcode;
        if (group >= FirstRetailServerGroup + 1 && group <= LastRetailServerGroup + 1)
            classicOpcode = WithOpcodeGroup(classicOpcode, group + shift);
    }
    return classicOpcode;
}

uint32 ClassicOpcodes::TranslateClientOpcode70245(uint32 classicOpcode)
{
    if (auto itr = ClientOpcodes.find(classicOpcode); itr != ClientOpcodes.end())
        return itr->second;

    uint32 group = OpcodeGroup(classicOpcode);
    if (group >= FirstRetailClientGroup + 1 && group <= ClassicClientConnectionGroup)
        return WithOpcodeGroup(classicOpcode, group - 1);

    return classicOpcode;
}

uint32 ClassicOpcodes::TranslateServerOpcode70245(uint32 coreOpcode)
{
    if (auto itr = ServerOpcodes.find(coreOpcode); itr != ServerOpcodes.end())
        return itr->second;

    uint32 group = OpcodeGroup(coreOpcode);
    if (group >= FirstRetailServerGroup && group <= LastRetailServerGroup)
    {
        uint32 index = coreOpcode & 0xFFFF;
        // Client protocol (retail 0x45): Classic removed SMSG_HEALTH_UPDATE (retail 0x17D), so Classic index = retail - 1 for retail
        // 0x17E..0x1C0 (signature alignment, classic_re/smsg_align.py: POWER_UPDATE at Classic 0x17D, PLAYED_TIME 0x180, LOG_XP_GAIN 0x18F,
        // mirror timers, SERVER_TIME_OFFSET 0x1BF; MINIMAP_PING 0x17A and FISH_* 0x17B/0x17C keep their index)
        if (group == FirstRetailServerGroup && index >= 0x17E && index <= 0x1C0)
            --index;
        // Chat (retail 0x4A, Classic 0x4B, client switch rva 0xA11168, 39 cases): Classic has two extra messages at indices 4-5, so
        // Classic index = retail + 2 from retail 4 on (CHAT_PLAYER_NOTFOUND/AMBIGUOUS name readers at 6/15, USERLIST_* 17-19,
        // CHANNEL_NOTIFY_JOINED 0x1B, CHANNEL_NOTIFY_LEFT 0x1C, CHAT_SERVER_MESSAGE 0x1E; CHAT and MOTD keep 1 and 3)
        if (group == ChatRetailServerGroup && index >= 4)
            index += 2;
        // Guild (retail 0x51, Classic 0x52, client switch rva 0xA1F9E5 in 70058, table 0xA233EC, 0x47 cases vs retail 0x48): Classic has
        // no GUILD_CHALLENGE_COMPLETED (retail 0x1B, blocked), so Classic index = retail - 1 from retail 0x1C on (Classic 0x1A int32 =
        // CHALLENGE_UPDATE, 0x1B guid = ITEM_LOOTED_NOTIFY, 0x22 = NAME_CHANGED, 0x23 bit = FLAGGED_FOR_RENAME, 0x25 = BANK_QUERY_RESULTS,
        // 0x2C = QUERY_GUILD_INFO_RESPONSE, 0x31 = EVENT_PLAYER_JOINED, 0x35 = EVENT_MOTD, 0x36 = EVENT_PRESENCE_CHANGE)
        if (group == GuildRetailServerGroup && index >= 0x1C)
            --index;
        return ((group + 1) << 16) | index;
    }

    return coreOpcode;
}

bool ClassicOpcodes::IsServerOpcodeBlocked(uint32 coreOpcode)
{
    return BlockedServerOpcodes.contains(coreOpcode);
}

std::vector<uint8> const* ClassicOpcodes::GetServerStubPayload(uint32 coreOpcode)
{
    auto itr = StubServerPayloads.find(coreOpcode);
    return itr != StubServerPayloads.end() ? &itr->second : nullptr;
}
