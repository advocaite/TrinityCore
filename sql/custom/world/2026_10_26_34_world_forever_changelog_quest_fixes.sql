-- Fixes from cross-checking the other Forever server changelogs against our data.

-- Skyborne druids could not learn Aquatic Form or Cure Poison: the Night Elf and Tauren druid quest lines that teach them
-- were open to those two races only. The other Forever server (world database) opens the Night Elf lines to the High Order (Skyborne
-- Alliance, race mask bit 32): A Lesson to Learn 26, Trial of the Lake 29, Trial of the Sea Lion 272, Aquatic Form 5061,
-- Lessons Anew 6121, The Principal Source 6122, Gathering the Cure 6123, Power over Poison 6125 (6124 is open to all).
-- The other Forever server does the same for the Windshapers (Skyborne Horde, bit 33) on the Tauren lines: A Lesson to Learn 27, Trial
-- of the Lake 28, Aquatic Form 31, Lessons Anew 6126 - Power over Poison 6130 (Trial of the Sea Lion 30 is open to all).
UPDATE `quest_template` SET `AllowableRaces`=`AllowableRaces`|(1<<32) WHERE `ID` IN (26,29,272,5061,6121,6122,6123,6125);
UPDATE `quest_template` SET `AllowableRaces`=`AllowableRaces`|(1<<33) WHERE `ID` IN (27,28,31,6126,6127,6128,6129,6130);

-- Quest-starting items that nothing dropped (the other Forever server "quest-starting items that never dropped now do"); loot from the
-- The other Forever server world database: Tattered Note (212748, starts Storm, Earth, and Fire 79358) and Empty Powder Keg (268548,
-- starts 95213).


-- quest-starting item 212748
DELETE FROM `creature_loot_template` WHERE `Entry`=4421 AND `Item`=212748;
INSERT INTO `creature_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES
(4421,0,212748,7.767,0,1,0,1,1,'Charlga Razorflank - Tattered Note (starts 79358), Wowhead Forever 161/2073 (1111)');

-- quest-starting item 255155

-- quest-starting item 268548
DELETE FROM `creature_loot_template` WHERE `Entry`=1116 AND `Item`=268548;
INSERT INTO `creature_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES
(1116,0,268548,48.0,1,1,0,1,1,'Wowhead Forever drop 136/9171 x1.84 floor 48.0 (quest item, INFERRED, 1328)');
DELETE FROM `creature_loot_template` WHERE `Entry`=1118 AND `Item`=268548;
INSERT INTO `creature_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES
(1118,0,268548,48.0,1,1,0,1,1,'Wowhead Forever drop 26/1783 x1.84 floor 48.0 (quest item, INFERRED, 1328)');

-- quest-starting item 275722

-- quest-starting item 281145

-- quest-starting item 281147

-- Child of Nature (94911, the Night Elf/High Order copy) had no class restriction, so every class was offered it; the
-- other copy (94912) and the other Forever server have it druid-only ("Child of Nature is for druids only", the other Forever server changelog).
INSERT INTO `quest_template_addon` (`ID`,`AllowableClasses`) VALUES (94911,1024) ON DUPLICATE KEY UPDATE `AllowableClasses`=1024;
