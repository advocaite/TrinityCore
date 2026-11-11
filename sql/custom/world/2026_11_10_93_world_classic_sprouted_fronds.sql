-- The Sprouted Fronds (2399, Teldrassil): a player reported (Discord, 2026-11-10) that the quest can't be taken. It starts and
-- ends at a Sprouted Frond (gameobject 7510), but the three fronds next to Denalan (2080) are spawned hidden (spawn time -600):
-- in vanilla they sprouted when Denalan planted the Shimmering Frond after The Shimmering Frond (931), and the VMaNGOS quest
-- end script of 931 came over without that step, so nothing ever grew. A player who has already turned in 931 could never get
-- the quest.
-- The fronds now simply stand there (60 s respawn). Only a player who has done 931 sees the quest on them (2399 needs 931).
-- Safe to run again.

UPDATE `gameobject` SET `spawntimesecs` = 60 WHERE `guid` IN (751000, 751001, 751002) AND `id` = 7510;

-- 2399 follows 931 (VMaNGOS / AzerothCore PrevQuestId); our row had no prerequisite.
INSERT INTO `quest_template_addon` (`ID`, `PrevQuestID`) VALUES (2399, 931)
ON DUPLICATE KEY UPDATE `PrevQuestID` = IF(`PrevQuestID` = 0, 931, `PrevQuestID`);
