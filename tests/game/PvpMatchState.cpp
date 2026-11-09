#include "tc_catch2.h"
#include "BattlegroundPackets.h"
#include "ByteBuffer.h"

// SMSG_PVP_MATCH_SET_STATE: one byte, the client's Enum.PvPMatchState (Waiting 0, StartUp 1, Engaged 2, PostRound 3, Inactive 4, Complete 5).
// The server sends Inactive when a player leaves a battleground (Battleground::RemovePlayerAtLeave): without it the 70291 client keeps its PvP
// match, and with it the restricted addon environment, active after the player is out (audit F-3).
TEST_CASE("PVP_MATCH_SET_STATE is one byte holding the client's state value", "[PvpMatchState]")
{
    using namespace WorldPackets::Battleground;

    CHECK(uint8(PVPMatchState::Waiting) == 0);
    CHECK(uint8(PVPMatchState::StartUp) == 1);
    CHECK(uint8(PVPMatchState::Engaged) == 2);
    CHECK(uint8(PVPMatchState::PostRound) == 3);
    CHECK(uint8(PVPMatchState::Inactive) == 4);
    CHECK(uint8(PVPMatchState::Complete) == 5);

    PVPMatchSetState packet(PVPMatchState::Inactive);
    WorldPacket const* data = packet.Write();
    REQUIRE(data->size() == 1);
    CHECK(data->data()[0] == 4);
}
