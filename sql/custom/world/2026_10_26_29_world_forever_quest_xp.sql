-- Quest XP from the Wowhead WoW Forever quest list (10-25), where ours differed by more than 10% at the quest's Forever level.
-- XP = QuestXP.db2[quest level][RewardXPDifficulty]; each difficulty below gives the list's value exactly.
-- Kept as they are: 13 quests from the 70124 Forever sniff. Ten carry a sniffed XP multiplier (2.0-2.2) that the list leaves
-- out, and the list's base value matches their difficulty. 91745, 91758 and 91772 differ from the sniff and need a quest page.

-- no XP: Lunar Festival elders and Fish in a Bucket (vMaNGOS gives no XP either)
UPDATE `quest_template` SET `RewardXPDifficulty`=0 WHERE `ID` IN (5421,8619,8635,8636,8642,8643,8644,8645,8646,8647,8648,8649,8650,8651,8652,8653,8654,8670,8671,8672,8673,8674,8675,8676,8677,8678,8679,8680,8681,8682,8683,8684,8685,8686,8688,8713,8714,8715,8716,8717,8718,8719,8720,8721,8722,8723,8724,8725,8726,8727,8866);

-- 100 Call of Water 185 (vanilla 0); 688 Myzrael's Allies 315 (vanilla 2350)
UPDATE `quest_template` SET `RewardXPDifficulty`=1 WHERE `ID` IN (100,688);

-- battleground mark quests at their Forever brackets (vanilla gives the same), AQ class armor 6600 (vanilla 9950),
-- 228 Mor'Ladim 2750, 606 Scaring Shaky 3300, 745 Sharing the Land 700
UPDATE `quest_template` SET `RewardXPDifficulty`=5 WHERE `ID` IN (228,606,745,7788,7789,7872,7875,8154,8155,8160,8162,8544,8559,8602,8621,8637,8641,8660,8664,8665,8669);
