-- Questie (Questie team, Questie/QuestieDB, GPLv3), update 4605a11c -> 90ab55bd (12 Nov 2026): quest givers and turn-ins that
-- players' Questie recorded on the live WoW Forever realm (src/corrections/Forever/traces/foreverNpcTraces.lua,
-- foreverObjectTraces.lua, foreverQuestTraces.lua) and the hand fixes in foreverNPCFixes.lua. Written as our own rows; only
-- NPCs and objects spawned in our world and quests we have. Safe to run more than once.
--
-- Ten Camping 101 quests (Alchemy, Blacksmithing, Enchanting, Engineering, First Aid, Fishing, Herbalism, Leatherworking,
-- Mining, Tailoring: 97915-97923, 97925) had no giver at all; on live Sam Sarsaparilla gives them, as he gives Cooking (97924).
-- Crown of the Earth (928) and Teldrassil (940) had no giver either: the Lost Journal and the Oracle Tree Bark start them.

-- giver 1518 Apothecary Johaan: 95314 That Shadowvale Green Elixir
-- giver 2437 Keeper Bel'varil: 93680 Key to the City
-- giver 5395 Felgur Twocuts: 94873 Buying Friendship: Gelkis
-- giver 259961 Wharfmaster Whizzbuzz: 95988 Goes Down Smooth
-- giver 263399 Sam Sarsaparilla: 97915 Camping 101: Alchemy
-- giver 263399 Sam Sarsaparilla: 97916 Camping 101: Blacksmithing
-- giver 263399 Sam Sarsaparilla: 97917 Camping 101: Enchanting
-- giver 263399 Sam Sarsaparilla: 97918 Camping 101: Engineering
-- giver 263399 Sam Sarsaparilla: 97919 Camping 101: First Aid
-- giver 263399 Sam Sarsaparilla: 97920 Camping 101: Fishing
-- giver 263399 Sam Sarsaparilla: 97921 Camping 101: Herbalism
-- giver 263399 Sam Sarsaparilla: 97922 Camping 101: Leatherworking
-- giver 263399 Sam Sarsaparilla: 97923 Camping 101: Mining
-- giver 263399 Sam Sarsaparilla: 97925 Camping 101: Tailoring
-- giver 264072 Yorn Grimtotem: 96241 Talkin' bout Toxins, Mon
-- giver 270038 Calder Gray: 1113 Hearts of Zeal
DELETE FROM `creature_queststarter` WHERE (`id`,`quest`) IN ((1518,95314),(2437,93680),(5395,94873),(259961,95988),(263399,97915),(263399,97916),(263399,97917),(263399,97918),(263399,97919),(263399,97920),(263399,97921),(263399,97922),(263399,97923),(263399,97925),(264072,96241),(270038,1113));
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (1518,95314,0),(2437,93680,0),(5395,94873,0),(259961,95988,0),(263399,97915,0),(263399,97916,0),(263399,97917,0),(263399,97918,0),(263399,97919,0),(263399,97920,0),(263399,97921,0),(263399,97922,0),(263399,97923,0),(263399,97925,0),(264072,96241,0),(270038,1113,0);

-- turn-in 257591 Ashe Amberhall: 95148 Southern Watch Gnoll Report
DELETE FROM `creature_questender` WHERE (`id`,`quest`) IN ((257591,95148));
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (257591,95148,0);

-- giver 415106 Burned-Out Remains: 79192 Stepping Stones
-- giver 415107 Burned-Out Remains: 79192 Stepping Stones
-- giver 556670 WANTED: 566 WANTED: Baron Vardus
-- giver 654846 Lost Journal: 928 Crown of the Earth
-- giver 673378 Oracle Tree Bark: 940 Teldrassil
DELETE FROM `gameobject_queststarter` WHERE (`id`,`quest`) IN ((415106,79192),(415107,79192),(556670,566),(654846,928),(673378,940));
INSERT INTO `gameobject_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (415106,79192,0),(415107,79192,0),(556670,566,0),(654846,928,0),(673378,940,0);

-- turn-in 417072 Nailed Plank: 79008 ... and that note you found
-- turn-in 424010 Nailed Plank: 79007 ... and that note you found
DELETE FROM `gameobject_questender` WHERE (`id`,`quest`) IN ((417072,79008),(424010,79007));
INSERT INTO `gameobject_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (417072,79008,0),(424010,79007,0);

-- Camping 101: Herbalism (97968) is handed in to Halassa Fernbreeze on live, not to Syriel Nightrain (Questie NPC fix).
DELETE FROM `creature_questender` WHERE `id` = 254345 AND `quest` = 97968;
