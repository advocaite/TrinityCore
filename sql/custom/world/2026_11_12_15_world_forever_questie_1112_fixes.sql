-- Questie (Questie team, Questie/QuestieDB, GPLv3), update e19dfdf8 -> 4605a11c (11 to 12 Nov): src/corrections/Forever/foreverQuestFixes.lua,
-- foreverNPCFixes.lua, traces/foreverQuestTraces.lua (quest givers and turn-ins players' Questie recorded on the live realm) and
-- support/Forever/DropTables/itemDropCorrections.lua. Written as our own rows. Safe to run more than once.

-- 1. Quest order. Questie's "unverified" links (Return the Statuette after 286, Yelmak's Medley after Meal Appeal) are left out,
--    and so is The Hermit Tanner after Bruised Pride: ours follows Vrang's Game, and Blizzard's own record names The Hermit Tanner
--    as the follow-up of both quests.
INSERT INTO `quest_template_addon` (`ID`,`PrevQuestID`) VALUES (96261,96259) ON DUPLICATE KEY UPDATE `PrevQuestID` = VALUES(`PrevQuestID`); -- The Broodmother after Defending the Dead (same giver, Questie fix 6aa47ac)
INSERT INTO `quest_template_addon` (`ID`,`PrevQuestID`) VALUES (97275,97242) ON DUPLICATE KEY UPDATE `PrevQuestID` = VALUES(`PrevQuestID`); -- Whuut's the Rush after Yelmak's Medley
INSERT INTO `quest_template_addon` (`ID`,`PrevQuestID`) VALUES (97326,97249) ON DUPLICATE KEY UPDATE `PrevQuestID` = VALUES(`PrevQuestID`); -- Rocks to Rests after Favorite Food
INSERT INTO `quest_template_addon` (`ID`,`PrevQuestID`) VALUES (98191,98190) ON DUPLICATE KEY UPDATE `PrevQuestID` = VALUES(`PrevQuestID`); -- the second A Friend of the Family after the first (it starts where the first ends)
INSERT INTO `quest_template_addon` (`ID`,`PrevQuestID`) VALUES (94616,94501) ON DUPLICATE KEY UPDATE `PrevQuestID` = VALUES(`PrevQuestID`); -- Water Sapta after the fifth Call of Water step
UPDATE `quest_template_addon` SET `SpecialFlags` = `SpecialFlags` | 1 WHERE `ID` = 94616; -- Water Sapta can be taken again (repeatable)

-- 2. Savannah Lion Hide (269715): drop chances from Questie's drop table (Wowhead Forever counts) replace our 40% guess.
UPDATE `creature_loot_template` SET `Chance` = 31.42, `Comment` = 'Questie drop table (Wowhead Forever): 31.42%' WHERE `Entry` = 3241 AND `Item` = 269715 AND `ItemType` = 0; -- Savannah Patriarch
UPDATE `creature_loot_template` SET `Chance` = 4.08, `Comment` = 'Questie drop table (Wowhead Forever): 4.08%' WHERE `Entry` = 3416 AND `Item` = 269715 AND `ItemType` = 0; -- Savannah Matriarch

