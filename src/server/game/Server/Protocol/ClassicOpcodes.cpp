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
#include "StringConvert.h"
#include "Util.h"
#include <algorithm>
#include <string>
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

bool ClassicOpcodes::IsBuild70425OrLater()
{
    static bool const later = []()
    {
        std::shared_ptr<Realm const> realm = sRealmList->GetCurrentRealm();
        return realm && realm->Build >= 70425;
    }();
    return later;
}

// Client 1.60.1.70755 (2026-11-03, Battle.net pre-install for the 4 November launch) added messages to the main groups. The 70707
// numbers stay the base; a 70755 client's numbers are mapped to them per connection, so 70707 and older clients keep working on the
// same realm. Each config list holds the 70755 numbers (full opcode, hex like 0x4400C3 or decimal) of messages that are new in 70755:
//  - Classic.Build70755.ClientNewMessages: client messages after a listed one (same group) are one lower in 70707 numbering; a
//    listed one has no 70707 counterpart and is logged as unknown. 2026-11-03 login: SERVER_TIME_OFFSET_REQUEST, BATTLE_PAY_GET_*_LIST,
//    GET_UNDELETE_CHARACTER_COOLDOWN_STATUS, UPDATE_VAS_PURCHASE_STATES and SOCIAL_CONTRACT_REQUEST came 2 higher; HOTFIX_REQUEST
//    (0x440011) and ENUM_CHARACTERS (0x440014) did not move.
//    Packet log 2026-11-03 20:05 (all character-screen messages, empty payloads where expected): moved by 2 = 0x4400CC, 0x4400EB,
//    0x4400EC, 0x440111, 0x440125, 0x440135 (1 byte), 0x440179; not moved = DB_QUERY_BULK 0x440010, HOTFIX_REQUEST, ENUM_CHARACTERS and
//    group 0x2B. So both new messages lie in 0x440015..0x4400CB; the default is the lowest place and is tuned with a restart from
//    what the next packet log shows. 20:27 test: CHECK_CHARACTER_NAME_AVAILABILITY came at 0x440073 (= 0x71 + 2).
//    20:30 test: CREATE_CHARACTER (96 bytes, holds the name) came at 0x440070 (not moved) and name check at 0x440073, so the two
//    new messages are 0x440071 and 0x440072. Defaults: client 0x440071/0x440072 and server 0x470125 (server place as in
//    advocaite/TrinityCore PR #271).
//  - Classic.Build70755.ServerNewMessages: server messages from a listed number on (same group) are sent one higher. 2026-11-03:
//    ENUM_CHARACTERS_RESULT (0x470018) and FEATURE_SYSTEM_STATUS_GLUE_SCREEN (0x470064) still work, ACCOUNT_DATA_TIMES (0x4701B6)
//    is no longer recognised (the client then waits at "Retrieving character list" forever).
namespace
{
struct BuildMessageChanges
{
    uint32 FirstBuild = 0;
    std::vector<uint32> ClientNew;   // sorted
    std::vector<uint32> ServerNew;   // sorted
    std::vector<uint32> ServerRemoved;   // sorted, previous build's numbering
};

std::vector<uint32> ParseOpcodeList(std::string const& text)
{
    std::vector<uint32> list;
    for (std::string_view token : Trinity::Tokenize(text, ',', false))
    {
        std::string t(token);
        t.erase(std::remove_if(t.begin(), t.end(), [](char c) { return c == ' ' || c == '\t'; }), t.end());
        if (t.empty())
            continue;
        try { list.push_back(uint32(std::stoul(t, nullptr, 0))); }
        catch (...) { }
    }
    std::sort(list.begin(), list.end());
    list.erase(std::unique(list.begin(), list.end()), list.end());
    return list;
}

BuildMessageChanges const& Build70755Changes()
{
    static BuildMessageChanges const changes = []()
    {
        BuildMessageChanges c;
        c.FirstBuild = uint32(sConfigMgr->GetIntDefault("Classic.Build70755.FirstBuild", 70755));
        c.ClientNew = ParseOpcodeList(sConfigMgr->GetStringDefault("Classic.Build70755.ClientNewMessages", "0x440071,0x440072"));
        c.ServerNew = ParseOpcodeList(sConfigMgr->GetStringDefault("Classic.Build70755.ServerNewMessages", "0x470125"));
        return c;
    }();
    return changes;
}

// Client 1.60.1.70790 (2026-11-04 18:49 UK, Battle.net update a few hours before the launch). Same idea one step further: a 70790
// client's numbers are first mapped to 70755 numbers with Classic.Build70790.*, then to 70707 numbers with Classic.Build70755.*.
// Client messages: unchanged from 70755 (login, world, spells, quests on 70790 2026-11-04).
// Server messages, main group: official 70532 vs 70790 sniffs (advocaite/TrinityCore b4e7b940, 49 messages) have one new message
// against 70532: everything up to INSTANCE_INFO (wire 0x4700DE) unchanged, everything from ACHIEVEMENT_EARNED (0x4700ED) on one
// higher (every message seen above it). Our 70755 rule (one higher from 0x470125 on) leaves wire
// 0x4700DF..0x470124 one too low for 70790, and 2026-11-05 a 70790 ghost crashed reading our CORPSE_LOCATION (0x4700F8) as
// SERVER_FIRST_ACHIEVEMENTS. So for 70790 the 70755 message at 0x470125 is taken out again (Classic.Build70790.ServerRemovedMessages)
// and the new one goes in at 0x4700DF (Classic.Build70790.ServerNewMessages): base numbers from 0x4700DF on are one higher, as in
// the sniffs. The sniffs place the new message in 0x4700DF..0x4700ED (nothing between was seen). Tested on a 70790 client
// 2026-11-05: with 0x4700DF the mail result (0x4700E4) did not reach the client (mail window frozen), with 0x4700ED it did, so the
// new message sits at 0x4700ED, right below ACHIEVEMENT_EARNED, and 0x4700DF..0x4700EC keep their numbers.
BuildMessageChanges const& Build70790Changes()
{
    static BuildMessageChanges const changes = []()
    {
        BuildMessageChanges c;
        c.FirstBuild = uint32(sConfigMgr->GetIntDefault("Classic.Build70790.FirstBuild", 70790));
        c.ClientNew = ParseOpcodeList(sConfigMgr->GetStringDefault("Classic.Build70790.ClientNewMessages", ""));
        c.ServerNew = ParseOpcodeList(sConfigMgr->GetStringDefault("Classic.Build70790.ServerNewMessages", "0x4700ED"));
        c.ServerRemoved = ParseOpcodeList(sConfigMgr->GetStringDefault("Classic.Build70790.ServerRemovedMessages", "0x470125"));
        return c;
    }();
    return changes;
}

bool AppliesTo(BuildMessageChanges const& changes, uint32 clientBuild)
{
    return clientBuild && changes.FirstBuild && clientBuild >= changes.FirstBuild;
}

// 70755 client number -> 70707 client number (UNKNOWN_OPCODE for a message 70707 does not have)
uint32 ClientToBaseBuild(uint32 opcode, BuildMessageChanges const& changes)
{
    uint32 group = OpcodeGroup(opcode);
    uint32 lower = 0;
    for (uint32 added : changes.ClientNew)
    {
        if (OpcodeGroup(added) != group)
            continue;
        if (added == opcode)
            return UNKNOWN_OPCODE;
        if (added < opcode)
            ++lower;
    }
    return opcode - lower;
}

// previous build's server number -> this build's server number (70707 -> 70755, 70755 -> 70790)
uint32 ServerFromBaseBuild(uint32 opcode, BuildMessageChanges const& changes)
{
    uint32 group = OpcodeGroup(opcode);
    uint32 lower = 0;
    for (uint32 removed : changes.ServerRemoved)   // gone in this build: everything after it is one lower
        if (OpcodeGroup(removed) == group && opcode > removed)
            ++lower;
    opcode -= lower;
    for (uint32 added : changes.ServerNew)   // ascending: each new message pushes everything from its number on up by one
        if (OpcodeGroup(added) == group && opcode >= added)
            ++opcode;
    return opcode;
}
}

