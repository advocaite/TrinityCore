-- 92753 Destruction in Deadmines (Westfall, Alliance): the Extra-Destructive Explosives (item 254553) could not be used.
-- Player report 2026-11-08 (#bug-report, zeroyka).
-- Item 254553 and its use spell 1258822 "Place Explosives" (6 s cast, kill credit 253370) are in the client and server
-- DB2 files. The spell needs spell focus 2240 "Deadmines Foundry" (SpellCastingRequirements), and no object with that
-- focus existed in our world, so the cast always failed. Blizzard's own focus object is not in any open data, so this adds
-- a server-only spell focus at the foundry. The quest rows below are Blizzard's quest record (player caches 11-08, build 70862).

-- A. Spell focus "Deadmines Foundry" (server only, the client never sees it). Data0 = focus 2240, Data1 = 40 yd.
DELETE FROM `gameobject_template` WHERE `entry`=9275301;
INSERT INTO `gameobject_template` (`entry`,`type`,`displayId`,`name`,`IconName`,`castBarCaption`,`unk1`,`size`,`Data0`,`Data1`,`Data2`,`Data3`,`Data4`,`Data5`,`ContentTuningId`,`RequiredLevel`,`AIName`,`ScriptName`,`VerifiedBuild`) VALUES
(9275301,8,0,'Deadmines Foundry','','','',1,2240,40,0,1,0,0,0,0,'','',0);

-- One spawn where Gilnid (1763, "The Smelter") stands in the Goblin Foundry, taken from our own creature row so the
-- map, phase and difficulty match. If Gilnid has no spawn row, the fallback point is the foundry floor near him.
DELETE FROM `gameobject` WHERE `guid`=927530100;
INSERT INTO `gameobject` (`guid`,`id`,`map`,`zoneId`,`areaId`,`spawnDifficulties`,`phaseUseFlags`,`PhaseId`,`PhaseGroup`,`terrainSwapMap`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`animprogress`,`state`,`ScriptName`,`VerifiedBuild`)
SELECT 927530100,9275301,c.`map`,c.`zoneId`,c.`areaId`,c.`spawnDifficulties`,c.`phaseUseFlags`,c.`PhaseId`,c.`PhaseGroup`,c.`terrainSwapMap`,c.`position_x`,c.`position_y`,c.`position_z`,0,0,0,0,1,300,255,1,'',0
FROM `creature` c WHERE c.`id`=1763 AND c.`map`=36 ORDER BY c.`guid` LIMIT 1;
INSERT IGNORE INTO `gameobject` (`guid`,`id`,`map`,`zoneId`,`areaId`,`spawnDifficulties`,`phaseUseFlags`,`PhaseId`,`PhaseGroup`,`terrainSwapMap`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`animprogress`,`state`,`ScriptName`,`VerifiedBuild`) VALUES
(927530100,9275301,36,0,0,'1',0,0,0,-1,-213.4,-591.6,21.0,0,0,0,0,1,300,255,1,'',0);

-- B. The quest hands out the explosives when it is taken (Blizzard: StartItem 254553), so a player who lost them gets new ones.
UPDATE `quest_template` SET `StartItem`=254553 WHERE `ID`=92753;

-- C. Blizzard's two objectives in place of our own one: the kill credit from Place Explosives, and the explosives
-- themselves as a hidden optional item objective (flags 0x2C: optional, hidden, keep quest items).
DELETE FROM `quest_objectives` WHERE `QuestID`=92753;
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`Flags`,`Visible`,`Description`,`VerifiedBuild`) VALUES
(465741,92753,0,0,0,253370,1,32,1,'Explosives placed',70862),
(477699,92753,1,1,1,254553,1,44,1,NULL,70862);