-- 3. Quest givers and turn-ins Questie's players recorded that we lack: 23 givers, 14 turn-ins. A Sealed Crate 91899/91904/91905
--    had no giver at all. Links Questie's own fixes remove are left out (Senir Whitebeard 96608, Eric Brighthammer 420,
--    Valennia Stormfist 93948, Zamja 97246, 252476 for 92579, 3188 for 97225). All NPCs here are spawned in our world.
DELETE FROM `creature_queststarter` WHERE (`id`,`quest`) IN ((256386,91899),(256390,91899),(256390,91900),(256386,91904),(256390,91904),(256386,91905),(256390,91905),(2437,92434),(251366,92467),(253140,93227),(253590,93949),(253844,93949),(257446,94449),(258113,94494),(259119,94911),(259049,94999),(5397,96222),(258098,96243),(1495,97558),(4722,98070),(1661,98601),(383,99143),(6806,99158));
--    giver 256386: Dokimi (91899) A Sealed Crate
--    giver 256390: Marcy Baker (91899) A Sealed Crate
--    giver 256390: Marcy Baker (91900) A Sealed Crate
--    giver 256386: Dokimi (91904) A Sealed Crate
--    giver 256390: Marcy Baker (91904) A Sealed Crate
--    giver 256386: Dokimi (91905) A Sealed Crate
--    giver 256390: Marcy Baker (91905) A Sealed Crate
--    giver 2437: Keeper Bel'varil (92434) Blood in the Streets
--    giver 251366: Aetheen of the Gales (92467) Call of Earth
--    giver 253140: Molkar (93227) Buying Friendship: Gelkis
--    giver 253590: Valennia Stormfist (93949) Bugged
--    giver 253844: Valennia Stormfist (93949) Bugged
--    giver 257446: Teo Hammerstorm (94449) Call of Fire
--    giver 258113: Ingrid Dunwald (94494) Call of Water
--    giver 259119: Alaana Stormwalker (94911) Child of Nature
--    giver 259049: Robbie Holmsby (94999) Eyes for the Watchman
--    giver 5397: Uthek the Wise (96222) Khan Hratha
--    giver 258098: Eldrun Stormbreaker (96243) Call of Earth
--    giver 1495: Deathguard Linnea (97558) Hides for the Forsaken
--    giver 4722: Rau Cliffrunner (98070) Stop the Screeching
--    giver 1661: Novice Elreth (98601) A Difficult Path
--    giver 383: Jason Mathers (99143) Bottles and Baubles
--    giver 6806: Tannok Frosthammer (99158) Dawn in the Mountains
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (256386,91899,0),(256390,91899,0),(256390,91900,0),(256386,91904,0),(256390,91904,0),(256386,91905,0),(256390,91905,0),(2437,92434,0),(251366,92467,0),(253140,93227,0),(253590,93949,0),(253844,93949,0),(257446,94449,0),(258113,94494,0),(259119,94911,0),(259049,94999,0),(5397,96222,0),(258098,96243,0),(1495,97558,0),(4722,98070,0),(1661,98601,0),(383,99143,0),(6806,99158,0);
DELETE FROM `creature_questender` WHERE (`id`,`quest`) IN ((258466,769),(211022,79091),(211033,79094),(211033,79095),(268,79363),(257558,94879),(7941,95219),(259118,95349),(1076,95810),(5953,96822),(246378,96896),(377,97919),(3610,98403),(1423,99129));
--    turn-in 258466: Boramu (769) Kodo Hide Bag
--    turn-in 211022: Owen Thadd (79091) Archmage Antonidas: The Unabridged Autobiography
--    turn-in 211033: Garion Wendell (79094) The Lessons of Ta'zo
--    turn-in 211033: Garion Wendell (79095) The Apothecary's Metaphysical Primer
--    turn-in 268: Sirra Von'Indi (79363) Silvia's Sword
--    turn-in 257558: Demar Brant (94879) Scraps of Knowledge
--    turn-in 7941: Mardrack Greenwell (95219) From Port to Port
--    turn-in 259118: Muln Earthfury (95349) The Earthen Ring
--    turn-in 1076: Merrin Rockweaver (95810) Lost Relic Carry
--    turn-in 5953: Razor Hill Grunt (96822) For Honor
--    turn-in 246378: Danitha Morr (96896) A Righteous Cause
--    turn-in 377: Priestess Josetta (97919) Camping 101: First Aid
--    turn-in 3610: Jeena Featherbow (98403) Twisted Hatred
--    turn-in 1423: Stormwind Guard (99129) A Man About a Murloc
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (258466,769,0),(211022,79091,0),(211033,79094,0),(211033,79095,0),(268,79363,0),(257558,94879,0),(7941,95219,0),(259118,95349,0),(1076,95810,0),(5953,96822,0),(246378,96896,0),(377,97919,0),(3610,98403,0),(1423,99129,0);
--    Stormwind Guard (1423), Jeena Featherbow (3610), Razor Hill Grunt (5953), Mardrack Greenwell (7941), Boramu (258466) had no quest flag; a player turning a quest in to them on live means they have it there.
UPDATE `creature_template` SET `npcflag` = `npcflag` | 2 WHERE `entry` IN (1423,3610,5953,7941,258466);
