-- WoW Forever live quest data from EG Link (Efficient Games), JIVESCORP/eg-link-output-wowf commit 3067c69, folders 1.60.1.70862
-- and 1.60.1.70888 (70888 wins where both have the quest). Data from Efficient Games (https://efficient.games), licensed under
-- CC BY 4.0 (https://creativecommons.org/licenses/by/4.0/). Changed: compared with our world data and written as TrinityCore rows.
-- The game's quest records as players' clients received them from the live realm. VerifiedBuild = the EG folder's build.
-- Aeternum (Baileys88it/aeternum-core 7df55070) was only used as a pointer to what to look at; no row is taken from it.
-- Only quests we already have. Safe to run more than once.

-- 1. Objectives live has and we lack. 63 rows on 59 quests:
--    28 explore-an-area objectives (with their area trigger link; most of these quests had no trigger link at all, so they could
--    not be finished), 19 reputation requirements (Magram/Gelkis, care packages: hidden on live, storage -1), 14 items to bring,
--    2 criteria objectives replacing our guesses on Camping 101: Cooking 96655/96661 (ours named Tomas in Goldshire on the Horde ones).
--    Items are only added when our data has a drop, vendor or quest source for them.
--    Raene's Cleansing (1045): live asks for Ran Bloodtooth's Skull (item 5388) and 4 Bloodtooth Guards; ours asked to kill Ran Bloodtooth (3696) instead of the skull.
DELETE FROM `quest_objectives` WHERE `ID` IN (9665500,9666100,90010450);
DELETE FROM `quest_objectives` WHERE `ID` IN (385521,385473,385476,385475,385479,385480,385481,385482,480587,385484,381127,385491,385516,381144,381185,381562,381629,381762,381230,381571,381348,381398,381577,385492,385504,385494,385495,385513,385514,385519,385518,385517,385496,385520,385522,480569,384263,383722,384186,385506,385510,385508,385509,385507,477032,477093,478265,465591,466025,466189,472288,472291,472289,473835,473847,477083,477300,477551,477552,477553,477548,477549,477550);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES
(385521,25,10,1,1,2926,1,0,0,0,0,0,1,NULL,70888),
(385473,76,10,0,0,87,1,0,0,0,0,0,1,NULL,70862),
(385476,201,10,0,0,98,1,0,0,0,0,0,1,NULL,70888),
(385475,287,10,1,1,97,1,0,0,0,0,0,1,NULL,70862),
(385479,437,10,1,1,173,1,0,0,0,0,0,1,NULL,70862),
(385480,455,10,2,2,175,1,0,0,0,0,0,1,NULL,70862),
(385481,503,10,1,1,178,1,0,0,0,0,0,1,NULL,70888),
(385482,578,10,0,0,196,1,0,0,0,0,0,1,NULL,70862),
(480587,772,1,0,0,4823,1,0,0,0,0,0,1,NULL,70862),
(385484,870,10,0,0,216,1,0,0,0,0,0,1,NULL,70862),
(381127,1045,1,0,0,5388,1,0,0,0,0,0,1,NULL,70862),
(385491,1149,10,0,0,246,1,0,0,0,0,0,1,NULL,70862),
(385516,1265,10,0,0,1667,1,0,0,0,0,0,1,NULL,70862),
(381144,1367,6,0,0,93,3000,0,0,0,0,0,1,NULL,70888),
(381185,1368,6,0,0,92,3000,0,0,0,0,0,1,NULL,70888),
(381562,1369,6,1,1,93,3000,0,0,0,0,0,1,NULL,70888),
(381629,1370,6,1,1,92,3000,0,0,0,0,0,1,NULL,70862),
(381762,1371,6,0,-1,93,3000,0,0,0,0,0,1,NULL,70862),
(381230,1373,6,0,-1,92,3000,0,0,0,0,0,1,NULL,70888),
(381571,1382,6,0,0,92,3000,0,0,0,0,0,1,NULL,70862),
(381348,1384,6,1,1,92,3000,0,0,0,0,0,1,NULL,70862),
(381398,1385,6,0,0,93,3000,0,0,0,0,0,1,NULL,70862),
(381577,1386,6,3,3,93,3000,0,0,0,0,0,1,NULL,70862),
(385492,1448,10,0,0,362,1,0,0,0,0,0,1,NULL,70862),
(385504,1658,10,0,0,3991,1,0,0,0,0,0,1,NULL,70862),
(385494,1699,10,0,0,482,1,0,0,0,0,0,1,NULL,70862),
(385495,1719,10,1,1,522,1,0,0,0,0,0,1,NULL,70862),
(385513,2240,10,0,0,822,1,0,0,0,0,0,1,NULL,70862),
(385514,2989,10,0,0,1205,1,0,0,0,0,0,1,NULL,70888),
(385519,4811,10,0,0,2486,1,0,0,0,0,0,1,NULL,70862),
(385518,4842,10,0,0,2327,1,0,0,0,0,0,1,NULL,70862),
(385517,5156,10,2,2,2206,1,0,0,0,0,0,1,NULL,70888),
(385496,6025,10,0,0,3366,1,0,0,0,0,0,1,NULL,70888),
(385520,6185,10,3,3,2726,1,0,0,0,0,0,1,NULL,70888),
(385522,6421,10,1,1,2946,1,0,0,0,0,0,1,NULL,70862),
(480569,6481,1,1,1,16603,1,0,0,0,0,0,1,NULL,70862),
(384263,8260,6,0,0,509,3000,0,0,0,0,0,1,NULL,70862),
(383722,8263,6,0,0,510,3000,0,0,0,0,0,1,NULL,70862),
(384186,8470,6,1,1,576,0,0,0,0,0,0,1,NULL,70862),
(385506,9261,10,1,1,4098,1,0,0,0,0,0,1,NULL,70862),
(385510,9262,10,1,1,4104,1,0,0,0,0,0,1,NULL,70862),
(385508,9263,10,1,1,4101,1,0,0,0,0,0,1,NULL,70862),
(385509,9264,10,1,1,4103,1,0,0,0,0,0,1,NULL,70862),
(385507,9265,10,1,1,4100,1,0,0,0,0,0,1,NULL,70862),
(477032,92434,1,1,1,279469,1,0,0,0,0,0,1,NULL,70888),
(477093,92459,1,0,0,251961,1,0,0,0,0,0,1,NULL,70862),
(478265,92481,1,0,0,282416,1,0,0,0,0,0,1,NULL,70862),
(465591,92818,6,0,-1,93,3000,0,0,0,0,0,1,NULL,70862),
(466025,93062,6,0,-1,92,9000,0,0,0,0,0,1,NULL,70862),
(466189,93128,6,0,-1,92,9000,0,0,0,0,0,1,NULL,70888),
(472288,93227,6,0,-1,92,0,0,0,0,0,0,1,NULL,70862),
(472291,94872,6,0,-1,93,0,0,0,0,0,0,1,NULL,70888),
(472289,94873,6,0,-1,92,0,0,0,0,0,0,1,NULL,70888),
(473835,96655,14,0,0,227879,1,0,32,0,0,0,1,'Learn cooking from Cook Torka in Razor Hill',70862),
(473847,96661,14,0,0,227886,1,0,32,0,0,0,1,'Learn cooking from Pyall Silentstride in Bloodhoof Village',70862),
(477083,97287,1,0,0,251961,1,0,0,0,0,0,1,NULL,70888),
(477300,98293,1,0,0,280369,30,0,0,0,0,0,1,NULL,70862),
(477551,98392,1,0,0,280831,1,0,0,0,0,0,1,NULL,70862),
(477552,98392,1,1,1,280832,1,0,0,0,0,0,1,NULL,70862),
(477553,98392,1,2,2,280834,1,0,0,0,0,0,1,NULL,70862),
(477548,98396,1,0,0,280502,1,0,0,0,0,0,1,NULL,70862),
(477549,98396,1,1,1,280679,1,0,0,0,0,0,1,NULL,70862),
(477550,98396,1,2,2,280680,1,0,0,0,0,0,1,NULL,70862);
DELETE FROM `areatrigger_involvedrelation` WHERE (`id`,`quest`) IN ((2926,25),(87,76),(98,201),(173,437),(175,455),(178,503),(196,578),(216,870),(246,1149),(1667,1265),(3991,1658),(482,1699),(522,1719),(822,2240),(1205,2989),(2486,4811),(2327,4842),(2946,6421));
INSERT INTO `areatrigger_involvedrelation` (`id`,`quest`) VALUES (2926,25),(87,76),(98,201),(173,437),(175,455),(178,503),(196,578),(216,870),(246,1149),(1667,1265),(3991,1658),(482,1699),(522,1719),(822,2240),(1205,2989),(2486,4811),(2327,4842),(2946,6421);

-- 1b. Live texts for 97287, 98392, 98396, 1045: 2026_11_11_15 kept our old text because ours lacked these objectives; now that they are in, the live
--     text matches again.
UPDATE `quest_template` SET `LogDescription` = 'Speak with the Image of Archmage Modera near Dalaran.', `QuestDescription` = 'You have done well to infiltrate the city of mages, but I believe it is time for us to be more... negotiatory.\n\nAs much as it pains me to admit, there is no other place on Azeroth with such a vast knowledge of the arcane. My brilliant work still pales in comparison to the ancient wisdom the Kirin Tor have amassed over the years. I think it''s high time we are granted access to this exclusive order, don''t you think?\n\nTake the mote you acquired. They will surely give anything to get it back. Anything.' WHERE `ID` = 97287;
UPDATE `quest_template` SET `LogDescription` = 'Collect Hatescreech''s Amulet, Windmistress Gaedress'' Amulet, and Witchmother Arysa''s Amulet for Sentinel Arynia Cloudsbreak in the Oracle Glade.', `QuestDescription` = 'These belts... look closely at the leather, $n. Do you see the taint running through the fibers?\n\nI have a suspicion as to its origin, but we will need further proof to be sure.\n\nVenture back out into the harpies'' nests to the west and take out their leaders. If their necklaces carry the same darkness as these belts, we''ll have our answer.' WHERE `ID` = 98392;
UPDATE `quest_template` SET `LogDescription` = 'Recover the Relic of the Fang, Relic of the Claw, and Relic of the Silent Shadow from the Stormrage Barrow Den, and return them to the Great Cat Spirit.', `QuestDescription` = 'To draw upon my spirit, you will need more than a few words. To understand and respect my form you must prove that you can embody cunning, stealth, and if you should be cornered, ferocity.\n\nIn the Barrow Dens just east of here, are three relics of my favor you must acquire. They are guarded by powerful enraged spirits that will attack you on sight. A statue at the entrance will bless you with temporary stealth to get past them.\n\nReturn these relics to me. We will see if you are ready to take my shape.' WHERE `ID` = 98396;
UPDATE `quest_template` SET `LogDescription` = 'Kill 4 Bloodtooth Guards and retrieve Ran Bloodtooth''s Skull for Krolg near Lake Mystral.', `QuestDescription` = 'The night elves are as old as time, but their arrogance destroys their very world. And now it destroys our lives too. But that can no longer be controlled...$B$BI blame the elves for much, but we are still responsible for our actions. Even beyond the corruption, we are capable of great evil.$B$BRan Bloodtooth is one such evil. Power hungry, lusting... the corruption has only made him stronger. My hatred for the elves burns, but I do not wish ill upon their children, so we must police our own.' WHERE `ID` = 1045;

-- 2. Follow-up quest offered at turn-in (RewardNextQuest), 67 quests. 1 of them had another follow-up in ours, the rest none.
UPDATE `quest_template` SET `RewardNextQuest` = 98298 WHERE `ID` = 99; -- Arugal's Folly
UPDATE `quest_template` SET `RewardNextQuest` = 98283 WHERE `ID` = 275; -- Blisters on The Land
UPDATE `quest_template` SET `RewardNextQuest` = 98189 WHERE `ID` = 286; -- Return the Statuette
UPDATE `quest_template` SET `RewardNextQuest` = 98216 WHERE `ID` = 299; -- Uncovering the Past
UPDATE `quest_template` SET `RewardNextQuest` = 99156 WHERE `ID` = 356; -- Rear Guard Patrol
UPDATE `quest_template` SET `RewardNextQuest` = 91920 WHERE `ID` = 430; -- Return to Quinn
UPDATE `quest_template` SET `RewardNextQuest` = 484 WHERE `ID` = 469; -- Daily Delivery
UPDATE `quest_template` SET `RewardNextQuest` = 98435 WHERE `ID` = 760; -- Wildmane Cleansing
UPDATE `quest_template` SET `RewardNextQuest` = 98403 WHERE `ID` = 932; -- Twisted Hatred
UPDATE `quest_template` SET `RewardNextQuest` = 935 WHERE `ID` = 934; -- Crown of the Earth
UPDATE `quest_template` SET `RewardNextQuest` = 98392 WHERE `ID` = 937; -- The Enchanted Glade
UPDATE `quest_template` SET `RewardNextQuest` = 93034 WHERE `ID` = 1373; -- Ongeku (ours 1374)
UPDATE `quest_template` SET `RewardNextQuest` = 1473 WHERE `ID` = 1478; -- Halgar's Summons
UPDATE `quest_template` SET `RewardNextQuest` = 1498 WHERE `ID` = 1505; -- Veteran Uzzek
UPDATE `quest_template` SET `RewardNextQuest` = 1501 WHERE `ID` = 1506; -- Gan'rul's Summons
UPDATE `quest_template` SET `RewardNextQuest` = 1639 WHERE `ID` = 1638; -- A Warrior's Training
UPDATE `quest_template` SET `RewardNextQuest` = 1678 WHERE `ID` = 1679; -- Muren Stormpike
UPDATE `quest_template` SET `RewardNextQuest` = 1683 WHERE `ID` = 1684; -- Elanaria
UPDATE `quest_template` SET `RewardNextQuest` = 1819 WHERE `ID` = 1818; -- Speak with Dillinger
UPDATE `quest_template` SET `RewardNextQuest` = 1963 WHERE `ID` = 1859; -- Therzok
UPDATE `quest_template` SET `RewardNextQuest` = 1861 WHERE `ID` = 1860; -- Speak with Jennea
UPDATE `quest_template` SET `RewardNextQuest` = 1880 WHERE `ID` = 1879; -- Speak with Bink
UPDATE `quest_template` SET `RewardNextQuest` = 1882 WHERE `ID` = 1881; -- Speak with Anastasia
UPDATE `quest_template` SET `RewardNextQuest` = 1884 WHERE `ID` = 1883; -- Speak with Un'thuwa
UPDATE `quest_template` SET `RewardNextQuest` = 1886 WHERE `ID` = 1885; -- Mennet Carkad
UPDATE `quest_template` SET `RewardNextQuest` = 2238 WHERE `ID` = 2218; -- Road to Salvation
UPDATE `quest_template` SET `RewardNextQuest` = 2759 WHERE `ID` = 2758; -- The Origins of Smithing
UPDATE `quest_template` SET `RewardNextQuest` = 95774 WHERE `ID` = 4921; -- Lost in Battle
UPDATE `quest_template` SET `RewardNextQuest` = 5482 WHERE `ID` = 5482; -- Doom Weed
UPDATE `quest_template` SET `RewardNextQuest` = 5658 WHERE `ID` = 5659; -- Touch of Weakness
UPDATE `quest_template` SET `RewardNextQuest` = 7877 WHERE `ID` = 7461; -- The Madness Within
UPDATE `quest_template` SET `RewardNextQuest` = 65597 WHERE `ID` = 65593; -- Hearts of the Lovers
UPDATE `quest_template` SET `RewardNextQuest` = 65610 WHERE `ID` = 65601; -- Love Hurts
UPDATE `quest_template` SET `RewardNextQuest` = 65603 WHERE `ID` = 65602; -- What Is Love?
UPDATE `quest_template` SET `RewardNextQuest` = 65604 WHERE `ID` = 65610; -- Wish You Were Here
UPDATE `quest_template` SET `RewardNextQuest` = 79364 WHERE `ID` = 79360; -- Elemental Aid
UPDATE `quest_template` SET `RewardNextQuest` = 79361 WHERE `ID` = 79364; -- Eyes on the Sky
UPDATE `quest_template` SET `RewardNextQuest` = 91859 WHERE `ID` = 91858; -- Diplomatic Incident
UPDATE `quest_template` SET `RewardNextQuest` = 91860 WHERE `ID` = 91859; -- A Curious Pair
UPDATE `quest_template` SET `RewardNextQuest` = 91861 WHERE `ID` = 91860; -- A Grim Fate
UPDATE `quest_template` SET `RewardNextQuest` = 91862 WHERE `ID` = 91861; -- Into Fenris Keep
UPDATE `quest_template` SET `RewardNextQuest` = 96204 WHERE `ID` = 91862; -- Lumina Windsinger
UPDATE `quest_template` SET `RewardNextQuest` = 92458 WHERE `ID` = 92432; -- An Alarming Request
UPDATE `quest_template` SET `RewardNextQuest` = 96984 WHERE `ID` = 92434; -- Blood in the Streets
UPDATE `quest_template` SET `RewardNextQuest` = 93035 WHERE `ID` = 93034; -- Valley of Bones
UPDATE `quest_template` SET `RewardNextQuest` = 93797 WHERE `ID` = 93791; -- Speak with Belann
UPDATE `quest_template` SET `RewardNextQuest` = 94978 WHERE `ID` = 94007; -- Taming the Beast
UPDATE `quest_template` SET `RewardNextQuest` = 96264 WHERE `ID` = 94789; -- Call of Air
UPDATE `quest_template` SET `RewardNextQuest` = 5929 WHERE `ID` = 94914; -- Moonglade
UPDATE `quest_template` SET `RewardNextQuest` = 95125 WHERE `ID` = 95111; -- An Underrated Talent
UPDATE `quest_template` SET `RewardNextQuest` = 95126 WHERE `ID` = 95125; -- Ott's Masterwork
UPDATE `quest_template` SET `RewardNextQuest` = 95034 WHERE `ID` = 96204; -- The Windshaper's Wrath
UPDATE `quest_template` SET `RewardNextQuest` = 97287 WHERE `ID` = 96984; -- Heart of Disruption
UPDATE `quest_template` SET `RewardNextQuest` = 98065 WHERE `ID` = 98046; -- Crown of the Earth
UPDATE `quest_template` SET `RewardNextQuest` = 98240 WHERE `ID` = 98072; -- Crocs of the Sky
UPDATE `quest_template` SET `RewardNextQuest` = 98190 WHERE `ID` = 98189; -- Return the Statuette
UPDATE `quest_template` SET `RewardNextQuest` = 98191 WHERE `ID` = 98190; -- A Friend of the Family
UPDATE `quest_template` SET `RewardNextQuest` = 98209 WHERE `ID` = 98208; -- Bloom of the Heavens
UPDATE `quest_template` SET `RewardNextQuest` = 98310 WHERE `ID` = 98216; -- Understanding Our Present
UPDATE `quest_template` SET `RewardNextQuest` = 98245 WHERE `ID` = 98240; -- Crimson Crate Delivery
UPDATE `quest_template` SET `RewardNextQuest` = 98313 WHERE `ID` = 98310; -- Gleaning Our Future
UPDATE `quest_template` SET `RewardNextQuest` = 2519 WHERE `ID` = 98391; -- The Sisterhood of Elune
UPDATE `quest_template` SET `RewardNextQuest` = 98398 WHERE `ID` = 98392; -- Darkness in the Glade
UPDATE `quest_template` SET `RewardNextQuest` = 98394 WHERE `ID` = 98393; -- The Great Cat Spirit
UPDATE `quest_template` SET `RewardNextQuest` = 98396 WHERE `ID` = 98394; -- The Great Cat Spirit
UPDATE `quest_template` SET `RewardNextQuest` = 98731 WHERE `ID` = 98396; -- The Great Cat Spirit
UPDATE `quest_template` SET `RewardNextQuest` = 98397 WHERE `ID` = 98731; -- Blessings of the Great Cat Spirit

-- 3. Spell cast on the player at turn-in (RewardSpell), 17 quests.
UPDATE `quest_template` SET `RewardSpell` = 364161 WHERE `ID` = 99; -- Arugal's Folly (ours 1460)
UPDATE `quest_template` SET `RewardSpell` = 1309120 WHERE `ID` = 905; -- The Angry Scytheclaws (ours 0)
UPDATE `quest_template` SET `RewardSpell` = 364161 WHERE `ID` = 1014; -- Arugal Must Die (ours 1460)
UPDATE `quest_template` SET `RewardSpell` = 366567 WHERE `ID` = 2203; -- Badlands Reagent Run II (ours 11522)
UPDATE `quest_template` SET `RewardSpell` = 364159 WHERE `ID` = 2359; -- Klaven's Tower (ours 2995)
UPDATE `quest_template` SET `RewardSpell` = 366567 WHERE `ID` = 2501; -- Badlands Reagent Run II (ours 11522)
UPDATE `quest_template` SET `RewardSpell` = 1299323 WHERE `ID` = 8193; -- Master Angler (ours 0)
UPDATE `quest_template` SET `RewardSpell` = 364163 WHERE `ID` = 8257; -- Blood of Morphaz (ours 9884)
UPDATE `quest_template` SET `RewardSpell` = 8385 WHERE `ID` = 94789; -- Call of Air (ours 0)
UPDATE `quest_template` SET `RewardSpell` = 1302696 WHERE `ID` = 97286; -- Research Access (ours 0)
UPDATE `quest_template` SET `RewardSpell` = 1309073 WHERE `ID` = 98093; -- Field to Clear (ours 0)
UPDATE `quest_template` SET `RewardSpell` = 1307068 WHERE `ID` = 98191; -- A Friend of the Family (ours 0)
UPDATE `quest_template` SET `RewardSpell` = 364161 WHERE `ID` = 98298; -- Arugal's Folly (ours 0)
UPDATE `quest_template` SET `RewardSpell` = 364161 WHERE `ID` = 98299; -- Stop the Spread (ours 0)
UPDATE `quest_template` SET `RewardSpell` = 1314842 WHERE `ID` = 98396; -- The Great Cat Spirit (ours 0)
UPDATE `quest_template` SET `RewardSpell` = 24962 WHERE `ID` = 98887; -- Conflict at Darkspear Islands (ours 0)
UPDATE `quest_template` SET `RewardSpell` = 24962 WHERE `ID` = 98888; -- Conflict at Darkspear Islands (ours 0)

-- 4. Reward items as live lists them: 24 quests with fixed items, 65 with a choice. Mostly the new Forever reward items
--    (27xxxx) next to the old ones. Where the live record shows no items at all we keep ours (see the report).
UPDATE `quest_template` SET `RewardItem1` = 250947, `RewardAmount1` = 5, `RewardItem2` = 0, `RewardAmount2` = 0, `RewardItem3` = 0, `RewardAmount3` = 0, `RewardItem4` = 0, `RewardAmount4` = 0 WHERE `ID` = 17; -- Uldaman Reagent Run
UPDATE `quest_template` SET `RewardItem1` = 16608, `RewardAmount1` = 1, `RewardItem2` = 263411, `RewardAmount2` = 1, `RewardItem3` = 0, `RewardAmount3` = 0, `RewardItem4` = 0, `RewardAmount4` = 0 WHERE `ID` = 31; -- Aquatic Form
UPDATE `quest_template` SET `RewardItem1` = 2842, `RewardAmount1` = 1, `RewardItem2` = 3342, `RewardAmount2` = 1, `RewardItem3` = 3344, `RewardAmount3` = 1, `RewardItem4` = 270257, `RewardAmount4` = 1 WHERE `ID` = 140; -- Captain Sander's Hidden Treasure
UPDATE `quest_template` SET `RewardItem1` = 414, `RewardAmount1` = 5, `RewardItem2` = 287061, `RewardAmount2` = 1, `RewardItem3` = 0, `RewardAmount3` = 0, `RewardItem4` = 0, `RewardAmount4` = 0 WHERE `ID` = 224; -- In Defense of the King's Lands
UPDATE `quest_template` SET `RewardItem1` = 2000, `RewardAmount1` = 1, `RewardItem2` = 270051, `RewardAmount2` = 1, `RewardItem3` = 0, `RewardAmount3` = 0, `RewardItem4` = 0, `RewardAmount4` = 0 WHERE `ID` = 231; -- A Daughter's Love
UPDATE `quest_template` SET `RewardItem1` = 270017, `RewardAmount1` = 1, `RewardItem2` = 0, `RewardAmount2` = 0, `RewardItem3` = 0, `RewardAmount3` = 0, `RewardItem4` = 0, `RewardAmount4` = 0 WHERE `ID` = 531; -- Vyrin's Revenge
UPDATE `quest_template` SET `RewardItem1` = 276720, `RewardAmount1` = 1, `RewardItem2` = 0, `RewardAmount2` = 0, `RewardItem3` = 0, `RewardAmount3` = 0, `RewardItem4` = 0, `RewardAmount4` = 0 WHERE `ID` = 549; -- WANTED: Syndicate Personnel
UPDATE `quest_template` SET `RewardItem1` = 281250, `RewardAmount1` = 1, `RewardItem2` = 0, `RewardAmount2` = 0, `RewardItem3` = 0, `RewardAmount3` = 0, `RewardItem4` = 0, `RewardAmount4` = 0 WHERE `ID` = 940; -- Teldrassil
UPDATE `quest_template` SET `RewardItem1` = 1116, `RewardAmount1` = 1, `RewardItem2` = 276242, `RewardAmount2` = 1, `RewardItem3` = 0, `RewardAmount3` = 0, `RewardItem4` = 0, `RewardAmount4` = 0 WHERE `ID` = 1046; -- Raene's Cleansing
UPDATE `quest_template` SET `RewardItem1` = 5176, `RewardAmount1` = 1, `RewardItem2` = 263412, `RewardAmount2` = 1, `RewardItem3` = 0, `RewardAmount3` = 0, `RewardItem4` = 0, `RewardAmount4` = 0 WHERE `ID` = 1527; -- Call of Fire
UPDATE `quest_template` SET `RewardItem1` = 250947, `RewardAmount1` = 5, `RewardItem2` = 0, `RewardAmount2` = 0, `RewardItem3` = 0, `RewardAmount3` = 0, `RewardItem4` = 0, `RewardAmount4` = 0 WHERE `ID` = 2202; -- Uldaman Reagent Run
UPDATE `quest_template` SET `RewardItem1` = 9527, `RewardAmount1` = 1, `RewardItem2` = 0, `RewardAmount2` = 0, `RewardItem3` = 0, `RewardAmount3` = 0, `RewardItem4` = 0, `RewardAmount4` = 0 WHERE `ID` = 2846; -- Tiara of the Deep
UPDATE `quest_template` SET `RewardItem1` = 10823, `RewardAmount1` = 1, `RewardItem2` = 0, `RewardAmount2` = 0, `RewardItem3` = 0, `RewardAmount3` = 0, `RewardItem4` = 0, `RewardAmount4` = 0 WHERE `ID` = 3341; -- Bring the End
UPDATE `quest_template` SET `RewardItem1` = 16608, `RewardAmount1` = 1, `RewardItem2` = 263411, `RewardAmount2` = 1, `RewardItem3` = 0, `RewardAmount3` = 0, `RewardItem4` = 0, `RewardAmount4` = 0 WHERE `ID` = 5061; -- Aquatic Form
UPDATE `quest_template` SET `RewardItem1` = 17001, `RewardAmount1` = 1, `RewardItem2` = 18022, `RewardAmount2` = 1, `RewardItem3` = 0, `RewardAmount3` = 0, `RewardItem4` = 0, `RewardAmount4` = 0 WHERE `ID` = 6163; -- Ramstein
UPDATE `quest_template` SET `RewardItem1` = 19120, `RewardAmount1` = 1, `RewardItem2` = 270225, `RewardAmount2` = 1, `RewardItem3` = 0, `RewardAmount3` = 0, `RewardItem4` = 0, `RewardAmount4` = 0 WHERE `ID` = 7862; -- Job Opening: Guard Captain of Revantusk Village
UPDATE `quest_template` SET `RewardItem1` = 19991, `RewardAmount1` = 1, `RewardItem2` = 19992, `RewardAmount2` = 1, `RewardItem3` = 20083, `RewardAmount3` = 1, `RewardItem4` = 0, `RewardAmount4` = 0 WHERE `ID` = 8232; -- The Green Drake
UPDATE `quest_template` SET `RewardItem1` = 19982, `RewardAmount1` = 1, `RewardItem2` = 19984, `RewardAmount2` = 1, `RewardItem3` = 20255, `RewardAmount3` = 1, `RewardItem4` = 0, `RewardAmount4` = 0 WHERE `ID` = 8236; -- The Azure Key
UPDATE `quest_template` SET `RewardItem1` = 20035, `RewardAmount1` = 1, `RewardItem2` = 20036, `RewardAmount2` = 1, `RewardItem3` = 20037, `RewardAmount3` = 1, `RewardItem4` = 0, `RewardAmount4` = 0 WHERE `ID` = 8253; -- Destroy Morphaz
UPDATE `quest_template` SET `RewardItem1` = 19990, `RewardAmount1` = 1, `RewardItem2` = 20006, `RewardAmount2` = 1, `RewardItem3` = 20082, `RewardAmount3` = 1, `RewardItem4` = 0, `RewardAmount4` = 0 WHERE `ID` = 8257; -- Blood of Morphaz
UPDATE `quest_template` SET `RewardItem1` = 20369, `RewardAmount1` = 1, `RewardItem2` = 20503, `RewardAmount2` = 1, `RewardItem3` = 20556, `RewardAmount3` = 1, `RewardItem4` = 0, `RewardAmount4` = 0 WHERE `ID` = 8413; -- Da Voodoo
UPDATE `quest_template` SET `RewardItem1` = 20530, `RewardAmount1` = 1, `RewardItem2` = 20534, `RewardAmount2` = 1, `RewardItem3` = 20536, `RewardAmount3` = 1, `RewardItem4` = 0, `RewardAmount4` = 0 WHERE `ID` = 8422; -- Trolls of a Feather
UPDATE `quest_template` SET `RewardItem1` = 20130, `RewardAmount1` = 1, `RewardItem2` = 20517, `RewardAmount2` = 1, `RewardItem3` = 20521, `RewardAmount3` = 1, `RewardItem4` = 0, `RewardAmount4` = 0 WHERE `ID` = 8425; -- Voodoo Feathers
UPDATE `quest_template` SET `RewardItem1` = 22272, `RewardAmount1` = 1, `RewardItem2` = 22274, `RewardAmount2` = 1, `RewardItem3` = 22458, `RewardAmount3` = 1, `RewardItem4` = 0, `RewardAmount4` = 0 WHERE `ID` = 9053; -- A Better Ingredient
UPDATE `quest_template` SET `RewardChoiceItemID1` = 1561, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 3578, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 276695, `RewardChoiceItemQuantity3` = 1, `RewardChoiceItemID4` = 0, `RewardChoiceItemQuantity4` = 0, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 9; -- The Killing Fields
UPDATE `quest_template` SET `RewardChoiceItemID1` = 2238, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 2249, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 287059, `RewardChoiceItemQuantity3` = 1, `RewardChoiceItemID4` = 0, `RewardChoiceItemQuantity4` = 0, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 11; -- Riverpaw Gnoll Bounty
UPDATE `quest_template` SET `RewardChoiceItemID1` = 1480, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 1566, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 12296, `RewardChoiceItemQuantity3` = 1, `RewardChoiceItemID4` = 270006, `RewardChoiceItemQuantity4` = 1, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 14; -- The People's Militia
UPDATE `quest_template` SET `RewardChoiceItemID1` = 1008, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 1158, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 4840, `RewardChoiceItemQuantity3` = 1, `RewardChoiceItemID4` = 276686, `RewardChoiceItemQuantity4` = 1, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 46; -- Bounty on Murlocs
UPDATE `quest_template` SET `RewardChoiceItemID1` = 6084, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 6085, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 6215, `RewardChoiceItemQuantity3` = 1, `RewardChoiceItemID4` = 270000, `RewardChoiceItemQuantity4` = 1, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 176; -- Wanted:  "Hogger"
UPDATE `quest_template` SET `RewardChoiceItemID1` = 17686, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 17687, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 270084, `RewardChoiceItemQuantity3` = 1, `RewardChoiceItemID4` = 270085, `RewardChoiceItemQuantity4` = 1, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 208; -- Big Game Hunter
UPDATE `quest_template` SET `RewardChoiceItemID1` = 3493, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 3566, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 5246, `RewardChoiceItemQuantity3` = 1, `RewardChoiceItemID4` = 270039, `RewardChoiceItemQuantity4` = 1, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 296; -- Ormer's Revenge
UPDATE `quest_template` SET `RewardChoiceItemID1` = 3461, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 3462, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 270020, `RewardChoiceItemQuantity3` = 1, `RewardChoiceItemID4` = 276722, `RewardChoiceItemQuantity4` = 1, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 442; -- Assault on Fenris Isle
UPDATE `quest_template` SET `RewardChoiceItemID1` = 3451, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 3582, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 276723, `RewardChoiceItemQuantity3` = 1, `RewardChoiceItemID4` = 0, `RewardChoiceItemQuantity4` = 0, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 451; -- A Recipe For Death
UPDATE `quest_template` SET `RewardChoiceItemID1` = 2230, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 3741, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 6482, `RewardChoiceItemQuantity3` = 1, `RewardChoiceItemID4` = 276724, `RewardChoiceItemQuantity4` = 1, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 501; -- Elixir of Pain
UPDATE `quest_template` SET `RewardChoiceItemID1` = 3747, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 3749, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 276727, `RewardChoiceItemQuantity3` = 1, `RewardChoiceItemID4` = 0, `RewardChoiceItemQuantity4` = 0, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 515; -- Elixir of Agony
UPDATE `quest_template` SET `RewardChoiceItemID1` = 3750, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 3751, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 270046, `RewardChoiceItemQuantity3` = 1, `RewardChoiceItemID4` = 0, `RewardChoiceItemQuantity4` = 0, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 547; -- Humbert's Sword
UPDATE `quest_template` SET `RewardChoiceItemID1` = 4138, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 270092, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 270093, `RewardChoiceItemQuantity3` = 1, `RewardChoiceItemID4` = 0, `RewardChoiceItemQuantity4` = 0, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 608; -- The Bloodsail Buccaneers
UPDATE `quest_template` SET `RewardChoiceItemID1` = 4120, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 270076, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 270077, `RewardChoiceItemQuantity3` = 1, `RewardChoiceItemID4` = 0, `RewardChoiceItemQuantity4` = 0, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 611; -- The Curse of the Tides
UPDATE `quest_template` SET `RewardChoiceItemID1` = 4129, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 270090, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 270091, `RewardChoiceItemQuantity3` = 1, `RewardChoiceItemID4` = 0, `RewardChoiceItemQuantity4` = 0, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 613; -- Cracking Maury's Foot
UPDATE `quest_template` SET `RewardChoiceItemID1` = 4118, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 270226, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 0, `RewardChoiceItemQuantity3` = 0, `RewardChoiceItemID4` = 0, `RewardChoiceItemQuantity4` = 0, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 630; -- Message in a Bottle
UPDATE `quest_template` SET `RewardChoiceItemID1` = 4507, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 4508, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 270082, `RewardChoiceItemQuantity3` = 1, `RewardChoiceItemID4` = 270083, `RewardChoiceItemQuantity4` = 1, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 646; -- Trol'kalar
UPDATE `quest_template` SET `RewardChoiceItemID1` = 9643, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 9644, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 270102, `RewardChoiceItemQuantity3` = 1, `RewardChoiceItemID4` = 270103, `RewardChoiceItemQuantity4` = 1, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 648; -- Rescue OOX-17/TN!
UPDATE `quest_template` SET `RewardChoiceItemID1` = 4543, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 4545, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 270073, `RewardChoiceItemQuantity3` = 1, `RewardChoiceItemID4` = 0, `RewardChoiceItemQuantity4` = 0, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 673; -- Foul Magics
UPDATE `quest_template` SET `RewardChoiceItemID1` = 9520, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 9521, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 270069, `RewardChoiceItemQuantity3` = 1, `RewardChoiceItemID4` = 0, `RewardChoiceItemQuantity4` = 0, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 679; -- Call to Arms
UPDATE `quest_template` SET `RewardChoiceItemID1` = 4976, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 4977, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 270079, `RewardChoiceItemQuantity3` = 1, `RewardChoiceItemID4` = 0, `RewardChoiceItemQuantity4` = 0, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 680; -- The Real Threat
UPDATE `quest_template` SET `RewardChoiceItemID1` = 4975, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 15104, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 270066, `RewardChoiceItemQuantity3` = 1, `RewardChoiceItemID4` = 0, `RewardChoiceItemQuantity4` = 0, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 697; -- Malin's Request
UPDATE `quest_template` SET `RewardChoiceItemID1` = 4987, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 270088, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 270089, `RewardChoiceItemQuantity3` = 1, `RewardChoiceItemID4` = 0, `RewardChoiceItemQuantity4` = 0, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 762; -- An Ambassador of Evil
UPDATE `quest_template` SET `RewardChoiceItemID1` = 5311, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 5312, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 270011, `RewardChoiceItemQuantity3` = 1, `RewardChoiceItemID4` = 0, `RewardChoiceItemQuantity4` = 0, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 898; -- Free From the Hold
UPDATE `quest_template` SET `RewardChoiceItemID1` = 5626, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 5627, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 270022, `RewardChoiceItemQuantity3` = 1, `RewardChoiceItemID4` = 0, `RewardChoiceItemQuantity4` = 0, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 943; -- The Absent Minded Prospector
UPDATE `quest_template` SET `RewardChoiceItemID1` = 10919, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 270008, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 270009, `RewardChoiceItemQuantity3` = 1, `RewardChoiceItemID4` = 0, `RewardChoiceItemQuantity4` = 0, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 962; -- Serpentbloom
UPDATE `quest_template` SET `RewardChoiceItemID1` = 15401, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 15402, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 270004, `RewardChoiceItemQuantity3` = 1, `RewardChoiceItemID4` = 0, `RewardChoiceItemQuantity4` = 0, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 982; -- Deep Ocean, Vast Sea
UPDATE `quest_template` SET `RewardChoiceItemID1` = 5813, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 270048, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 270049, `RewardChoiceItemQuantity3` = 1, `RewardChoiceItemID4` = 0, `RewardChoiceItemQuantity4` = 0, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 1012; -- Insane Druids
UPDATE `quest_template` SET `RewardChoiceItemID1` = 6675, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 6676, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 270035, `RewardChoiceItemQuantity3` = 1, `RewardChoiceItemID4` = 0, `RewardChoiceItemQuantity4` = 0, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 1059; -- Reclaiming the Charred Vale
UPDATE `quest_template` SET `RewardChoiceItemID1` = 6723, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 270095, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 0, `RewardChoiceItemQuantity3` = 0, `RewardChoiceItemID4` = 0, `RewardChoiceItemQuantity4` = 0, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 1139; -- The Lost Tablets of Will
UPDATE `quest_template` SET `RewardChoiceItemID1` = 10702, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 270086, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 270087, `RewardChoiceItemQuantity3` = 1, `RewardChoiceItemID4` = 0, `RewardChoiceItemQuantity4` = 0, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 1166; -- Overlord Mok'Morokk's Concern
UPDATE `quest_template` SET `RewardChoiceItemID1` = 10700, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 10701, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 270094, `RewardChoiceItemQuantity3` = 1, `RewardChoiceItemID4` = 0, `RewardChoiceItemQuantity4` = 0, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 1172; -- The Brood of Onyxia
UPDATE `quest_template` SET `RewardChoiceItemID1` = 269901, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 269903, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 0, `RewardChoiceItemQuantity3` = 0, `RewardChoiceItemID4` = 0, `RewardChoiceItemQuantity4` = 0, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 1371; -- Gizmo for Warug
UPDATE `quest_template` SET `RewardChoiceItemID1` = 11123, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 11124, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 270115, `RewardChoiceItemQuantity3` = 1, `RewardChoiceItemID4` = 0, `RewardChoiceItemQuantity4` = 0, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 1446; -- Jammal'an the Prophet
UPDATE `quest_template` SET `RewardChoiceItemID1` = 9626, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 9627, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 270078, `RewardChoiceItemQuantity3` = 1, `RewardChoiceItemID4` = 0, `RewardChoiceItemQuantity4` = 0, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 2240; -- The Hidden Chamber
UPDATE `quest_template` SET `RewardChoiceItemID1` = 9533, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 9534, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 270099, `RewardChoiceItemQuantity3` = 1, `RewardChoiceItemID4` = 0, `RewardChoiceItemQuantity4` = 0, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 2768; -- Divino-matic Rod
UPDATE `quest_template` SET `RewardChoiceItemID1` = 9531, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 270098, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 0, `RewardChoiceItemQuantity3` = 0, `RewardChoiceItemID4` = 0, `RewardChoiceItemQuantity4` = 0, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 2846; -- Tiara of the Deep
UPDATE `quest_template` SET `RewardChoiceItemID1` = 9658, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 9660, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 287073, `RewardChoiceItemQuantity3` = 1, `RewardChoiceItemID4` = 0, `RewardChoiceItemQuantity4` = 0, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 2987; -- Gordunni Cobalt
UPDATE `quest_template` SET `RewardChoiceItemID1` = 9665, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 9666, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 270104, `RewardChoiceItemQuantity3` = 1, `RewardChoiceItemID4` = 270105, `RewardChoiceItemQuantity4` = 1, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 3062; -- Dark Heart
UPDATE `quest_template` SET `RewardChoiceItemID1` = 10824, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 270081, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 0, `RewardChoiceItemQuantity3` = 0, `RewardChoiceItemID4` = 0, `RewardChoiceItemQuantity4` = 0, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 3341; -- Bring the End
UPDATE `quest_template` SET `RewardChoiceItemID1` = 10742, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 10743, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 10744, `RewardChoiceItemQuantity3` = 1, `RewardChoiceItemID4` = 270114, `RewardChoiceItemQuantity4` = 1, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 3463; -- Set Them Ablaze!
UPDATE `quest_template` SET `RewardChoiceItemID1` = 10653, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 10654, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 270038, `RewardChoiceItemQuantity3` = 1, `RewardChoiceItemID4` = 0, `RewardChoiceItemQuantity4` = 0, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 3514; -- Horde Presence
UPDATE `quest_template` SET `RewardChoiceItemID1` = 10749, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 10750, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 10751, `RewardChoiceItemQuantity3` = 1, `RewardChoiceItemID4` = 270117, `RewardChoiceItemQuantity4` = 1, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 3528; -- The God Hakkar
UPDATE `quest_template` SET `RewardChoiceItemID1` = 11964, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 12000, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 270123, `RewardChoiceItemQuantity3` = 1, `RewardChoiceItemID4` = 0, `RewardChoiceItemQuantity4` = 0, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 4126; -- Hurley Blackbreath
UPDATE `quest_template` SET `RewardChoiceItemID1` = 11964, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 12000, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 270123, `RewardChoiceItemQuantity3` = 1, `RewardChoiceItemID4` = 0, `RewardChoiceItemQuantity4` = 0, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 4134; -- Lost Thunderbrew Recipe
UPDATE `quest_template` SET `RewardChoiceItemID1` = 11866, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 11962, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 270120, `RewardChoiceItemQuantity3` = 1, `RewardChoiceItemID4` = 270121, `RewardChoiceItemQuantity4` = 1, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 4201; -- The Love Potion
UPDATE `quest_template` SET `RewardChoiceItemID1` = 12018, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 12021, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 12041, `RewardChoiceItemQuantity3` = 1, `RewardChoiceItemID4` = 270118, `RewardChoiceItemQuantity4` = 1, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 4242; -- Abandoned Hope
UPDATE `quest_template` SET `RewardChoiceItemID1` = 247840, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 247841, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 247846, `RewardChoiceItemQuantity3` = 1, `RewardChoiceItemID4` = 0, `RewardChoiceItemQuantity4` = 0, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 4402; -- Galgar's Cactus Apple Surprise
UPDATE `quest_template` SET `RewardChoiceItemID1` = 15464, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 15465, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 15466, `RewardChoiceItemQuantity3` = 1, `RewardChoiceItemID4` = 270037, `RewardChoiceItemQuantity4` = 1, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 5088; -- Arikara
UPDATE `quest_template` SET `RewardChoiceItemID1` = 15467, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 287066, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 0, `RewardChoiceItemQuantity3` = 0, `RewardChoiceItemID4` = 0, `RewardChoiceItemQuantity4` = 0, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 5151; -- Hypercapacitor Gizmo
UPDATE `quest_template` SET `RewardChoiceItemID1` = 15205, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 15206, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 15207, `RewardChoiceItemQuantity3` = 1, `RewardChoiceItemID4` = 270014, `RewardChoiceItemQuantity4` = 1, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 5321; -- The Sleeper Has Awakened
UPDATE `quest_template` SET `RewardChoiceItemID1` = 15691, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 15692, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 15695, `RewardChoiceItemQuantity3` = 1, `RewardChoiceItemID4` = 270065, `RewardChoiceItemQuantity4` = 1, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 5943; -- Gizelton Caravan
UPDATE `quest_template` SET `RewardChoiceItemID1` = 16987, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 16990, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 270028, `RewardChoiceItemQuantity3` = 1, `RewardChoiceItemID4` = 0, `RewardChoiceItemQuantity4` = 0, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 6283; -- Bloodfury Bloodline
UPDATE `quest_template` SET `RewardChoiceItemID1` = 1306, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 16981, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 270010, `RewardChoiceItemQuantity3` = 1, `RewardChoiceItemID4` = 0, `RewardChoiceItemQuantity4` = 0, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 6481; -- Earthen Arise
UPDATE `quest_template` SET `RewardChoiceItemID1` = 12038, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 270119, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 0, `RewardChoiceItemQuantity3` = 0, `RewardChoiceItemID4` = 0, `RewardChoiceItemQuantity4` = 0, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 7201; -- The Last Element
UPDATE `quest_template` SET `RewardChoiceItemID1` = 18410, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 18411, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 270127, `RewardChoiceItemQuantity3` = 1, `RewardChoiceItemID4` = 0, `RewardChoiceItemQuantity4` = 0, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 7441; -- Pusillin and the Elder Azj'Tordin
UPDATE `quest_template` SET `RewardChoiceItemID1` = 18491, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 270124, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 270125, `RewardChoiceItemQuantity3` = 1, `RewardChoiceItemID4` = 0, `RewardChoiceItemQuantity4` = 0, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 7489; -- Lethtendris's Web
UPDATE `quest_template` SET `RewardChoiceItemID1` = 19127, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 19128, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 270113, `RewardChoiceItemQuantity3` = 1, `RewardChoiceItemID4` = 0, `RewardChoiceItemQuantity4` = 0, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 7701; -- WANTED: Overseer Maltorius
UPDATE `quest_template` SET `RewardChoiceItemID1` = 19114, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 19115, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 270111, `RewardChoiceItemQuantity3` = 1, `RewardChoiceItemID4` = 0, `RewardChoiceItemQuantity4` = 0, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 7847; -- Return to Primal Torntusk
UPDATE `quest_template` SET `RewardChoiceItemID1` = 19118, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 270106, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 270107, `RewardChoiceItemQuantity3` = 1, `RewardChoiceItemID4` = 270108, `RewardChoiceItemQuantity4` = 1, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 7850; -- Dark Vessels
UPDATE `quest_template` SET `RewardChoiceItemID1` = 4130, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 270109, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 270110, `RewardChoiceItemQuantity3` = 1, `RewardChoiceItemID4` = 0, `RewardChoiceItemQuantity4` = 0, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 8554; -- Facing Negolash
UPDATE `quest_template` SET `RewardChoiceItemID1` = 247840, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 247841, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 247846, `RewardChoiceItemQuantity3` = 1, `RewardChoiceItemID4` = 0, `RewardChoiceItemQuantity4` = 0, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 91743; -- Rascally Rodents
UPDATE `quest_template` SET `RewardChoiceItemID1` = 281274, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 287060, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 0, `RewardChoiceItemQuantity3` = 0, `RewardChoiceItemID4` = 0, `RewardChoiceItemQuantity4` = 0, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 97282; -- Stormy Potential
UPDATE `quest_template` SET `RewardChoiceItemID1` = 281289, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 287062, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 0, `RewardChoiceItemQuantity3` = 0, `RewardChoiceItemID4` = 0, `RewardChoiceItemQuantity4` = 0, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 98025; -- WANTED: Jai'vhanel
UPDATE `quest_template` SET `RewardChoiceItemID1` = 279391, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 287064, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 0, `RewardChoiceItemQuantity3` = 0, `RewardChoiceItemID4` = 0, `RewardChoiceItemQuantity4` = 0, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 98093; -- Field to Clear

-- 5. Reward display spells (quest_reward_display_spell, which our server reads and which was empty): 259 spells on 214 quests.
--    Shown in the reward window. On 1 quests with no RewardSpell the server also casts them at turn-in, as live does: 93128.
DELETE FROM `quest_reward_display_spell` WHERE `QuestID` IN (31,96,889,1470,1471,1474,1485,1498,1504,1513,1518,1521,1527,1598,1599,1652,1661,1665,1678,1683,1689,1719,1739,1785,1788,1795,1819,2203,2359,2480,2501,2756,4490,5042,5043,5044,5045,5046,5061,5141,5143,5144,5145,5146,5148,5284,5302,5627,5628,5629,5630,5631,5632,5633,5634,5635,5636,5637,5639,5641,5642,5643,5644,5645,5646,5647,5652,5654,5655,5656,5657,5658,5660,5661,5662,5663,5672,5673,5674,5675,5676,5677,5678,5679,5680,6001,6002,6081,6082,6085,6086,6088,6089,6102,6103,6125,6130,6607,6610,6622,6624,7463,8193,9364,65597,65603,65604,90902,92468,93128,94003,94013,94050,94375,94441,94468,94505,94638,94773,94774,94775,94776,94777,94793,94817,94818,94819,94820,94821,94822,94824,94864,96031,96044,96045,96046,96047,96050,96055,96056,96057,96058,96059,96102,97257,97286,97899,97900,97901,97902,97903,97904,97905,97906,97907,97908,97915,97916,97917,97918,97919,97920,97921,97922,97923,97924,97925,97927,97928,97929,97931,97932,97933,97934,97935,97936,97937,97938,97939,97940,97941,97942,97943,97944,97946,97948,97949,97950,97951,97952,97953,97954,97955,97956,97957,97958,97959,97960,97961,97963,97964,97965,97967,97968,97969,97970,97971,97972,97973,98284,98285,98286,98362,98397);
INSERT INTO `quest_reward_display_spell` (`QuestID`,`Idx`,`SpellID`,`PlayerConditionID`,`Type`,`VerifiedBuild`) VALUES
(31,0,1066,0,3,70862),
(96,0,5394,0,3,70888),
(889,0,16618,0,0,70862),
(1470,0,688,0,3,70862),
(1471,0,697,0,3,70862),
(1474,0,712,0,3,70862),
(1485,0,688,0,3,70862),
(1498,0,71,0,3,70862),
(1498,1,7386,0,3,70862),
(1498,2,355,0,3,70862),
(1504,0,697,0,3,70862),
(1513,0,712,0,3,70862),
(1518,0,8071,0,3,70862),
(1521,0,8071,0,3,70862),
(1527,0,3599,0,3,70862),
(1598,0,688,0,3,70862),
(1599,0,688,0,3,70862),
(1652,0,5502,0,3,70862),
(1661,0,13819,0,3,70862),
(1665,0,71,0,3,70862),
(1665,1,7386,0,3,70862),
(1665,2,355,0,3,70862),
(1678,0,71,0,3,70862),
(1678,1,7386,0,3,70862),
(1678,2,355,0,3,70862),
(1683,0,71,0,3,70862),
(1683,1,7386,0,3,70862),
(1683,2,355,0,3,70862),
(1689,0,697,0,3,70862),
(1719,0,2458,0,3,70862),
(1719,1,20252,0,3,70862),
(1739,0,712,0,3,70862),
(1785,0,7328,0,3,70862),
(1788,0,7328,0,3,70862),
(1795,0,691,0,3,70862),
(1819,0,71,0,3,70862),
(1819,1,7386,0,3,70862),
(1819,2,355,0,3,70862),
(2203,0,1251744,0,2,70862),
(2359,0,2842,0,2,70862),
(2480,0,2842,0,2,70862),
(2501,0,1251744,0,2,70862),
(2756,0,9957,0,2,70862),
(4490,0,5784,0,3,70862),
(5042,0,16612,0,0,70862),
(5043,0,17013,0,0,70862),
(5044,0,7764,0,0,70862),
(5045,0,10767,0,0,70862),
(5046,0,16610,0,0,70862),
(5061,0,1066,0,3,70862),
(5141,0,10656,0,3,70888),
(5143,0,10660,0,3,70888),
(5144,0,10658,0,3,70888),
(5145,0,10656,0,3,70888),
(5146,0,10658,0,3,70888),
(5148,0,10660,0,3,70888),
(5284,0,9787,0,3,70862),
(5302,0,9787,0,3,70862),
(5627,0,10797,0,3,70862),
(5628,0,10797,0,3,70862),
(5629,0,10797,0,3,70862),
(5630,0,10797,0,3,70862),
(5631,0,10797,0,3,70862),
(5632,0,10797,0,3,70862),
(5633,0,10797,0,3,70862),
(5634,0,13908,0,3,70862),
(5635,0,13908,0,3,70862),
(5636,0,13908,0,3,70862),
(5637,0,13908,0,3,70862),
(5639,0,13908,0,3,70862),
(5641,0,1277331,0,3,70862),
(5642,0,18137,0,3,70862),
(5643,0,18137,0,3,70862),
(5644,0,1277324,0,3,70862),
(5645,0,1277331,0,3,70862),
(5646,0,1277324,0,3,70862),
(5647,0,1277331,0,3,70862),
(5652,0,9035,0,3,70862),
(5654,0,9035,0,3,70862),
(5655,0,9035,0,3,70862),
(5656,0,9035,0,3,70862),
(5657,0,9035,0,3,70862),
(5658,0,2652,0,3,70862),
(5660,0,2652,0,3,70862),
(5661,0,2652,0,3,70862),
(5662,0,2652,0,3,70862),
(5663,0,2652,0,3,70862),
(5672,0,2651,0,3,70862),
(5673,0,2651,0,3,70862),
(5674,0,2651,0,3,70862),
(5675,0,2651,0,3,70862),
(5676,0,13896,0,3,70862),
(5677,0,13896,0,3,70862),
(5678,0,13896,0,3,70862),
(5679,0,1277324,0,3,70862),
(5680,0,18137,0,3,70862),
(6001,0,5487,0,3,70862),
(6001,1,6795,0,3,70862),
(6001,2,6807,0,3,70862),
(6002,0,5487,0,3,70862),
(6002,1,6795,0,3,70862),
(6002,2,6807,0,3,70862),
(6081,0,5149,0,3,70862),
(6081,1,6991,0,3,70862),
(6081,2,982,0,3,70862),
(6082,0,1515,0,3,70862),
(6082,1,883,0,3,70862),
(6082,2,2641,0,3,70862),
(6085,0,1515,0,3,70862),
(6085,1,883,0,3,70862),
(6085,2,2641,0,3,70862),
(6086,0,5149,0,3,70862),
(6086,1,6991,0,3,70862),
(6086,2,982,0,3,70862),
(6088,0,1515,0,3,70862),
(6088,1,883,0,3,70862),
(6088,2,2641,0,3,70862),
(6089,0,5149,0,3,70862),
(6089,1,6991,0,3,70862),
(6089,2,982,0,3,70862),
(6102,0,1515,0,3,70862),
(6102,1,883,0,3,70862),
(6102,2,2641,0,3,70862),
(6103,0,5149,0,3,70862),
(6103,1,6991,0,3,70862),
(6103,2,982,0,3,70862),
(6125,0,8946,0,3,70862),
(6130,0,8946,0,3,70862),
(6607,0,18248,0,3,70862),
(6610,0,18260,0,3,70862),
(6622,0,10846,0,3,70862),
(6624,0,10846,0,3,70862),
(7463,0,10140,0,3,70862),
(8193,0,1299323,0,4,70862),
(9364,0,28272,0,3,70888),
(65597,0,713,0,3,70862),
(65603,0,713,0,3,70862),
(65604,0,713,0,3,70862),
(90902,0,635,0,0,70862),
(92468,0,8071,0,3,70862),
(93128,0,1261774,0,0,70888),
(94003,0,71,0,3,70862),
(94003,1,7386,0,3,70862),
(94003,2,355,0,3,70862),
(94013,0,1515,0,3,70862),
(94013,1,883,0,3,70862),
(94013,2,2641,0,3,70862),
(94050,0,5149,0,3,70862),
(94050,1,6991,0,3,70862),
(94050,2,982,0,3,70862),
(94375,0,8071,0,5,70862),
(94441,0,7328,0,0,70862),
(94468,0,3599,0,3,70862),
(94505,0,5394,0,3,70862),
(94638,0,5487,0,3,70862),
(94638,1,6795,0,3,70862),
(94638,2,6807,0,3,70862),
(94773,0,1277370,0,0,70862),
(94773,1,1277370,0,0,70862),
(94774,0,1277370,0,0,70862),
(94775,0,1277370,0,0,70862),
(94776,0,1277370,0,0,70862),
(94777,0,1277370,0,0,70862),
(94793,0,23357,0,0,70862),
(94817,0,1277455,0,0,70862),
(94817,1,1277455,0,0,70862),
(94818,0,1277462,0,0,70862),
(94818,1,1277462,0,0,70862),
(94819,0,1277462,0,0,70862),
(94820,0,1277462,0,0,70862),
(94821,0,1277455,0,0,70862),
(94822,0,1277455,0,0,70862),
(94824,0,1277455,0,0,70862),
(94864,0,23356,0,0,70862),
(94864,1,1515,0,3,70862),
(94864,2,883,0,3,70862),
(94864,3,2641,0,3,70862),
(96031,0,1229432,0,2,70862),
(96044,0,1255257,0,2,70862),
(96045,0,1230564,0,2,70862),
(96046,0,1230161,0,2,70862),
(96047,0,1230117,0,2,70862),
(96050,0,1229745,0,2,70862),
(96055,0,1229705,0,2,70862),
(96056,0,1229517,0,2,70862),
(96057,0,1263425,0,2,70862),
(96058,0,1230656,0,2,70862),
(96059,0,1230643,0,2,70862),
(96102,0,1229504,0,2,70862),
(97257,0,3599,0,3,70862),
(97286,0,1302508,0,3,70862),
(97899,0,1230564,0,2,70862),
(97900,0,1255257,0,2,70862),
(97901,0,1230643,0,2,70862),
(97902,0,1230656,0,2,70862),
(97903,0,1230117,0,2,70862),
(97904,0,1229745,0,2,70888),
(97905,0,1229705,0,2,70862),
(97906,0,1229432,0,2,70862),
(97907,0,1230161,0,2,70862),
(97908,0,1229517,0,2,70862),
(97915,0,1230564,0,2,70862),
(97916,0,1255257,0,2,70862),
(97917,0,1230643,0,2,70862),
(97918,0,1230656,0,2,70862),
(97919,0,1230117,0,2,70862),
(97920,0,1229745,0,2,70862),
(97921,0,1229705,0,2,70862),
(97922,0,1229432,0,2,70862),
(97923,0,1230161,0,2,70862),
(97924,0,1229517,0,2,70862),
(97925,0,1263425,0,2,70862),
(97927,0,1230564,0,2,70862),
(97928,0,1255257,0,2,70862),
(97929,0,1230643,0,2,70862),
(97931,0,1230117,0,2,70862),
(97932,0,1229745,0,2,70862),
(97933,0,1229705,0,2,70862),
(97934,0,1229432,0,2,70862),
(97935,0,1230161,0,2,70862),
(97936,0,1229517,0,2,70862),
(97937,0,1229504,0,2,70862),
(97938,0,1230564,0,2,70862),
(97939,0,1255257,0,2,70862),
(97940,0,1230643,0,2,70862),
(97941,0,1230656,0,2,70862),
(97942,0,1230117,0,2,70862),
(97943,0,1229745,0,2,70862),
(97944,0,1229705,0,2,70862),
(97946,0,1229432,0,2,70862),
(97948,0,1230161,0,2,70862),
(97949,0,1229517,0,2,70862),
(97950,0,1263425,0,2,70862),
(97951,0,1230564,0,2,70862),
(97952,0,1255257,0,2,70862),
(97953,0,1230643,0,2,70888),
(97954,0,1230656,0,2,70862),
(97955,0,1230117,0,2,70862),
(97956,0,1229745,0,2,70862),
(97957,0,1229705,0,2,70862),
(97958,0,1229432,0,2,70862),
(97959,0,1230161,0,2,70862),
(97960,0,1229517,0,2,70862),
(97961,0,1229504,0,2,70888),
(97963,0,1230564,0,2,70862),
(97964,0,1255257,0,2,70862),
(97965,0,1230117,0,2,70862),
(97967,0,1229745,0,2,70862),
(97968,0,1229705,0,2,70862),
(97969,0,1229432,0,2,70862),
(97970,0,1230161,0,2,70862),
(97971,0,1229517,0,2,70862),
(97972,0,1229504,0,2,70862),
(97973,0,1263425,0,2,70862),
(98284,0,1230643,0,2,70862),
(98285,0,1230656,0,2,70862),
(98286,0,1230643,0,2,70862),
(98362,0,768,0,3,70862),
(98397,0,768,0,3,70862);
