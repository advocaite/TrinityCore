-- Classic 1.60 (WoW Forever): quest XP and quest level from live player traces (Questie team, Questie/QuestieDB
-- support/Forever/QuestXP/xpDB-traces.lua, 2026-11-08, 724 quests). Every trace value is one QuestXP.db2 column at its level,
-- so each quest gets that column with multiplier 1 and the traced level. Quests that already give the traced XP are not
-- listed. Left out on purpose: traced XP that equals our XP cut by the over-level penalty (10-80%), and traces in the
-- lowest column for quests whose current values were not checked, because those can come from players far above the quest.
-- Rows that do not exist are not created. Safe to run again.
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=65610; -- Wish You Were Here: 2200 at level 27 (was 1550 at level 20)
UPDATE `quest_template_classic_level` SET `QuestLevel`=27 WHERE `ID`=65610;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 27 FROM `quest_template` WHERE `ID`=65610;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=78088; -- A Strange Artifact: 2450 at level 30 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=30 WHERE `ID`=78088;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 30 FROM `quest_template` WHERE `ID`=78088;
UPDATE `quest_template` SET `RewardXPDifficulty`=4, `RewardXPMultiplier`=1 WHERE `ID`=78089; -- Advice From Stormwind: 1850 at level 30 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=30 WHERE `ID`=78089;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 30 FROM `quest_template` WHERE `ID`=78089;
UPDATE `quest_template` SET `RewardXPDifficulty`=6, `RewardXPMultiplier`=1 WHERE `ID`=78092; -- It Must Be Destroyed: 3050 at level 30 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=30 WHERE `ID`=78092;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 30 FROM `quest_template` WHERE `ID`=78092;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=78261; -- The Horn of Xelthos: 1850 at level 23 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=23 WHERE `ID`=78261;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 23 FROM `quest_template` WHERE `ID`=78261;
UPDATE `quest_template` SET `RewardXPDifficulty`=6, `RewardXPMultiplier`=1 WHERE `ID`=78676; -- The Eye of Bhossca: 3450 at level 35 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=35 WHERE `ID`=78676;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 35 FROM `quest_template` WHERE `ID`=78676;
UPDATE `quest_template` SET `RewardXPDifficulty`=3, `RewardXPMultiplier`=1 WHERE `ID`=78823; -- Terror of the Desert Skies: 1550 at level 40 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=40 WHERE `ID`=78823;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 40 FROM `quest_template` WHERE `ID`=78823;
UPDATE `quest_template` SET `RewardXPDifficulty`=6, `RewardXPMultiplier`=1 WHERE `ID`=78830; -- quest: 3900 at level 40 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=40 WHERE `ID`=78830;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 40 FROM `quest_template` WHERE `ID`=78830;
UPDATE `quest_template` SET `RewardXPDifficulty`=3, `RewardXPMultiplier`=1 WHERE `ID`=79229; -- Highway Robbery: 1550 at level 40 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=40 WHERE `ID`=79229;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 40 FROM `quest_template` WHERE `ID`=79229;
UPDATE `quest_template` SET `RewardXPDifficulty`=3, `RewardXPMultiplier`=1 WHERE `ID`=79235; -- quest: 1550 at level 40 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=40 WHERE `ID`=79235;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 40 FROM `quest_template` WHERE `ID`=79235;
UPDATE `quest_template` SET `RewardXPDifficulty`=3, `RewardXPMultiplier`=1 WHERE `ID`=79236; -- quest: 1550 at level 40 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=40 WHERE `ID`=79236;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 40 FROM `quest_template` WHERE `ID`=79236;
UPDATE `quest_template` SET `RewardXPDifficulty`=6, `RewardXPMultiplier`=1 WHERE `ID`=79242; -- quest: 3900 at level 40 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=40 WHERE `ID`=79242;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 40 FROM `quest_template` WHERE `ID`=79242;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=79363; -- Silvia's Sword: 2200 at level 27 (was 2100 at level 26)
UPDATE `quest_template_classic_level` SET `QuestLevel`=27 WHERE `ID`=79363;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 27 FROM `quest_template` WHERE `ID`=79363;
UPDATE `quest_template` SET `RewardXPDifficulty`=3, `RewardXPMultiplier`=1 WHERE `ID`=79624; -- Anyone Can Cook: 1550 at level 40 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=40 WHERE `ID`=79624;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 40 FROM `quest_template` WHERE `ID`=79624;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=79677; -- quest: 3150 at level 40 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=40 WHERE `ID`=79677;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 40 FROM `quest_template` WHERE `ID`=79677;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=86585; -- Banner of the Fallen: 1250 at level 17 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=17 WHERE `ID`=86585;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 17 FROM `quest_template` WHERE `ID`=86585;
UPDATE `quest_template` SET `RewardXPDifficulty`=3, `RewardXPMultiplier`=1 WHERE `ID`=86667; -- Snowbound: 420 at level 10 (was 1550 at level 20)
UPDATE `quest_template_classic_level` SET `QuestLevel`=10 WHERE `ID`=86667;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 10 FROM `quest_template` WHERE `ID`=86667;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=86757; -- Slowing the Orc Advance: 2450 at level 30 (was 2550 at level 32)
UPDATE `quest_template_classic_level` SET `QuestLevel`=30 WHERE `ID`=86757;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 30 FROM `quest_template` WHERE `ID`=86757;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=87491; -- Spark of Freedom: 2450 at level 30 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=30 WHERE `ID`=87491;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 30 FROM `quest_template` WHERE `ID`=87491;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=87760; -- Holy Diver: 1550 at level 20 (was 0 at level 20)
UPDATE `quest_template_classic_level` SET `QuestLevel`=20 WHERE `ID`=87760;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 20 FROM `quest_template` WHERE `ID`=87760;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=88756; -- Bring Back a Bang: 2500 at level 31 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=31 WHERE `ID`=88756;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 31 FROM `quest_template` WHERE `ID`=88756;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=88757; -- Accessories of the Lost: 2650 at level 33 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=33 WHERE `ID`=88757;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 33 FROM `quest_template` WHERE `ID`=88757;
UPDATE `quest_template` SET `RewardXPDifficulty`=4, `RewardXPMultiplier`=1 WHERE `ID`=91723; -- Delicate Instruments: 530 at level 8 (was 630 at level 10)
UPDATE `quest_template_classic_level` SET `QuestLevel`=8 WHERE `ID`=91723;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 8 FROM `quest_template` WHERE `ID`=91723;
UPDATE `quest_template` SET `RewardXPDifficulty`=3, `RewardXPMultiplier`=1 WHERE `ID`=91732; -- Good Steel: 315 at level 7 (was 630 at level 10)
UPDATE `quest_template_classic_level` SET `QuestLevel`=7 WHERE `ID`=91732;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 7 FROM `quest_template` WHERE `ID`=91732;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=91740; -- Croaky's Head: 880 at level 11 (was 840 at level 10)
UPDATE `quest_template_classic_level` SET `QuestLevel`=11 WHERE `ID`=91740;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 11 FROM `quest_template` WHERE `ID`=91740;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=91746; -- Elmpaw's Head: 910 at level 12 (was 840 at level 10)
UPDATE `quest_template_classic_level` SET `QuestLevel`=12 WHERE `ID`=91746;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 12 FROM `quest_template` WHERE `ID`=91746;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=91751; -- Rough Wolf Pelts: 700 at level 8 (was 475 at level 7)
UPDATE `quest_template_classic_level` SET `QuestLevel`=8 WHERE `ID`=91751;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 8 FROM `quest_template` WHERE `ID`=91751;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=91772; -- Shhh! We're Hunting Kobolds: 355 at level 4 (was 270 at level 4)
UPDATE `quest_template_classic_level` SET `QuestLevel`=4 WHERE `ID`=91772;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 4 FROM `quest_template` WHERE `ID`=91772;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=92743; -- A Conqueror's Crown: 3150 at level 40 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=40 WHERE `ID`=92743;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 40 FROM `quest_template` WHERE `ID`=92743;
UPDATE `quest_template` SET `RewardXPDifficulty`=4, `RewardXPMultiplier`=1 WHERE `ID`=92747; -- Moonbrook Espionage: 800 at level 15 (was 580 at level 16)
UPDATE `quest_template_classic_level` SET `QuestLevel`=15 WHERE `ID`=92747;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 15 FROM `quest_template` WHERE `ID`=92747;
UPDATE `quest_template` SET `RewardXPDifficulty`=1, `RewardXPMultiplier`=1 WHERE `ID`=92750; -- Detonation at a Distance: 115 at level 16 (was 580 at level 16)
UPDATE `quest_template_classic_level` SET `QuestLevel`=16 WHERE `ID`=92750;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 16 FROM `quest_template` WHERE `ID`=92750;
UPDATE `quest_template` SET `RewardXPDifficulty`=1, `RewardXPMultiplier`=1 WHERE `ID`=92751; -- Detonation at a Distance: 115 at level 16 (was 580 at level 16)
UPDATE `quest_template_classic_level` SET `QuestLevel`=16 WHERE `ID`=92751;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 16 FROM `quest_template` WHERE `ID`=92751;
UPDATE `quest_template` SET `RewardXPDifficulty`=4, `RewardXPMultiplier`=1 WHERE `ID`=92755; -- Mysterious Mysticism: 2050 at level 35 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=35 WHERE `ID`=92755;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 35 FROM `quest_template` WHERE `ID`=92755;
UPDATE `quest_template` SET `RewardXPDifficulty`=4, `RewardXPMultiplier`=1 WHERE `ID`=92814; -- I Need a Tailor: 2050 at level 35 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=35 WHERE `ID`=92814;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 35 FROM `quest_template` WHERE `ID`=92814;
UPDATE `quest_template` SET `RewardXPDifficulty`=4, `RewardXPMultiplier`=1 WHERE `ID`=92817; -- Leather Technique: 2050 at level 35 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=35 WHERE `ID`=92817;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 35 FROM `quest_template` WHERE `ID`=92817;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=92867; -- Ground to Keep: 3000 at level 39 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=39 WHERE `ID`=92867;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 39 FROM `quest_template` WHERE `ID`=92867;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=92872; -- Dutiful Steps: 2850 at level 38 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=38 WHERE `ID`=92872;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 38 FROM `quest_template` WHERE `ID`=92872;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=92911; -- Harvesting the Harvesters: 1050 at level 15 (was 800 at level 15)
UPDATE `quest_template_classic_level` SET `QuestLevel`=15 WHERE `ID`=92911;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 15 FROM `quest_template` WHERE `ID`=92911;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=92914; -- quest: 3600 at level 43 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=43 WHERE `ID`=92914;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 43 FROM `quest_template` WHERE `ID`=92914;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=93176; -- My Little Friends: 2750 at level 35 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=35 WHERE `ID`=93176;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 35 FROM `quest_template` WHERE `ID`=93176;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=93429; -- Blood of My Blood: 2750 at level 35 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=35 WHERE `ID`=93429;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 35 FROM `quest_template` WHERE `ID`=93429;
UPDATE `quest_template` SET `RewardXPDifficulty`=4, `RewardXPMultiplier`=1 WHERE `ID`=93430; -- Taking the Bait Back: 2050 at level 35 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=35 WHERE `ID`=93430;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 35 FROM `quest_template` WHERE `ID`=93430;
UPDATE `quest_template` SET `RewardXPDifficulty`=3, `RewardXPMultiplier`=1 WHERE `ID`=93464; -- The Visiting Mage: 1450 at level 38 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=38 WHERE `ID`=93464;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 38 FROM `quest_template` WHERE `ID`=93464;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=93644; -- quest: 3900 at level 45 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=45 WHERE `ID`=93644;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 45 FROM `quest_template` WHERE `ID`=93644;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=93786; -- quest: 2800 at level 36 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=36 WHERE `ID`=93786;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 36 FROM `quest_template` WHERE `ID`=93786;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=93787; -- quest: 2800 at level 36 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=36 WHERE `ID`=93787;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 36 FROM `quest_template` WHERE `ID`=93787;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=93788; -- quest: 2850 at level 38 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=38 WHERE `ID`=93788;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 38 FROM `quest_template` WHERE `ID`=93788;
UPDATE `quest_template` SET `RewardXPDifficulty`=4, `RewardXPMultiplier`=1 WHERE `ID`=94481; -- quest: 2150 at level 38 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=38 WHERE `ID`=94481;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 38 FROM `quest_template` WHERE `ID`=94481;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=94483; -- quest: 3000 at level 39 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=39 WHERE `ID`=94483;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 39 FROM `quest_template` WHERE `ID`=94483;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=94614; -- quest: 2850 at level 38 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=38 WHERE `ID`=94614;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 38 FROM `quest_template` WHERE `ID`=94614;
UPDATE `quest_template` SET `RewardXPDifficulty`=3, `RewardXPMultiplier`=1 WHERE `ID`=94615; -- quest: 1450 at level 38 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=38 WHERE `ID`=94615;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 38 FROM `quest_template` WHERE `ID`=94615;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=94617; -- quest: 2850 at level 38 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=38 WHERE `ID`=94617;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 38 FROM `quest_template` WHERE `ID`=94617;
UPDATE `quest_template` SET `RewardXPDifficulty`=3, `RewardXPMultiplier`=1 WHERE `ID`=94618; -- quest: 1450 at level 38 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=38 WHERE `ID`=94618;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 38 FROM `quest_template` WHERE `ID`=94618;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=94678; -- quest: 2850 at level 38 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=38 WHERE `ID`=94678;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 38 FROM `quest_template` WHERE `ID`=94678;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=94679; -- quest: 3150 at level 40 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=40 WHERE `ID`=94679;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 40 FROM `quest_template` WHERE `ID`=94679;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=94734; -- quest: 3300 at level 41 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=41 WHERE `ID`=94734;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 41 FROM `quest_template` WHERE `ID`=94734;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=94745; -- quest: 3750 at level 44 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=44 WHERE `ID`=94745;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 44 FROM `quest_template` WHERE `ID`=94745;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=94748; -- quest: 3750 at level 44 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=44 WHERE `ID`=94748;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 44 FROM `quest_template` WHERE `ID`=94748;
UPDATE `quest_template` SET `RewardXPDifficulty`=2, `RewardXPMultiplier`=1 WHERE `ID`=94775; -- Divine Grace: 210 at level 10 (was 85 at level 10)
UPDATE `quest_template_classic_level` SET `QuestLevel`=10 WHERE `ID`=94775;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 10 FROM `quest_template` WHERE `ID`=94775;
UPDATE `quest_template` SET `RewardXPDifficulty`=2, `RewardXPMultiplier`=1 WHERE `ID`=94776; -- Divine Grace: 210 at level 10 (was 85 at level 10)
UPDATE `quest_template_classic_level` SET `QuestLevel`=10 WHERE `ID`=94776;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 10 FROM `quest_template` WHERE `ID`=94776;
UPDATE `quest_template` SET `RewardXPDifficulty`=2, `RewardXPMultiplier`=1 WHERE `ID`=94777; -- Divine Grace: 210 at level 10 (was 85 at level 10)
UPDATE `quest_template_classic_level` SET `QuestLevel`=10 WHERE `ID`=94777;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 10 FROM `quest_template` WHERE `ID`=94777;
UPDATE `quest_template` SET `RewardXPDifficulty`=2, `RewardXPMultiplier`=1 WHERE `ID`=94820; -- Contingency Plan: 390 at level 20 (was 155 at level 20)
UPDATE `quest_template_classic_level` SET `QuestLevel`=20 WHERE `ID`=94820;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 20 FROM `quest_template` WHERE `ID`=94820;
UPDATE `quest_template` SET `RewardXPDifficulty`=2, `RewardXPMultiplier`=1 WHERE `ID`=94823; -- Confounding Flash: 210 at level 10 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=10 WHERE `ID`=94823;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 10 FROM `quest_template` WHERE `ID`=94823;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=94842; -- quest: 2850 at level 38 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=38 WHERE `ID`=94842;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 38 FROM `quest_template` WHERE `ID`=94842;
UPDATE `quest_template` SET `RewardXPDifficulty`=4, `RewardXPMultiplier`=1 WHERE `ID`=94843; -- quest: 2150 at level 38 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=38 WHERE `ID`=94843;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 38 FROM `quest_template` WHERE `ID`=94843;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=94861; -- quest: 3000 at level 39 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=39 WHERE `ID`=94861;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 39 FROM `quest_template` WHERE `ID`=94861;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=94877; -- quest: 3150 at level 40 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=40 WHERE `ID`=94877;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 40 FROM `quest_template` WHERE `ID`=94877;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=94879; -- quest: 3150 at level 40 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=40 WHERE `ID`=94879;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 40 FROM `quest_template` WHERE `ID`=94879;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=94881; -- quest: 2850 at level 38 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=38 WHERE `ID`=94881;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 38 FROM `quest_template` WHERE `ID`=94881;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=94885; -- quest: 3150 at level 40 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=40 WHERE `ID`=94885;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 40 FROM `quest_template` WHERE `ID`=94885;
UPDATE `quest_template` SET `RewardXPDifficulty`=3, `RewardXPMultiplier`=1 WHERE `ID`=94887; -- quest: 2100 at level 47 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=47 WHERE `ID`=94887;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 47 FROM `quest_template` WHERE `ID`=94887;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=94907; -- quest: 3750 at level 44 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=44 WHERE `ID`=94907;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 44 FROM `quest_template` WHERE `ID`=94907;
UPDATE `quest_template` SET `RewardXPDifficulty`=4, `RewardXPMultiplier`=1 WHERE `ID`=94923; -- quest: 2150 at level 38 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=38 WHERE `ID`=94923;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 38 FROM `quest_template` WHERE `ID`=94923;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=94924; -- quest: 3000 at level 39 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=39 WHERE `ID`=94924;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 39 FROM `quest_template` WHERE `ID`=94924;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=94925; -- quest: 3000 at level 39 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=39 WHERE `ID`=94925;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 39 FROM `quest_template` WHERE `ID`=94925;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=94931; -- quest: 2850 at level 38 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=38 WHERE `ID`=94931;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 38 FROM `quest_template` WHERE `ID`=94931;
UPDATE `quest_template` SET `RewardXPDifficulty`=4, `RewardXPMultiplier`=1 WHERE `ID`=94961; -- quest: 2350 at level 40 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=40 WHERE `ID`=94961;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 40 FROM `quest_template` WHERE `ID`=94961;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=94963; -- quest: 3150 at level 40 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=40 WHERE `ID`=94963;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 40 FROM `quest_template` WHERE `ID`=94963;
UPDATE `quest_template` SET `RewardXPDifficulty`=6, `RewardXPMultiplier`=1 WHERE `ID`=94965; -- quest: 4650 at level 44 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=44 WHERE `ID`=94965;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 44 FROM `quest_template` WHERE `ID`=94965;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=94992; -- quest: 2850 at level 38 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=38 WHERE `ID`=94992;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 38 FROM `quest_template` WHERE `ID`=94992;
UPDATE `quest_template` SET `RewardXPDifficulty`=2, `RewardXPMultiplier`=1 WHERE `ID`=94999; -- quest: 700 at level 36 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=36 WHERE `ID`=94999;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 36 FROM `quest_template` WHERE `ID`=94999;
UPDATE `quest_template` SET `RewardXPDifficulty`=6, `RewardXPMultiplier`=1 WHERE `ID`=95063; -- quest: 4850 at level 45 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=45 WHERE `ID`=95063;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 45 FROM `quest_template` WHERE `ID`=95063;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=95064; -- quest: 3000 at level 39 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=39 WHERE `ID`=95064;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 39 FROM `quest_template` WHERE `ID`=95064;
UPDATE `quest_template` SET `RewardXPDifficulty`=4, `RewardXPMultiplier`=1 WHERE `ID`=95065; -- Fishin' Time: 680 at level 12 (was 910 at level 12)
UPDATE `quest_template_classic_level` SET `QuestLevel`=12 WHERE `ID`=95065;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 12 FROM `quest_template` WHERE `ID`=95065;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=95119; -- quest: 3600 at level 43 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=43 WHERE `ID`=95119;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 43 FROM `quest_template` WHERE `ID`=95119;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=95122; -- quest: 3750 at level 44 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=44 WHERE `ID`=95122;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 44 FROM `quest_template` WHERE `ID`=95122;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=95123; -- quest: 3750 at level 44 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=44 WHERE `ID`=95123;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 44 FROM `quest_template` WHERE `ID`=95123;
UPDATE `quest_template` SET `RewardXPDifficulty`=4, `RewardXPMultiplier`=1 WHERE `ID`=95156; -- quest: 2100 at level 37 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=37 WHERE `ID`=95156;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 37 FROM `quest_template` WHERE `ID`=95156;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=95162; -- quest: 3300 at level 41 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=41 WHERE `ID`=95162;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 41 FROM `quest_template` WHERE `ID`=95162;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=95184; -- quest: 2850 at level 38 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=38 WHERE `ID`=95184;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 38 FROM `quest_template` WHERE `ID`=95184;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=95254; -- quest: 3450 at level 42 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=42 WHERE `ID`=95254;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 42 FROM `quest_template` WHERE `ID`=95254;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=95288; -- quest: 3600 at level 43 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=43 WHERE `ID`=95288;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 43 FROM `quest_template` WHERE `ID`=95288;
UPDATE `quest_template` SET `RewardXPDifficulty`=3, `RewardXPMultiplier`=1 WHERE `ID`=95293; -- quest: 1700 at level 42 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=42 WHERE `ID`=95293;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 42 FROM `quest_template` WHERE `ID`=95293;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=95328; -- Whispering Horror Residue: 910 at level 13 (was 840 at level 10)
UPDATE `quest_template_classic_level` SET `QuestLevel`=13 WHERE `ID`=95328;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 13 FROM `quest_template` WHERE `ID`=95328;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=95384; -- Partial Mystical Notes: 3150 at level 40 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=40 WHERE `ID`=95384;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 40 FROM `quest_template` WHERE `ID`=95384;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=95426; -- quest: 3000 at level 39 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=39 WHERE `ID`=95426;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 39 FROM `quest_template` WHERE `ID`=95426;
UPDATE `quest_template` SET `RewardXPDifficulty`=1, `RewardXPMultiplier`=1 WHERE `ID`=95495; -- The Hermit Tanner: 135 at level 18 (was 340 at level 18)
UPDATE `quest_template_classic_level` SET `QuestLevel`=18 WHERE `ID`=95495;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 18 FROM `quest_template` WHERE `ID`=95495;
UPDATE `quest_template` SET `RewardXPDifficulty`=4, `RewardXPMultiplier`=1 WHERE `ID`=95507; -- Vrang's Game: 1000 at level 18 (was 1350 at level 18)
UPDATE `quest_template_classic_level` SET `QuestLevel`=18 WHERE `ID`=95507;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 18 FROM `quest_template` WHERE `ID`=95507;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=95508; -- Unwelcome Guests: 1350 at level 18 (was 1000 at level 18)
UPDATE `quest_template_classic_level` SET `QuestLevel`=18 WHERE `ID`=95508;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 18 FROM `quest_template` WHERE `ID`=95508;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=95534; -- Strahnbrad Mystery: 2800 at level 36 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=36 WHERE `ID`=95534;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 36 FROM `quest_template` WHERE `ID`=95534;
UPDATE `quest_template` SET `RewardXPDifficulty`=3, `RewardXPMultiplier`=1 WHERE `ID`=95561; -- quest: 1450 at level 38 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=38 WHERE `ID`=95561;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 38 FROM `quest_template` WHERE `ID`=95561;
UPDATE `quest_template` SET `RewardXPDifficulty`=4, `RewardXPMultiplier`=1 WHERE `ID`=95603; -- quest: 2150 at level 38 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=38 WHERE `ID`=95603;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 38 FROM `quest_template` WHERE `ID`=95603;
UPDATE `quest_template` SET `RewardXPDifficulty`=4, `RewardXPMultiplier`=1 WHERE `ID`=95613; -- quest: 2150 at level 38 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=38 WHERE `ID`=95613;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 38 FROM `quest_template` WHERE `ID`=95613;
UPDATE `quest_template` SET `RewardXPDifficulty`=4, `RewardXPMultiplier`=1 WHERE `ID`=95615; -- quest: 2150 at level 38 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=38 WHERE `ID`=95615;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 38 FROM `quest_template` WHERE `ID`=95615;
UPDATE `quest_template` SET `RewardXPDifficulty`=3, `RewardXPMultiplier`=1 WHERE `ID`=95737; -- Seeking Caitlin: 1250 at level 31 (was 630 at level 31)
UPDATE `quest_template_classic_level` SET `QuestLevel`=31 WHERE `ID`=95737;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 31 FROM `quest_template` WHERE `ID`=95737;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=95748; -- The Gurubashi Legends of Old: 3150 at level 40 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=40 WHERE `ID`=95748;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 40 FROM `quest_template` WHERE `ID`=95748;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=95770; -- Locating the Lost City: 3150 at level 40 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=40 WHERE `ID`=95770;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 40 FROM `quest_template` WHERE `ID`=95770;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=95773; -- Enemies in the Water: 2850 at level 37 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=37 WHERE `ID`=95773;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 37 FROM `quest_template` WHERE `ID`=95773;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=95786; -- quest: 3150 at level 40 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=40 WHERE `ID`=95786;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 40 FROM `quest_template` WHERE `ID`=95786;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=95792; -- quest: 3450 at level 42 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=42 WHERE `ID`=95792;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 42 FROM `quest_template` WHERE `ID`=95792;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=95805; -- Grace of An'she and Mu'sha: 355 at level 4 (was 180 at level 4)
UPDATE `quest_template_classic_level` SET `QuestLevel`=4 WHERE `ID`=95805;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 4 FROM `quest_template` WHERE `ID`=95805;
UPDATE `quest_template` SET `RewardXPDifficulty`=3, `RewardXPMultiplier`=1 WHERE `ID`=95810; -- Lost Relic Carry: 1250 at level 31 (was 1229 at level 31)
UPDATE `quest_template_classic_level` SET `QuestLevel`=31 WHERE `ID`=95810;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 31 FROM `quest_template` WHERE `ID`=95810;
UPDATE `quest_template` SET `RewardXPDifficulty`=6, `RewardXPMultiplier`=1 WHERE `ID`=95882; -- quest: 4450 at level 43 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=43 WHERE `ID`=95882;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 43 FROM `quest_template` WHERE `ID`=95882;
UPDATE `quest_template` SET `RewardXPDifficulty`=6, `RewardXPMultiplier`=1 WHERE `ID`=95912; -- quest: 4300 at level 42 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=42 WHERE `ID`=95912;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 42 FROM `quest_template` WHERE `ID`=95912;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=95913; -- quest: 2850 at level 38 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=38 WHERE `ID`=95913;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 38 FROM `quest_template` WHERE `ID`=95913;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=95934; -- quest: 3450 at level 42 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=42 WHERE `ID`=95934;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 42 FROM `quest_template` WHERE `ID`=95934;
UPDATE `quest_template` SET `RewardXPDifficulty`=4, `RewardXPMultiplier`=1 WHERE `ID`=95943; -- quest: 2150 at level 38 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=38 WHERE `ID`=95943;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 38 FROM `quest_template` WHERE `ID`=95943;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=95951; -- quest: 2850 at level 38 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=38 WHERE `ID`=95951;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 38 FROM `quest_template` WHERE `ID`=95951;
UPDATE `quest_template` SET `RewardXPDifficulty`=6, `RewardXPMultiplier`=1 WHERE `ID`=95991; -- quest: 4300 at level 42 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=42 WHERE `ID`=95991;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 42 FROM `quest_template` WHERE `ID`=95991;
UPDATE `quest_template` SET `RewardXPDifficulty`=4, `RewardXPMultiplier`=1 WHERE `ID`=96026; -- quest: 2450 at level 41 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=41 WHERE `ID`=96026;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 41 FROM `quest_template` WHERE `ID`=96026;
UPDATE `quest_template` SET `RewardXPDifficulty`=3, `RewardXPMultiplier`=1 WHERE `ID`=96031; -- Camping 101: Leatherworking: 270 at level 6 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=6 WHERE `ID`=96031;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 6 FROM `quest_template` WHERE `ID`=96031;
UPDATE `quest_template` SET `RewardXPDifficulty`=3, `RewardXPMultiplier`=1 WHERE `ID`=96044; -- Camping 101: Blacksmithing: 270 at level 6 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=6 WHERE `ID`=96044;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 6 FROM `quest_template` WHERE `ID`=96044;
UPDATE `quest_template` SET `RewardXPDifficulty`=3, `RewardXPMultiplier`=1 WHERE `ID`=96045; -- Camping 101: Alchemy: 270 at level 6 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=6 WHERE `ID`=96045;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 6 FROM `quest_template` WHERE `ID`=96045;
UPDATE `quest_template` SET `RewardXPDifficulty`=3, `RewardXPMultiplier`=1 WHERE `ID`=96046; -- Camping 101: Mining: 270 at level 6 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=6 WHERE `ID`=96046;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 6 FROM `quest_template` WHERE `ID`=96046;
UPDATE `quest_template` SET `RewardXPDifficulty`=3, `RewardXPMultiplier`=1 WHERE `ID`=96047; -- Camping 101: First Aid: 270 at level 6 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=6 WHERE `ID`=96047;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 6 FROM `quest_template` WHERE `ID`=96047;
UPDATE `quest_template` SET `RewardXPDifficulty`=3, `RewardXPMultiplier`=1 WHERE `ID`=96050; -- Camping 101: Fishing: 270 at level 6 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=6 WHERE `ID`=96050;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 6 FROM `quest_template` WHERE `ID`=96050;
UPDATE `quest_template` SET `RewardXPDifficulty`=3, `RewardXPMultiplier`=1 WHERE `ID`=96055; -- Camping 101: Herbalism: 270 at level 6 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=6 WHERE `ID`=96055;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 6 FROM `quest_template` WHERE `ID`=96055;
UPDATE `quest_template` SET `RewardXPDifficulty`=3, `RewardXPMultiplier`=1 WHERE `ID`=96056; -- Camping 101: Skinning: 270 at level 6 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=6 WHERE `ID`=96056;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 6 FROM `quest_template` WHERE `ID`=96056;
UPDATE `quest_template` SET `RewardXPDifficulty`=3, `RewardXPMultiplier`=1 WHERE `ID`=96058; -- Camping 101: Engineering: 270 at level 6 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=6 WHERE `ID`=96058;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 6 FROM `quest_template` WHERE `ID`=96058;
UPDATE `quest_template` SET `RewardXPDifficulty`=3, `RewardXPMultiplier`=1 WHERE `ID`=96059; -- Camping 101: Enchanting: 270 at level 6 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=6 WHERE `ID`=96059;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 6 FROM `quest_template` WHERE `ID`=96059;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=96063; -- quest: 2850 at level 38 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=38 WHERE `ID`=96063;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 38 FROM `quest_template` WHERE `ID`=96063;
UPDATE `quest_template` SET `RewardXPDifficulty`=4, `RewardXPMultiplier`=1 WHERE `ID`=96067; -- quest: 2350 at level 40 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=40 WHERE `ID`=96067;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 40 FROM `quest_template` WHERE `ID`=96067;
UPDATE `quest_template` SET `RewardXPDifficulty`=4, `RewardXPMultiplier`=1 WHERE `ID`=96078; -- quest: 2350 at level 40 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=40 WHERE `ID`=96078;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 40 FROM `quest_template` WHERE `ID`=96078;
UPDATE `quest_template` SET `RewardXPDifficulty`=3, `RewardXPMultiplier`=1 WHERE `ID`=96081; -- quest: 1550 at level 40 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=40 WHERE `ID`=96081;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 40 FROM `quest_template` WHERE `ID`=96081;
UPDATE `quest_template` SET `RewardXPDifficulty`=2, `RewardXPMultiplier`=1 WHERE `ID`=96103; -- quest: 710 at level 38 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=38 WHERE `ID`=96103;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 38 FROM `quest_template` WHERE `ID`=96103;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=96104; -- quest: 3000 at level 39 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=39 WHERE `ID`=96104;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 39 FROM `quest_template` WHERE `ID`=96104;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=96107; -- quest: 3150 at level 40 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=40 WHERE `ID`=96107;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 40 FROM `quest_template` WHERE `ID`=96107;
UPDATE `quest_template` SET `RewardXPDifficulty`=4, `RewardXPMultiplier`=1 WHERE `ID`=96108; -- quest: 2350 at level 40 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=40 WHERE `ID`=96108;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 40 FROM `quest_template` WHERE `ID`=96108;
UPDATE `quest_template` SET `RewardXPDifficulty`=4, `RewardXPMultiplier`=1 WHERE `ID`=96130; -- Chakuyak: 335 at level 5 (was 350 at level 8)
UPDATE `quest_template_classic_level` SET `QuestLevel`=5 WHERE `ID`=96130;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 5 FROM `quest_template` WHERE `ID`=96130;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=96137; -- Ira's Dagger: 2000 at level 25 (was 1950 at level 24)
UPDATE `quest_template_classic_level` SET `QuestLevel`=25 WHERE `ID`=96137;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 25 FROM `quest_template` WHERE `ID`=96137;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=96138; -- Merrick's Bow: 2450 at level 30 (was 2300 at level 28)
UPDATE `quest_template_classic_level` SET `QuestLevel`=30 WHERE `ID`=96138;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 30 FROM `quest_template` WHERE `ID`=96138;
UPDATE `quest_template` SET `RewardXPDifficulty`=6, `RewardXPMultiplier`=1 WHERE `ID`=96222; -- Khan Hratha: 4300 at level 42 (was 3450 at level 42)
UPDATE `quest_template_classic_level` SET `QuestLevel`=42 WHERE `ID`=96222;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 42 FROM `quest_template` WHERE `ID`=96222;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=96241; -- Talkin' bout Toxins, Mon: 2350 at level 29 (was 2450 at level 30)
UPDATE `quest_template_classic_level` SET `QuestLevel`=29 WHERE `ID`=96241;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 29 FROM `quest_template` WHERE `ID`=96241;
UPDATE `quest_template` SET `RewardXPDifficulty`=3, `RewardXPMultiplier`=1 WHERE `ID`=96243; -- Call of Earth: 180 at level 4 (was 420 at level 10)
UPDATE `quest_template_classic_level` SET `QuestLevel`=4 WHERE `ID`=96243;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 4 FROM `quest_template` WHERE `ID`=96243;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=96259; -- Defending the Dead: 2350 at level 29 (was 2450 at level 30)
UPDATE `quest_template_classic_level` SET `QuestLevel`=29 WHERE `ID`=96259;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 29 FROM `quest_template` WHERE `ID`=96259;
UPDATE `quest_template` SET `RewardXPDifficulty`=4, `RewardXPMultiplier`=1 WHERE `ID`=96260; -- Stronger than Steel: 1750 at level 29 (was 2450 at level 30)
UPDATE `quest_template_classic_level` SET `QuestLevel`=29 WHERE `ID`=96260;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 29 FROM `quest_template` WHERE `ID`=96260;
UPDATE `quest_template` SET `RewardXPDifficulty`=3, `RewardXPMultiplier`=1 WHERE `ID`=96629; -- Camping 101: Cooking: 270 at level 6 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=6 WHERE `ID`=96629;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 6 FROM `quest_template` WHERE `ID`=96629;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=96781; -- What's the Big Idea?: 2800 at level 36 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=36 WHERE `ID`=96781;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 36 FROM `quest_template` WHERE `ID`=96781;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=96783; -- A Way In: 2800 at level 36 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=36 WHERE `ID`=96783;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 36 FROM `quest_template` WHERE `ID`=96783;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=96821; -- Legging It: 540 at level 6 (was 405 at level 6)
UPDATE `quest_template_classic_level` SET `QuestLevel`=6 WHERE `ID`=96821;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 6 FROM `quest_template` WHERE `ID`=96821;
UPDATE `quest_template` SET `RewardXPDifficulty`=4, `RewardXPMultiplier`=1 WHERE `ID`=96822; -- For Honor: 475 at level 7 (was 405 at level 6)
UPDATE `quest_template_classic_level` SET `QuestLevel`=7 WHERE `ID`=96822;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 7 FROM `quest_template` WHERE `ID`=96822;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=96825; -- This Fruit Could Bite Back: 540 at level 6 (was 405 at level 6)
UPDATE `quest_template_classic_level` SET `QuestLevel`=6 WHERE `ID`=96825;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 6 FROM `quest_template` WHERE `ID`=96825;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=96874; -- This Is Spinal Axe: 840 at level 10 (was 630 at level 10)
UPDATE `quest_template_classic_level` SET `QuestLevel`=10 WHERE `ID`=96874;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 10 FROM `quest_template` WHERE `ID`=96874;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=96875; -- Beasts of Thunder Ridge: 840 at level 10 (was 630 at level 10)
UPDATE `quest_template_classic_level` SET `QuestLevel`=10 WHERE `ID`=96875;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 10 FROM `quest_template` WHERE `ID`=96875;
UPDATE `quest_template` SET `RewardXPDifficulty`=6, `RewardXPMultiplier`=1 WHERE `ID`=96876; -- Ukor's Lost Pack: 880 at level 8 (was 530 at level 8)
UPDATE `quest_template_classic_level` SET `QuestLevel`=8 WHERE `ID`=96876;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 8 FROM `quest_template` WHERE `ID`=96876;
UPDATE `quest_template` SET `RewardXPDifficulty`=6, `RewardXPMultiplier`=1 WHERE `ID`=96877; -- Halikor's Hoof: 1150 at level 12 (was 680 at level 12)
UPDATE `quest_template_classic_level` SET `QuestLevel`=12 WHERE `ID`=96877;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 12 FROM `quest_template` WHERE `ID`=96877;
UPDATE `quest_template` SET `RewardXPDifficulty`=1, `RewardXPMultiplier`=1 WHERE `ID`=96896; -- A Righteous Cause: 90 at level 12 (was 90 at level 13)
UPDATE `quest_template_classic_level` SET `QuestLevel`=12 WHERE `ID`=96896;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 12 FROM `quest_template` WHERE `ID`=96896;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=97047; -- The Cook in the Cage: 2850 at level 38 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=38 WHERE `ID`=97047;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 38 FROM `quest_template` WHERE `ID`=97047;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=97048; -- Returning to Port: 2850 at level 38 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=38 WHERE `ID`=97048;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 38 FROM `quest_template` WHERE `ID`=97048;
UPDATE `quest_template` SET `RewardXPDifficulty`=2, `RewardXPMultiplier`=1 WHERE `ID`=97222; -- Gatehouse Goods: 270 at level 15 (was 800 at level 15)
UPDATE `quest_template_classic_level` SET `QuestLevel`=15 WHERE `ID`=97222;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 15 FROM `quest_template` WHERE `ID`=97222;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=97223; -- Bloodtalon Matriarch: 780 at level 9 (was 700 at level 8)
UPDATE `quest_template_classic_level` SET `QuestLevel`=9 WHERE `ID`=97223;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 9 FROM `quest_template` WHERE `ID`=97223;
UPDATE `quest_template` SET `RewardXPDifficulty`=3, `RewardXPMultiplier`=1 WHERE `ID`=97237; -- Shelf Picked: 680 at level 18 (was 1000 at level 18)
UPDATE `quest_template_classic_level` SET `QuestLevel`=18 WHERE `ID`=97237;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 18 FROM `quest_template` WHERE `ID`=97237;
UPDATE `quest_template` SET `RewardXPDifficulty`=1, `RewardXPMultiplier`=1 WHERE `ID`=97242; -- Yelmak's Medley: 105 at level 15 (was 800 at level 15)
UPDATE `quest_template_classic_level` SET `QuestLevel`=15 WHERE `ID`=97242;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 15 FROM `quest_template` WHERE `ID`=97242;
UPDATE `quest_template` SET `RewardXPDifficulty`=1, `RewardXPMultiplier`=1 WHERE `ID`=97246; -- Meal Appeal: 105 at level 15 (was 800 at level 15)
UPDATE `quest_template_classic_level` SET `QuestLevel`=15 WHERE `ID`=97246;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 15 FROM `quest_template` WHERE `ID`=97246;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=97253; -- Parts and Pieces: 1050 at level 15 (was 980 at level 14)
UPDATE `quest_template_classic_level` SET `QuestLevel`=15 WHERE `ID`=97253;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 15 FROM `quest_template` WHERE `ID`=97253;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=97282; -- Stormy Potential: 840 at level 10 (was 880 at level 11)
UPDATE `quest_template_classic_level` SET `QuestLevel`=10 WHERE `ID`=97282;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 10 FROM `quest_template` WHERE `ID`=97282;
UPDATE `quest_template` SET `RewardXPDifficulty`=3, `RewardXPMultiplier`=1 WHERE `ID`=97326; -- Rocks to Rests: 540 at level 15 (was 800 at level 15)
UPDATE `quest_template_classic_level` SET `QuestLevel`=15 WHERE `ID`=97326;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 15 FROM `quest_template` WHERE `ID`=97326;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=97328; -- Anything of Value: 2750 at level 35 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=35 WHERE `ID`=97328;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 35 FROM `quest_template` WHERE `ID`=97328;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=97329; -- Where's the Key?: 2750 at level 35 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=35 WHERE `ID`=97329;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 35 FROM `quest_template` WHERE `ID`=97329;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=97331; -- Zimmix's Great Escape: 2750 at level 35 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=35 WHERE `ID`=97331;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 35 FROM `quest_template` WHERE `ID`=97331;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=97332; -- quest: 3300 at level 41 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=41 WHERE `ID`=97332;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 41 FROM `quest_template` WHERE `ID`=97332;
UPDATE `quest_template` SET `RewardXPDifficulty`=6, `RewardXPMultiplier`=1 WHERE `ID`=97386; -- quest: 4650 at level 44 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=44 WHERE `ID`=97386;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 44 FROM `quest_template` WHERE `ID`=97386;
UPDATE `quest_template` SET `RewardXPDifficulty`=3, `RewardXPMultiplier`=1 WHERE `ID`=97453; -- quest: 1450 at level 38 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=38 WHERE `ID`=97453;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 38 FROM `quest_template` WHERE `ID`=97453;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=97884; -- quest: 3150 at level 40 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=40 WHERE `ID`=97884;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 40 FROM `quest_template` WHERE `ID`=97884;
UPDATE `quest_template` SET `RewardXPDifficulty`=3, `RewardXPMultiplier`=1 WHERE `ID`=97894; -- Business in Auberdine: 630 at level 17 (was 0 at level 17)
UPDATE `quest_template_classic_level` SET `QuestLevel`=17 WHERE `ID`=97894;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 17 FROM `quest_template` WHERE `ID`=97894;
UPDATE `quest_template` SET `RewardXPDifficulty`=3, `RewardXPMultiplier`=1 WHERE `ID`=97914; -- Expanding Horizons: 630 at level 17 (was 125 at level 17)
UPDATE `quest_template_classic_level` SET `QuestLevel`=17 WHERE `ID`=97914;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 17 FROM `quest_template` WHERE `ID`=97914;
UPDATE `quest_template` SET `RewardXPDifficulty`=3, `RewardXPMultiplier`=1 WHERE `ID`=97915; -- Camping 101: Alchemy: 270 at level 6 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=6 WHERE `ID`=97915;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 6 FROM `quest_template` WHERE `ID`=97915;
UPDATE `quest_template` SET `RewardXPDifficulty`=3, `RewardXPMultiplier`=1 WHERE `ID`=97916; -- Camping 101: Blacksmithing: 270 at level 6 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=6 WHERE `ID`=97916;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 6 FROM `quest_template` WHERE `ID`=97916;
UPDATE `quest_template` SET `RewardXPDifficulty`=3, `RewardXPMultiplier`=1 WHERE `ID`=97917; -- Camping 101: Enchanting: 270 at level 6 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=6 WHERE `ID`=97917;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 6 FROM `quest_template` WHERE `ID`=97917;
UPDATE `quest_template` SET `RewardXPDifficulty`=3, `RewardXPMultiplier`=1 WHERE `ID`=97918; -- Camping 101: Engineering: 270 at level 6 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=6 WHERE `ID`=97918;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 6 FROM `quest_template` WHERE `ID`=97918;
UPDATE `quest_template` SET `RewardXPDifficulty`=3, `RewardXPMultiplier`=1 WHERE `ID`=97919; -- Camping 101: First Aid: 270 at level 6 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=6 WHERE `ID`=97919;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 6 FROM `quest_template` WHERE `ID`=97919;
UPDATE `quest_template` SET `RewardXPDifficulty`=3, `RewardXPMultiplier`=1 WHERE `ID`=97920; -- Camping 101: Fishing: 270 at level 6 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=6 WHERE `ID`=97920;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 6 FROM `quest_template` WHERE `ID`=97920;
UPDATE `quest_template` SET `RewardXPDifficulty`=3, `RewardXPMultiplier`=1 WHERE `ID`=97921; -- Camping 101: Herbalism: 270 at level 6 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=6 WHERE `ID`=97921;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 6 FROM `quest_template` WHERE `ID`=97921;
UPDATE `quest_template` SET `RewardXPDifficulty`=3, `RewardXPMultiplier`=1 WHERE `ID`=97922; -- Camping 101: Leatherworking: 270 at level 6 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=6 WHERE `ID`=97922;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 6 FROM `quest_template` WHERE `ID`=97922;
UPDATE `quest_template` SET `RewardXPDifficulty`=3, `RewardXPMultiplier`=1 WHERE `ID`=97924; -- Camping 101: Skinning: 270 at level 6 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=6 WHERE `ID`=97924;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 6 FROM `quest_template` WHERE `ID`=97924;
UPDATE `quest_template` SET `RewardXPDifficulty`=3, `RewardXPMultiplier`=1 WHERE `ID`=97926; -- Making Do: 630 at level 17 (was 125 at level 17)
UPDATE `quest_template_classic_level` SET `QuestLevel`=17 WHERE `ID`=97926;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 17 FROM `quest_template` WHERE `ID`=97926;
UPDATE `quest_template` SET `RewardXPDifficulty`=3, `RewardXPMultiplier`=1 WHERE `ID`=97930; -- Camping 101: Engineering: 270 at level 6 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=6 WHERE `ID`=97930;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 6 FROM `quest_template` WHERE `ID`=97930;
UPDATE `quest_template` SET `RewardXPDifficulty`=3, `RewardXPMultiplier`=1 WHERE `ID`=97951; -- Camping 101: Alchemy: 270 at level 6 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=6 WHERE `ID`=97951;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 6 FROM `quest_template` WHERE `ID`=97951;
UPDATE `quest_template` SET `RewardXPDifficulty`=3, `RewardXPMultiplier`=1 WHERE `ID`=97952; -- Camping 101: Blacksmithing: 270 at level 6 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=6 WHERE `ID`=97952;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 6 FROM `quest_template` WHERE `ID`=97952;
UPDATE `quest_template` SET `RewardXPDifficulty`=3, `RewardXPMultiplier`=1 WHERE `ID`=97954; -- Camping 101: Engineering: 270 at level 6 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=6 WHERE `ID`=97954;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 6 FROM `quest_template` WHERE `ID`=97954;
UPDATE `quest_template` SET `RewardXPDifficulty`=3, `RewardXPMultiplier`=1 WHERE `ID`=97958; -- Camping 101: Leatherworking: 270 at level 6 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=6 WHERE `ID`=97958;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 6 FROM `quest_template` WHERE `ID`=97958;
UPDATE `quest_template` SET `RewardXPDifficulty`=3, `RewardXPMultiplier`=1 WHERE `ID`=97959; -- Camping 101: Mining: 270 at level 6 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=6 WHERE `ID`=97959;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 6 FROM `quest_template` WHERE `ID`=97959;
UPDATE `quest_template` SET `RewardXPDifficulty`=4, `RewardXPMultiplier`=1 WHERE `ID`=98027; -- Gleaming Crescent Necklace: 1600 at level 26 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=26 WHERE `ID`=98027;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 26 FROM `quest_template` WHERE `ID`=98027;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=98059; -- Hillsbrad's Hoard: 2200 at level 27 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=27 WHERE `ID`=98059;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 27 FROM `quest_template` WHERE `ID`=98059;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=98069; -- Stolen Freewind Supplies: 2450 at level 30 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=30 WHERE `ID`=98069;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 30 FROM `quest_template` WHERE `ID`=98069;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=98070; -- Stop the Screeching: 2450 at level 30 (was 2500 at level 31)
UPDATE `quest_template_classic_level` SET `QuestLevel`=30 WHERE `ID`=98070;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 30 FROM `quest_template` WHERE `ID`=98070;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=98071; -- Brewer's Trade: 2200 at level 27 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=27 WHERE `ID`=98071;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 27 FROM `quest_template` WHERE `ID`=98071;
UPDATE `quest_template` SET `RewardXPDifficulty`=3, `RewardXPMultiplier`=1 WHERE `ID`=98094; -- Scout Support: 1000 at level 25 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=25 WHERE `ID`=98094;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 25 FROM `quest_template` WHERE `ID`=98094;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=98095; -- Valuable Vantages: 2000 at level 25 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=25 WHERE `ID`=98095;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 25 FROM `quest_template` WHERE `ID`=98095;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=98155; -- Pristine Pesterhide Pelts: 2350 at level 29 (was 2500 at level 31)
UPDATE `quest_template_classic_level` SET `QuestLevel`=29 WHERE `ID`=98155;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 29 FROM `quest_template` WHERE `ID`=98155;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=98191; -- A Friend of the Family: 2000 at level 25 (was 200 at level 25)
UPDATE `quest_template_classic_level` SET `QuestLevel`=25 WHERE `ID`=98191;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 25 FROM `quest_template` WHERE `ID`=98191;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=98192; -- quest: 3600 at level 43 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=43 WHERE `ID`=98192;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 43 FROM `quest_template` WHERE `ID`=98192;
UPDATE `quest_template` SET `RewardXPDifficulty`=4, `RewardXPMultiplier`=1 WHERE `ID`=98208; -- Bloom of the Heavens: 1750 at level 29 (was 2350 at level 29)
UPDATE `quest_template_classic_level` SET `QuestLevel`=29 WHERE `ID`=98208;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 29 FROM `quest_template` WHERE `ID`=98208;
UPDATE `quest_template` SET `RewardXPDifficulty`=3, `RewardXPMultiplier`=1 WHERE `ID`=98223; -- Old Habits: 1250 at level 31 (was 250 at level 31)
UPDATE `quest_template_classic_level` SET `QuestLevel`=31 WHERE `ID`=98223;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 31 FROM `quest_template` WHERE `ID`=98223;
UPDATE `quest_template` SET `RewardXPDifficulty`=1, `RewardXPMultiplier`=1 WHERE `ID`=98230; -- This Land Was Their Land: 235 at level 29 (was 1200 at level 29)
UPDATE `quest_template_classic_level` SET `QuestLevel`=29 WHERE `ID`=98230;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 29 FROM `quest_template` WHERE `ID`=98230;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=98246; -- Trying Times: 2350 at level 29 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=29 WHERE `ID`=98246;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 29 FROM `quest_template` WHERE `ID`=98246;
UPDATE `quest_template` SET `RewardXPDifficulty`=3, `RewardXPMultiplier`=1 WHERE `ID`=98252; -- A Void Path: 920 at level 23 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=23 WHERE `ID`=98252;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 23 FROM `quest_template` WHERE `ID`=98252;
UPDATE `quest_template` SET `RewardXPDifficulty`=3, `RewardXPMultiplier`=1 WHERE `ID`=98319; -- Secure the Mountain: 350 at level 8 (was 175 at level 8)
UPDATE `quest_template_classic_level` SET `QuestLevel`=8 WHERE `ID`=98319;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 8 FROM `quest_template` WHERE `ID`=98319;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=98403; -- Twisted Hatred: 910 at level 12 (was 1150 at level 12)
UPDATE `quest_template_classic_level` SET `QuestLevel`=12 WHERE `ID`=98403;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 12 FROM `quest_template` WHERE `ID`=98403;
UPDATE `quest_template` SET `RewardXPDifficulty`=4, `RewardXPMultiplier`=1 WHERE `ID`=98417; -- Vengeful Trail: 1700 at level 28 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=28 WHERE `ID`=98417;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 28 FROM `quest_template` WHERE `ID`=98417;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=98418; -- Shared Fury: 2450 at level 30 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=30 WHERE `ID`=98418;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 30 FROM `quest_template` WHERE `ID`=98418;
UPDATE `quest_template` SET `RewardXPDifficulty`=3, `RewardXPMultiplier`=1 WHERE `ID`=98517; -- Call of Fire: 420 at level 10 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=10 WHERE `ID`=98517;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 10 FROM `quest_template` WHERE `ID`=98517;
UPDATE `quest_template` SET `RewardXPDifficulty`=1, `RewardXPMultiplier`=1 WHERE `ID`=98545; -- Leonid's Letter: 90 at level 12 (was 90 at level 13)
UPDATE `quest_template_classic_level` SET `QuestLevel`=12 WHERE `ID`=98545;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 12 FROM `quest_template` WHERE `ID`=98545;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=98903; -- The Battle for Darkspear Islands!: 3000 at level 39 (was not known here)
UPDATE `quest_template_classic_level` SET `QuestLevel`=39 WHERE `ID`=98903;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 39 FROM `quest_template` WHERE `ID`=98903;
UPDATE `quest_template` SET `RewardXPDifficulty`=1, `RewardXPMultiplier`=1 WHERE `ID`=99048; -- A Missing Hand: 80 at level 9 (was 90 at level 11)
UPDATE `quest_template_classic_level` SET `QuestLevel`=9 WHERE `ID`=99048;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 9 FROM `quest_template` WHERE `ID`=99048;
UPDATE `quest_template` SET `RewardXPDifficulty`=3, `RewardXPMultiplier`=1 WHERE `ID`=99049; -- Threat from Below: 390 at level 9 (was 880 at level 11)
UPDATE `quest_template_classic_level` SET `QuestLevel`=9 WHERE `ID`=99049;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 9 FROM `quest_template` WHERE `ID`=99049;
UPDATE `quest_template` SET `RewardXPDifficulty`=6, `RewardXPMultiplier`=1 WHERE `ID`=99052; -- Threat from Below: 1100 at level 11 (was 910 at level 12)
UPDATE `quest_template_classic_level` SET `QuestLevel`=11 WHERE `ID`=99052;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 11 FROM `quest_template` WHERE `ID`=99052;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=99053; -- Escaping Ban'ethil: 840 at level 10 (was 0 at level 9)
UPDATE `quest_template_classic_level` SET `QuestLevel`=10 WHERE `ID`=99053;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 10 FROM `quest_template` WHERE `ID`=99053;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=99080; -- Drive Them Out: 880 at level 11 (was 780 at level 9)
UPDATE `quest_template_classic_level` SET `QuestLevel`=11 WHERE `ID`=99080;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 11 FROM `quest_template` WHERE `ID`=99080;
UPDATE `quest_template` SET `RewardXPDifficulty`=1, `RewardXPMultiplier`=1 WHERE `ID`=99082; -- The High Chieftain: 90 at level 11 (was 80 at level 9)
UPDATE `quest_template_classic_level` SET `QuestLevel`=11 WHERE `ID`=99082;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 11 FROM `quest_template` WHERE `ID`=99082;
UPDATE `quest_template` SET `RewardXPDifficulty`=3, `RewardXPMultiplier`=1 WHERE `ID`=99108; -- Sparring Match: 270 at level 6 (was 540 at level 6)
UPDATE `quest_template_classic_level` SET `QuestLevel`=6 WHERE `ID`=99108;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 6 FROM `quest_template` WHERE `ID`=99108;
UPDATE `quest_template` SET `RewardXPDifficulty`=4, `RewardXPMultiplier`=1 WHERE `ID`=99127; -- A Net Disaster: 475 at level 7 (was 630 at level 7)
UPDATE `quest_template_classic_level` SET `QuestLevel`=7 WHERE `ID`=99127;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 7 FROM `quest_template` WHERE `ID`=99127;
UPDATE `quest_template` SET `RewardXPDifficulty`=4, `RewardXPMultiplier`=1 WHERE `ID`=99143; -- Bottles and Baubles: 475 at level 7 (was 630 at level 7)
UPDATE `quest_template_classic_level` SET `QuestLevel`=7 WHERE `ID`=99143;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 7 FROM `quest_template` WHERE `ID`=99143;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=99152; -- As Above, So Below: 910 at level 12 (was 840 at level 10)
UPDATE `quest_template_classic_level` SET `QuestLevel`=12 WHERE `ID`=99152;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 12 FROM `quest_template` WHERE `ID`=99152;
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=99153; -- The One That Got Away: 910 at level 12 (was 840 at level 10)
UPDATE `quest_template_classic_level` SET `QuestLevel`=12 WHERE `ID`=99153;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 12 FROM `quest_template` WHERE `ID`=99153;
UPDATE `quest_template` SET `RewardXPDifficulty`=3, `RewardXPMultiplier`=1 WHERE `ID`=99159; -- Finding Warmth: 350 at level 8 (was 700 at level 8)
UPDATE `quest_template_classic_level` SET `QuestLevel`=8 WHERE `ID`=99159;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 8 FROM `quest_template` WHERE `ID`=99159;
UPDATE `quest_template` SET `RewardXPDifficulty`=4, `RewardXPMultiplier`=1 WHERE `ID`=99160; -- Rime's Wrath: 530 at level 8 (was 700 at level 8)
UPDATE `quest_template_classic_level` SET `QuestLevel`=8 WHERE `ID`=99160;
INSERT IGNORE INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) SELECT `ID`, 2, 8 FROM `quest_template` WHERE `ID`=99160;
