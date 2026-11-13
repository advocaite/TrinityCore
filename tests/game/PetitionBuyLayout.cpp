#include "tc_catch2.h"
#include "PetitionPackets.h"

// CMSG_PETITION_BUY from the classic 1.60 client: an 8 bit title length, the petitioner, a uint32 and the title (player report
// 2026-11-13: read with a 7 bit length, "Knights of Valor" became the guild "Knights "). The title length has 8 bits in
// SMSG_QUERY_PETITION_RESPONSE too.
TEST_CASE("PETITION_BUY reads the whole guild name", "[PetitionBuyLayout]")
{
    std::string const title = "Knights of Valor";
    ObjectGuid const petitioner = ObjectGuid::Create<HighGuid::Creature>(0, 4974, UI64LIT(30001234));

    WorldPacket raw(CMSG_PETITION_BUY);
    raw.WriteBits(title.size(), 8);
    raw.FlushBits();
    raw << petitioner;
    raw << uint32(0);
    raw.WriteString(title);

    WorldPackets::Petition::PetitionBuy packet(std::move(raw));
    packet.Read();

    CHECK(packet.Title == title);
    CHECK(packet.Unit == petitioner);
    CHECK(packet.GetRawPacket()->rpos() == packet.GetRawPacket()->size());
}
