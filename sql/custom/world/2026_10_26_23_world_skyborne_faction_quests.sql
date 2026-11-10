-- Skyborne could not take faction-wide quests. These kept the vanilla race masks (0x4D Alliance, 0xB2 Horde, 0xFF both),
-- which have no bit for race 95 (Skyborne Alliance, RaceMask bit 32) or race 96 (Skyborne Horde, bit 33), while Forever's own
-- quests use masks that include them. Affected: battleground quests and mark turn-ins, holidays, Dungeon Set 2, AQ war effort,
-- Argent Dawn, mount swaps and more. Class- and race-specific masks are left alone.
UPDATE `quest_template` SET `AllowableRaces`=`AllowableRaces`|0x100000000 WHERE `AllowableRaces` IN (0x4D,0x44D);
UPDATE `quest_template` SET `AllowableRaces`=`AllowableRaces`|0x200000000 WHERE `AllowableRaces`=0xB2;
UPDATE `quest_template` SET `AllowableRaces`=`AllowableRaces`|0x300000000 WHERE `AllowableRaces`=0xFF;
