#include "tc_catch2.h"
#include "ChatPackets.h"
#include "Util.h"

// CMSG_CHAT_ADDON_MESSAGE_TARGETED from client 70685 (live realm, 2026-10-29): the ForeverBots add-on whispering "hello" with prefix FBOTS
// to its own player "Karrz". The classic 1.60 layout has a second 9 bit sized string after PlayerName (as CMSG_CHAT_MESSAGE_WHISPER does);
// reading it as retail made the bit block a byte short, so PlayerName came out as "\0Karr" and the whisper hooks never ran.
TEST_CASE("CHAT_ADDON_MESSAGE_TARGETED reads the classic whisper layout", "[ChatAddonTargetedLayout]")
{
    std::vector<uint8> payload = HexStrToByteVector("28280700000046424F545368656C6C6F0000000000000000030000004B6172727A00");
    WorldPacket raw(CMSG_CHAT_ADDON_MESSAGE_TARGETED, payload.size());
    raw.append(payload.data(), payload.size());

    WorldPackets::Chat::ChatAddonMessageTargeted packet(std::move(raw));
    packet.Read();

    CHECK(packet.Params.Prefix == "FBOTS");
    CHECK(packet.Params.Text == "hello");
    CHECK(packet.Params.Type == CHAT_MSG_WHISPER);
    CHECK_FALSE(packet.Params.IsLogged);
    CHECK(packet.PlayerName == "Karrz");
    CHECK(packet.PlayerGUID.IsEmpty());
    CHECK(packet.PlayerVirtualRealmAddress == 0);
    CHECK(packet.ChannelName.empty());
    CHECK(packet.ChannelGUID.IsEmpty());
    CHECK(packet.GetRawPacket()->rpos() == packet.GetRawPacket()->size());
}
