# Classic 1.60.1.70291 protocol: what is verified in the client

Everything here was read from the client itself (WowB.exe 70291, a memory dump of the running process: the executable's code is
encrypted on disk and decrypted page by page when it first runs, see `tools/protocol/README.md`). No guesses.

## Opcode numbering

- The client dispatches server messages of the main group in one function (RVA 0x8e5a90): `index = opcode - 0x470000`, bound 0x38e
  (911 messages), jump table at RVA 0x8f2308. Every message class returns its own opcode from an id method (case 0x97 returns 0x470097).
- Server groups are shifted by +1 against the 70245 numbering (main group 0x46 -> 0x47). Inside the main group one message was inserted
  at **index 0x98**; everything from 0x98 on moves up by one (`Main70291InsertIndex` in ClassicOpcodes.cpp, no longer configurable).
  - 0x93..0x96 unchanged (pet journal messages: GUID, GUID, GUID, empty).
  - **0x97 = PARTY_UPDATE**, unchanged number (the reader at RVA 0x805c30 reads the party layout; the old guess of 0x93 / 152 / 156 moved
    or kept it on a number whose layout we did not send, which is what crashed the client).
  - 0x98 = new message, fields `GUID u8 GUID u8` (not sent by us).
  - 0x99 UPDATE_RECENT_PLAYER_GUIDS (array), 0x9a READY_CHECK_STARTED (`u8 GUID GUID u64`), 0x9b READY_CHECK_RESPONSE (`GUID GUID u8`),
    0x9c READY_CHECK_COMPLETED (`u8 GUID`): all three equal our writers, one number higher.
  - 0x9d..0xa0 INSTANCE_ABANDON_VOTE_* (the handler of 0xa0 references `VOTE_TO_ABANDON_PLAYER_LEFT`), 0xa2/0xa3 PET_BATTLE_PVP_CHALLENGE and
    FINALIZE_LOCATION (shared location reader).
  - Further anchors from the official sniff are unchanged (0xAE START_ELAPSED_TIMERS, 0x124, 0x1B6, 0x1C0, 0x269, 0x2B0, 0x372).
- `tests/game/ClassicOpcodeTable.cpp` pins these.

## PARTY_UPDATE layout (client reader RVA 0x805c30)

Order of reads: `u16 flags, u8 index, u8 type, i32 myIndex, GUID party, i32 sequence, i8 (new), GUID leader, u8 faction, i32 ping,
u64 (new), i32 count`, then per member (stride 0xC0): bits `6 name size + 9 voice size (incl. NUL) + 3 flags` (3 bytes), `GUID, u8 subgroup,
u8 flags, u8 roles, u8 rolesUnk, i8 class, u8 faction`, leaver struct (GUID, f32, u32 u32 u32 i32, u64 u64, i32, 1 bit), name bytes,
voice bytes + NUL when the size is above 1; tail: one byte of 4 optional bits (challenge mode, LFG, loot, difficulty), the present blocks
in that order (LFG, loot and difficulty blocks equal our writers; the challenge block equals too).

Differences against the retail writer we had: the signed byte before the leader GUID, the u64 before the member count, and the voice size
is 9 bits, not 6. `PartyUpdate::Classic70291` (default: realm build >= 70291) selects it; the two new values are not used by the party
handler (callback RVA 0x2a3a4a0 uses index, type, GUID, sequence and the member array), so zero is sent. `tests/game/ClassicPartyUpdateLayout.cpp`
reads the packet with an independent reader that follows the client's order and requires every byte to be consumed.

The three crashes (n=56 N=49 in JamDynamicString.cpp:30, the NUL assert at RVA 0x458344c, and the follow-up) are this reader misparsing the
old layout: 0x805f7c is the name string allocation, 0x458344c the voice string "last byte must be NUL" check.

## Not verified yet

- The mapping of main-group messages in the 0x1C0..0x268 region (a Classic-only message sits somewhere there in 70245) and for many
  messages beyond the sniffed anchors: the arithmetic table is right wherever a check exists, everything else needs the case table
  (`/mnt/project-files/audit/70291-client/main-group-cases.txt`) compared with our writers one by one.
- 398 of 911 constructors were not decrypted in the dump (those messages never arrived during play). A longer session that touches
  battlegrounds, inspect, trade, auction house, mail, quests widens it (`Dump-WowBMemory.ps1 -Seconds`).
- The meaning of the two new PARTY_UPDATE values, the other groups' tables (chat 0x4C.., battleground/pvp 0x4C..), the client-bound
  opcodes of 70291 (only the connection group moved is known from the sniff).

## SMSG_FEATURE_SYSTEM_STATUS (case 0x63, reader 0x7ff4a0-0x7fffd7)

Compared read by read with `FeatureSystemStatus::Write`; test `tests/game/ClassicFeatureStatusLayout.cpp` (`[ClassicFeatureStatus]`).

- Everything before the flag run matches: u8, 8 x u32, quick join (22 x u32 + flag byte), u64, 2 x u32, squelch, restriction, content set,
  counts, the throttle block, game modes and rules.
- The flag run: 32 flags (4 bytes: bit 1 = Europa block present, bit 4 = session alert present), 7 flags, a 10 bit **string length**
  (client 0x198), 7 flags, 1 flag. Then the Europa block (a flag byte of 4 flags + 2 x 4 x u32), the session alert (3 x u32), and the string
  (`length` bytes, then the client writes the NUL itself).
- The retail layout has two guild flags where the Classic length starts. The old writer sent them plus a 10 bit "ClassicFlagBits10 = 2",
  which made the same bytes as the official packet for the official values (length 0, flag 0x5b4 on) but made the struct defaults
  (guild flags on, what a server without the official values would have sent) a length of 768: the client would read 768 bytes that are not there. The
  writer now sends a length of 0 and `ClassicFlagsAfterLength` (2 bits); the two guild members are gone.
- Not known: what the individual flags after the length are called in the client; the names stay the retail names by position.
