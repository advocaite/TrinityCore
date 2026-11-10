-- 97924 Camping 101: Skinning (Elwynn Forest): Sam Sarsaparilla in Goldshire offers the Elwynn set of Camping 101 quests
-- after The Great Outdoors (95998), but 97924 was missing from the data. It is on the Forever quest list (level 6, req 4,
-- Alliance). INFERRED: text, skill objective and reward spell 1255250 (Camp Chair) from the Horde copy 97960; giver,
-- prerequisite and "skinning skill 1" requirement as the Elwynn Herbalism one 97921. The objective uses criteria tree 231084
-- "Raise Skinning to 20+" from the 70532 CriteriaTree.db2 (the Elwynn set runs 231078 herbalism, 231082 mining, 231084
-- skinning; each quest needs its own tree). Turn-in at the Goldshire skinning trainer Helene Peltskinner, as the other
-- copies turn in at their zone's skinning trainer. Check against the quest page when it is fetched.
DELETE FROM `quest_template` WHERE `ID`=97924;
CREATE TEMPORARY TABLE `tmp_q97924` AS SELECT * FROM `quest_template` WHERE `ID`=97960;
UPDATE `tmp_q97924` SET `ID`=97924, `AllowableRaces`=6130900294268439629, `VerifiedBuild`=0,
  `LogDescription`='Speak with Helene Peltskinner in Goldshire to learn to craft a camping item for your profession once you have shown proficiency in your skill.';
INSERT INTO `quest_template` SELECT * FROM `tmp_q97924`;
DROP TEMPORARY TABLE `tmp_q97924`;

DELETE FROM `quest_objectives` WHERE `QuestID`=97924;
CREATE TEMPORARY TABLE `tmp_o97924` AS SELECT * FROM `quest_objectives` WHERE `QuestID`=97960;
UPDATE `tmp_o97924` SET `ID`=9792400, `QuestID`=97924, `ObjectID`=231084, `VerifiedBuild`=0;
INSERT INTO `quest_objectives` SELECT * FROM `tmp_o97924`;
DROP TEMPORARY TABLE `tmp_o97924`;

DELETE FROM `quest_classic_level` WHERE `ID`=97924;
INSERT INTO `quest_classic_level` SELECT 97924, `QuestLevel`, `MinLevel`, `MaxLevel` FROM `quest_classic_level` WHERE `ID`=97960;
DELETE FROM `quest_template_classic_level` WHERE `ID`=97924;
INSERT INTO `quest_template_classic_level` SELECT 97924, `LevelType`, `QuestLevel` FROM `quest_template_classic_level` WHERE `ID`=97960;

DELETE FROM `quest_template_addon` WHERE `ID`=97924;
INSERT INTO `quest_template_addon` (`ID`,`PrevQuestID`,`RequiredSkillID`,`RequiredSkillPoints`) VALUES (97924,95998,393,1);

DELETE FROM `creature_queststarter` WHERE `quest`=97924;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (263399,97924,0);
DELETE FROM `creature_questender` WHERE `quest`=97924;
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (6306,97924,0);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry`=6306;
