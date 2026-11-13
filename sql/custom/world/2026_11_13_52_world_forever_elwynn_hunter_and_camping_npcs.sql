-- WoW Forever: four Elwynn Forest NPCs live has and we lacked, so their quests could not be taken or handed in:
--   Tordrin Sternblade (248415), Hunter Trainer, Northshire: ends A Scribbled Letter (92479, the human hunter's first letter) and
--     Follow That Kobold! (91758), starts Shhh! We're Hunting Kobolds (91772). Trains hunters.
--   Josephine Carson (251507), Hunter Trainer, Goldshire: Taming the Beast (94792, 94863, 94864), starts Training the Beast (94793).
--   Isaac Chan (258930), Pet Trainer, Goldshire: ends Training the Beast (94793).
--   Sam Sarsaparilla (263399), Adventurer, Goldshire: ends The Adventurer (96627), The Great Outdoors (95998), starts Camping 101:
--     Cooking (96626) and Camping 101: Skinning (97924).
-- Name, title, type and model from players' live cache folders (client 70888, 2026-11-12; Tordrin's model is Thorgas Grimson's
-- 3395). Positions are live map points turned into world x/y with a fit on 43 Elwynn NPCs we already spawn (median error
-- 0.2 yards). z is the height of our nearest spawn (2.6 to 8.5 yards away): INFERRED, check with .gps in game.
-- INFERRED, copied from the closest NPC of the same kind we have: faction (Stormwind 12), level and flags, trainer lists and
-- trainer menus (Tordrin from Thorgas Grimson 895, the starting-zone hunter trainer; Josephine from Einris Brightspear 5515;
-- Isaac from Peria Lamenur 2878), Sam from Eric Brighthammer 265813 (the other Alliance Adventurer, same menu 42965).
-- Safe to run more than once. Spawn guids 46100000-46100009 (ours).

DROP TEMPORARY TABLE IF EXISTS `tmp_ct`;
CREATE TEMPORARY TABLE `tmp_ct` LIKE `creature_template`;
INSERT INTO `tmp_ct` SELECT * FROM `creature_template` WHERE `entry` IN (895, 5515, 2878, 265813);
UPDATE `tmp_ct` SET `entry` = 248415, `name` = 'Tordrin Sternblade', `subname` = 'Hunter Trainer', `faction` = 12, `VerifiedBuild` = 0 WHERE `entry` = 895;
UPDATE `tmp_ct` SET `entry` = 251507, `name` = 'Josephine Carson', `subname` = 'Hunter Trainer', `faction` = 12, `VerifiedBuild` = 0 WHERE `entry` = 5515;
UPDATE `tmp_ct` SET `entry` = 258930, `name` = 'Isaac Chan', `subname` = 'Pet Trainer', `faction` = 12, `VerifiedBuild` = 0 WHERE `entry` = 2878;
UPDATE `tmp_ct` SET `entry` = 263399, `name` = 'Sam Sarsaparilla', `subname` = 'Adventurer', `faction` = 12, `VerifiedBuild` = 0 WHERE `entry` = 265813;
DELETE FROM `creature_template` WHERE `entry` IN (248415, 251507, 258930, 263399);
INSERT INTO `creature_template` SELECT * FROM `tmp_ct`;
DROP TEMPORARY TABLE `tmp_ct`;

DROP TEMPORARY TABLE IF EXISTS `tmp_ctd`;
CREATE TEMPORARY TABLE `tmp_ctd` LIKE `creature_template_difficulty`;
INSERT INTO `tmp_ctd` SELECT * FROM `creature_template_difficulty` WHERE `Entry` IN (895, 5515, 2878, 265813);
UPDATE `tmp_ctd` SET `Entry` = CASE `Entry` WHEN 895 THEN 248415 WHEN 5515 THEN 251507 WHEN 2878 THEN 258930 ELSE 263399 END, `VerifiedBuild` = 0;
DELETE FROM `creature_template_difficulty` WHERE `Entry` IN (248415, 251507, 258930, 263399);
INSERT INTO `creature_template_difficulty` SELECT * FROM `tmp_ctd`;
DROP TEMPORARY TABLE `tmp_ctd`;

DELETE FROM `creature_template_model` WHERE `CreatureID` IN (248415, 251507, 258930, 263399);
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`) VALUES
(248415, 0, 3395, 1, 1, 70888),
(251507, 0, 137105, 1, 1, 70888),
(258930, 0, 141199, 1, 1, 70888),
(263399, 0, 143698, 1, 1, 70888);

DELETE FROM `creature_template_gossip` WHERE `CreatureID` IN (248415, 251507, 258930, 263399);
INSERT INTO `creature_template_gossip` (`CreatureID`, `MenuID`, `VerifiedBuild`) VALUES
(248415, 4675, 0), (251507, 4474, 0), (258930, 4783, 0), (263399, 42965, 0);

DELETE FROM `creature_trainer` WHERE `CreatureID` IN (248415, 251507, 258930);
INSERT INTO `creature_trainer` (`CreatureID`, `TrainerID`, `MenuID`, `OptionID`) VALUES
(248415, 19, 4675, 0), (251507, 20, 4474, 0), (258930, 1002878, 4783, 1);

DELETE FROM `creature` WHERE `guid` BETWEEN 46100000 AND 46100009;
INSERT INTO `creature` (`guid`,`id`,`map`,`zoneId`,`areaId`,`spawnDifficulties`,`phaseUseFlags`,`PhaseId`,`PhaseGroup`,`terrainSwapMap`,`modelid`,`equipment_id`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`currentwaypoint`,`curHealthPct`,`MovementType`,`npcflag`,`unit_flags`,`unit_flags2`,`unit_flags3`,`ScriptName`,`StringId`,`VerifiedBuild`) VALUES
(46100000,248415,0,0,0,'0',0,0,0,-1,0,0,-8884.13,-241.79,81.681,0.0,300,0.0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0),
(46100001,251507,0,0,0,'0',0,0,0,-1,0,0,-9472.48,107.10,57.399,0.0,300,0.0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0),
(46100002,258930,0,0,0,'0',0,0,0,-1,0,0,-9475.72,84.90,57.098,0.0,300,0.0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0),
(46100003,263399,0,0,0,'0',0,0,0,-1,0,0,-9405.59,-23.30,64.070,0.0,300,0.0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0);

DELETE FROM `creature_queststarter` WHERE `id` IN (248415, 251507, 258930, 263399);
INSERT INTO `creature_queststarter` (`id`, `quest`, `VerifiedBuild`) VALUES
(248415, 91772, 0),
(251507, 94792, 0), (251507, 94793, 0), (251507, 94863, 0), (251507, 94864, 0),
(263399, 95998, 0), (263399, 96626, 0), (263399, 97924, 0);
DELETE FROM `creature_questender` WHERE `id` IN (248415, 251507, 258930, 263399);
INSERT INTO `creature_questender` (`id`, `quest`, `VerifiedBuild`) VALUES
(248415, 91758, 0), (248415, 92479, 0),
(251507, 94792, 0), (251507, 94863, 0), (251507, 94864, 0),
(258930, 94793, 0),
(263399, 95998, 0), (263399, 96627, 0);
