-- Zephras Isle (Shen'dar Village) Camping 101: Enchanting and Tailoring come in a Horde and an Alliance version with the same text
-- and turn-in. The sniffed Horde ones (98284, 97972) ask for the profession and follow The Great Outdoors (96101), like every
-- other Zephras Camping 101 quest; the Alliance twins did not:
--  98286 Enchanting (A): no objective, no profession gate, no previous quest, so an Alliance Skyborne could take it without being
--        an enchanter and finish it by talking. It gets the same gate and an objective on the Camp Training tree "Raise Enchanting
--        to 20+". The core maps one objective per tree, so it can't share 98284's 231647; 231651 is the unused Enchanting tree
--        added next to it in the 70532 CriteriaTree.db2, and all these trees are the same (criteria type 7, skill line 2940,
--        modifier tree 446611), so the choice changes nothing in play.
--  97973 Tailoring (A): its sniffed objective (tree 231176) was there; the profession gate was missing.
-- 98285 Camping 101: Engineering (Strange Hermit, both sides): "come back and see me when you've gained a little bit of experience
-- as an engineer", but it had no objective and no gate. It gets the Zephras Engineering tree 231649 ("Raise Engineering to 20+",
-- skill line 2941) and the Engineering gate. Safe to run again.
INSERT INTO `quest_template_addon` (`ID`,`PrevQuestID`,`RequiredSkillID`,`RequiredSkillPoints`) VALUES
(98286,96101,333,1),
(97973,96101,197,1)
ON DUPLICATE KEY UPDATE `PrevQuestID`=VALUES(`PrevQuestID`), `RequiredSkillID`=VALUES(`RequiredSkillID`), `RequiredSkillPoints`=VALUES(`RequiredSkillPoints`);
INSERT INTO `quest_template_addon` (`ID`,`RequiredSkillID`,`RequiredSkillPoints`) VALUES (98285,202,1)
ON DUPLICATE KEY UPDATE `RequiredSkillID`=VALUES(`RequiredSkillID`), `RequiredSkillPoints`=VALUES(`RequiredSkillPoints`);
DELETE FROM `quest_objectives` WHERE `QuestID` IN (98285,98286);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES
(9828600,98286,14,0,0,231651,1,0,32,0,0,0,1,'Raise your enchanting skill to 20',0),
(9828500,98285,14,0,0,231649,1,0,32,0,0,0,1,'Raise your engineering skill to 20',0);
