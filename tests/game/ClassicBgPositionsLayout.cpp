#include "tc_catch2.h"
#include "BattlegroundPackets.h"
#include "ByteBuffer.h"
#include "ObjectGuid.h"
#include "Position.h"
#include "WorldPacket.h"

// SMSG_BATTLEGROUND_PLAYER_POSITIONS (0x4D0006) as the Classic 1.60.1.70291 client reads and uses it (WowB.exe 70291).
// Reader (rva 0xA29EF0): u32 count, then per entry (stride 0x20) packed guid, u32 x, u32 y, u8 icon (+0x18), u8 slot (+0x19).
// Handler (rva 0x2B0FA10): slot 2..6 are arena team slots; for a battleground it only looks at entries whose slot byte is 1
// (PLAYER_POSITION_ARENA_SLOT_NONE), looks the guid up as a player and gives it a map icon. An entry with slot 0 (what the server sent
// before) is read correctly and then never drawn: no flag carrier on the map or minimap in a battleground.
namespace
{
using namespace WorldPackets::Battleground;

struct ReadEntry
{
    ObjectGuid Guid;
    float X = 0.0f, Y = 0.0f;
    uint8 Icon = 0, Slot = 0;
};

std::vector<ReadEntry> ReadLikeClient(WorldPacket const& data)
{
    ByteBuffer buffer(data);
    uint32 count = 0;
    buffer >> count;
    std::vector<ReadEntry> entries;
    for (uint32 i = 0; i < count; ++i)
    {
        ReadEntry entry;
        buffer >> entry.Guid >> entry.X >> entry.Y >> entry.Icon >> entry.Slot;
        entries.push_back(entry);
    }
    // the client's reader stops here: a byte more or less shifts every later entry and the next message
    CHECK(buffer.rpos() == buffer.size());
    return entries;
}
}

TEST_CASE("BG player positions are read to the last byte and a carrier has slot 1", "[ClassicBgPositions]")
{
    BattlegroundPlayerPositions packet;

    BattlegroundPlayerPosition alliance;
    alliance.Guid = ObjectGuid::Create<HighGuid::Player>(1);
    alliance.Pos = Position(920.0f, 1434.0f, 0.0f);
    alliance.IconID = 2;
    packet.FlagCarriers.push_back(alliance);

    BattlegroundPlayerPosition horde;
    horde.Guid = ObjectGuid::Create<HighGuid::Player>(70000);
    horde.Pos = Position(1540.0f, 1481.0f, 0.0f);
    horde.IconID = 1;
    packet.FlagCarriers.push_back(horde);

    std::vector<ReadEntry> const entries = ReadLikeClient(*packet.Write());
    REQUIRE(entries.size() == 2);
    CHECK(entries[0].Guid == alliance.Guid);
    CHECK(entries[0].X == 920.0f);
    CHECK(entries[0].Y == 1434.0f);
    CHECK(entries[0].Icon == 2);
    CHECK(entries[1].Guid == horde.Guid);
    CHECK(entries[1].Icon == 1);
    for (ReadEntry const& entry : entries)
        CHECK(entry.Slot == 1);     // the value the client's handler requires for a battleground carrier
}

TEST_CASE("An empty BG positions packet is just the count", "[ClassicBgPositions]")
{
    BattlegroundPlayerPositions packet;
    WorldPacket const* data = packet.Write();
    CHECK(data->size() == 4);
    CHECK(ReadLikeClient(*data).empty());
}
