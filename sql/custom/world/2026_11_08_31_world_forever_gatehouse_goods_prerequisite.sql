-- Gatehouse Goods (97222, Elaine Trias, Goldshire): needs only Philmor's Favor (97220) first.
-- 2026_11_05_99 made it wait for all of 97220, Yelmak's Medley (97242) and Favorite Food (97249), read from a Wowhead series list.
-- A series list only shows quests that belong together, not what must come first. Questie's Forever data (QuestieDB 5fba5d8,
-- player traces and fixes) has 97220 as the only quest before it, and a Russian player reported (2026-11-07) that the
-- quest never showed up after the chain. Safe to run again.
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId`=19 AND `SourceEntry`=97222 AND `ConditionTypeOrReference`=8 AND `ConditionValue1` IN (97242,97249);
UPDATE `conditions` SET `Comment`='Gatehouse Goods - after Philmor''s Favor (97220, Questie Forever)'
 WHERE `SourceTypeOrReferenceId`=19 AND `SourceEntry`=97222 AND `ConditionTypeOrReference`=8 AND `ConditionValue1`=97220;
