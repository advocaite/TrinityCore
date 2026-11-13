#include "tc_catch2.h"
#include "TransmogrificationPackets.h"
#include "Util.h"

// CMSG_TRANSMOG_OUTFIT_UPDATE_SLOTS from the classic 1.60 client: it sends every outfit slot and slot option, 45 entries
// (TransmogOutfitSlotInfo 15 rows, TransmogOutfitSlotOption 42 rows). The packet took at most 30, so every outfit save was
// refused whole and armor changes never stuck (player reports 2026-11-13).
namespace
{
WorldPacket MakeUpdateSlots(uint32 outfitId, ObjectGuid const& npc, uint64 cost, uint32 count, bool discount)
{
    WorldPacket raw(CMSG_TRANSMOG_OUTFIT_UPDATE_SLOTS);
    raw << uint32(outfitId);
    raw << uint32(count);
    raw << npc;
    raw << uint64(cost);
    for (uint32 i = 0; i < count; ++i)
    {
        // written in reverse slot order: Read() sorts by slot and option
        uint32 const n = count - 1 - i;
        raw << int8(n / 4);         // Slot
        raw << uint8(n % 4);        // SlotOption
        raw << uint8(0);            // SheatheCategory
        raw << uint32(1000 + n);    // ItemModifiedAppearanceID
        raw << uint8(1);            // AppearanceDisplayType
        raw << uint32(0);           // SpellItemEnchantmentID
        raw << uint8(0);            // IllusionDisplayType
        raw << uint32(0);           // Flags
    }
    raw.WriteBit(discount);
    raw.FlushBits();
    return raw;
}
}

TEST_CASE("TRANSMOG_OUTFIT_UPDATE_SLOTS reads all 45 classic outfit slots", "[TransmogOutfitSlotsLayout]")
{
    ObjectGuid const npc = ObjectGuid::Create<HighGuid::Creature>(0, 4974, UI64LIT(30001234));

    WorldPackets::Transmogrification::TransmogOutfitUpdateSlots packet(MakeUpdateSlots(7, npc, 210, 45, true));
    packet.Read();

    CHECK(packet.OutfitID == 7);
    CHECK(packet.Npc == npc);
    CHECK(packet.Cost == 210);
    CHECK(packet.UseAvailableDiscount);
    REQUIRE(packet.Slots.size() == 45);
    for (uint32 n = 0; n < 45; ++n)
    {
        CHECK(AsUnderlyingType(packet.Slots[n].Slot) == int8(n / 4));
        CHECK(AsUnderlyingType(packet.Slots[n].SlotOption) == uint8(n % 4));
        CHECK(packet.Slots[n].ItemModifiedAppearanceID == 1000 + n);
    }
    CHECK(packet.GetRawPacket()->rpos() == packet.GetRawPacket()->size());
}

TEST_CASE("TRANSMOG_OUTFIT_UPDATE_SLOTS still refuses more slots than the client has", "[TransmogOutfitSlotsLayout]")
{
    ObjectGuid const npc = ObjectGuid::Create<HighGuid::Creature>(0, 4974, UI64LIT(30001234));

    WorldPackets::Transmogrification::TransmogOutfitUpdateSlots packet(MakeUpdateSlots(7, npc, 0, 46, false));
    CHECK_THROWS(packet.Read());
}