uint32 ClassicOpcodes::TranslateClientOpcode(uint32 classicOpcode, uint32 clientBuild)
{
    if (BuildMessageChanges const& changes = Build70790Changes(); AppliesTo(changes, clientBuild))
    {
        classicOpcode = ClientToBaseBuild(classicOpcode, changes);
        if (classicOpcode == UNKNOWN_OPCODE)
            return UNKNOWN_OPCODE;
    }

    if (BuildMessageChanges const& changes = Build70755Changes(); AppliesTo(changes, clientBuild))
    {
        classicOpcode = ClientToBaseBuild(classicOpcode, changes);
        if (classicOpcode == UNKNOWN_OPCODE)
            return UNKNOWN_OPCODE;
    }

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

uint32 ClassicOpcodes::TranslateServerOpcode(uint32 coreOpcode, uint32 clientBuild)
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
    if (BuildMessageChanges const& changes = Build70755Changes(); AppliesTo(changes, clientBuild))
        classicOpcode = ServerFromBaseBuild(classicOpcode, changes);
    if (BuildMessageChanges const& changes = Build70790Changes(); AppliesTo(changes, clientBuild))
        classicOpcode = ServerFromBaseBuild(classicOpcode, changes);
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

// Server messages that are not sent to clients of Classic.Build70790.FirstBuild or newer. Their number in that build is not known yet,
// and with the 70755 number the client reads them as a different message. 2026-11-05 client error file (70790, a ghost in
// Hillsbrad): "Requested 66387465216 bytes" in JamServerFirstAchievement = 0xA4E00380 records of 24 bytes, and 0xA4E00380 is the
// start of our CORPSE_LOCATION answer (80 = Valid bit, 03 E0 = packed guid masks, A4 = the player's own guid). So a 70790 client
// expects SERVER_FIRST_ACHIEVEMENTS on the number we send CORPSE_LOCATION on, and the corpse query crashed the game.
// Classic.Build70790.BlockedServerMessages takes core numbers (hex or decimal); empty by default since the 70790 server numbering
// (Build70790Changes) sends CORPSE_LOCATION on its own 70790 number.
std::vector<uint32> const& Build70790Blocked()
{
    static std::vector<uint32> const blocked = ParseOpcodeList(sConfigMgr->GetStringDefault("Classic.Build70790.BlockedServerMessages", ""));
    return blocked;
}

bool ClassicOpcodes::IsServerOpcodeBlocked(uint32 coreOpcode, uint32 clientBuild)
{
    if (BlockedServerOpcodes.contains(coreOpcode))
        return true;

    if (AppliesTo(Build70790Changes(), clientBuild))
    {
        std::vector<uint32> const& blocked = Build70790Blocked();
        if (std::binary_search(blocked.begin(), blocked.end(), coreOpcode))
            return true;
    }

    return false;
}

std::vector<uint8> const* ClassicOpcodes::GetServerStubPayload(uint32 coreOpcode)
{
    auto itr = StubServerPayloads.find(coreOpcode);
    return itr != StubServerPayloads.end() ? &itr->second : nullptr;
}
