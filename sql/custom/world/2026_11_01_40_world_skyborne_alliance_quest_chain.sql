-- Zephras Isle: the Alliance Skyborne story stopped after the first village. Three quests open for both factions had the Horde
-- quest of a faction pair as their only previous quest, so an Alliance Skyborne could never take them:
--   The Criminal Element 92517 needed Welcome to Shen'dar Village 92514 (Horde; the Alliance one is 93461)
--   Tower Defense 93320 needed In Service of Zephras 92871 (Horde; the Alliance one is 92860), and everything after it
--   (Return to Valanaar, The Inner Sanctum, the Portal To Rohashi Spires, Confront Lorthuna) hung on Tower Defense
--   Bugged 93949 needed To Valanaar 92579 (Horde; the Alliance one is 92701)
-- Either quest of the pair now unlocks the shared one (NextQuestID of both, no PrevQuestID). 93461 also gets its PrevQuestID
-- The Next Step 92472, like the Horde 92514. Safe to run again.
INSERT INTO `quest_template_addon` (`ID`,`PrevQuestID`,`NextQuestID`) VALUES
(92514,92472,92517),
(93461,92472,92517),
(92871,93746,93320),
(92860,92840,93320),
(92579,93927,93949),
(92701,93927,93949)
ON DUPLICATE KEY UPDATE `PrevQuestID` = VALUES(`PrevQuestID`), `NextQuestID` = VALUES(`NextQuestID`);
INSERT INTO `quest_template_addon` (`ID`,`PrevQuestID`) VALUES
(92517,0),
(93320,0),
(93949,0)
ON DUPLICATE KEY UPDATE `PrevQuestID` = VALUES(`PrevQuestID`);
