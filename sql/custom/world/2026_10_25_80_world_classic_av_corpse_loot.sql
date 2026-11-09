-- Classic 1.60: Alterac Valley player corpse loot. The 18 AV turn-in quests (Severed Night Elf Head, Orc Tooth, ... and the
-- Wanted/MORE repeatables) and the Stormpike/Frostwolf blood, flesh and medal turn-ins need items from the corpses of enemy players.
-- VMaNGOS drops them by the victim's race and team (reference_loot_template 1). Here that reference was never used by any loot
-- entry, and its race/team conditions would be checked against the looter, who is always on the other team, so nothing dropped.
-- Player.cpp now fills AV corpse loot from entry CLASSIC_AV_CORPSE_LOOT_ENTRY (90000000) + victim race. Items and chances: VMaNGOS 1.12.
-- Safe to run again.
DELETE FROM `creature_loot_template` WHERE `Entry` BETWEEN 90000001 AND 90000008;
INSERT INTO `creature_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES
(90000001,0,18144,100,0,1,0,1,1,'Classic AV corpse loot: Human Bone Chip'),
(90000001,0,17306,100,0,1,0,1,1,'Classic AV corpse loot: Alliance victim'),
(90000001,0,17326,75,0,1,0,1,1,'Classic AV corpse loot: Alliance victim'),
(90000001,0,17327,75,0,1,0,1,1,'Classic AV corpse loot: Alliance victim'),
(90000001,0,17328,75,0,1,0,1,1,'Classic AV corpse loot: Alliance victim'),
(90000002,0,18207,100,0,1,0,1,1,'Classic AV corpse loot: Orc Tooth'),
(90000002,0,17423,100,0,1,0,1,1,'Classic AV corpse loot: Horde victim'),
(90000002,0,17502,75,0,1,0,1,1,'Classic AV corpse loot: Horde victim'),
(90000002,0,17503,75,0,1,0,1,1,'Classic AV corpse loot: Horde victim'),
(90000002,0,17504,75,0,1,0,1,1,'Classic AV corpse loot: Horde victim'),
(90000003,0,18206,100,0,1,0,1,1,'Classic AV corpse loot: Dwarf Spine'),
(90000003,0,17306,100,0,1,0,1,1,'Classic AV corpse loot: Alliance victim'),
(90000003,0,17326,75,0,1,0,1,1,'Classic AV corpse loot: Alliance victim'),
(90000003,0,17327,75,0,1,0,1,1,'Classic AV corpse loot: Alliance victim'),
(90000003,0,17328,75,0,1,0,1,1,'Classic AV corpse loot: Alliance victim'),
(90000004,0,18142,100,0,1,0,1,1,'Classic AV corpse loot: Severed Night Elf Head'),
(90000004,0,17306,100,0,1,0,1,1,'Classic AV corpse loot: Alliance victim'),
(90000004,0,17326,75,0,1,0,1,1,'Classic AV corpse loot: Alliance victim'),
(90000004,0,17327,75,0,1,0,1,1,'Classic AV corpse loot: Alliance victim'),
(90000004,0,17328,75,0,1,0,1,1,'Classic AV corpse loot: Alliance victim'),
(90000005,0,18147,100,0,1,0,1,1,'Classic AV corpse loot: Forsaken Heart'),
(90000005,0,17423,100,0,1,0,1,1,'Classic AV corpse loot: Horde victim'),
(90000005,0,17502,75,0,1,0,1,1,'Classic AV corpse loot: Horde victim'),
(90000005,0,17503,75,0,1,0,1,1,'Classic AV corpse loot: Horde victim'),
(90000005,0,17504,75,0,1,0,1,1,'Classic AV corpse loot: Horde victim'),
(90000006,0,18145,100,0,1,0,1,1,'Classic AV corpse loot: Tauren Hoof'),
(90000006,0,17423,100,0,1,0,1,1,'Classic AV corpse loot: Horde victim'),
(90000006,0,17502,75,0,1,0,1,1,'Classic AV corpse loot: Horde victim'),
(90000006,0,17503,75,0,1,0,1,1,'Classic AV corpse loot: Horde victim'),
(90000006,0,17504,75,0,1,0,1,1,'Classic AV corpse loot: Horde victim'),
(90000007,0,18143,100,0,1,0,1,1,'Classic AV corpse loot: Tuft of Gnome Hair'),
(90000007,0,17306,100,0,1,0,1,1,'Classic AV corpse loot: Alliance victim'),
(90000007,0,17326,75,0,1,0,1,1,'Classic AV corpse loot: Alliance victim'),
(90000007,0,17327,75,0,1,0,1,1,'Classic AV corpse loot: Alliance victim'),
(90000007,0,17328,75,0,1,0,1,1,'Classic AV corpse loot: Alliance victim'),
(90000008,0,18146,100,0,1,0,1,1,'Classic AV corpse loot: Darkspear Troll Mojo'),
(90000008,0,17423,100,0,1,0,1,1,'Classic AV corpse loot: Horde victim'),
(90000008,0,17502,75,0,1,0,1,1,'Classic AV corpse loot: Horde victim'),
(90000008,0,17503,75,0,1,0,1,1,'Classic AV corpse loot: Horde victim'),
(90000008,0,17504,75,0,1,0,1,1,'Classic AV corpse loot: Horde victim');
