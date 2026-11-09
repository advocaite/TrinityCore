-- Call of Fire 94467 (dwarf shaman, 4th step, Braldir Ashmantle 257808 in Loch Modan): "use the fire sapta at the shrine,
-- climb to the peak, defeat the Minor Manifestation of Fire and place the Glowing Ember in the brazier".
-- Discord 11-08 (dmitriibard): the Fire Sapta says it needs the shrine, and after a forced completion nothing happens.
--
-- Cause: the Loch Modan shrine had none of its parts in our world. The Fire Sapta (6636, Sapta Sight 8898) needs spell
-- focus 63 (Shaman Shrine, focus object 63674); the only foci are at the old shrines (Durotar, Barrens, ...). The
-- Minor Manifestation of Fire (5893, drops the Glowing Ember 6655) and the Brazier of the Dormant Flame (61934, ends
-- 94467 and starts 94468) were spawned only at the old Durotar shrine. So the sapta could not be used, the elemental
-- could not be found, and there was nothing to hand the quest in to.
--
-- Spots: Questie Forever data (Questie team, Questie/QuestieDB, GPLv3): brazier 61934 at Loch Modan 31.79 64.52 and
-- Minor Manifestation of Fire at 31.96 64.42, i.e. about 28 yd north of Braldir and "above" him, as the quest comments say.
-- Map percent to world with the Loch Modan map bounds (same sum gives the Dun Morogh Earth spirit spot to 1 yd).
-- Height: not in the data. The Durotar copy of this shrine has the brazier 19 yd above Telf Joolam, the same distance away
-- (34 yd); Braldir stands at 457.5, so 476.5 is used here. The PC steps check the real height in game and fix it if needed.
-- The focus stands where Braldir is (radius 50), as at Durotar where it stands next to Telf Joolam; it also covers the peak.
-- Safe to run again.
DELETE FROM `gameobject` WHERE `guid` IN (21311081,21311082);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`zoneId`,`areaId`,`spawnDifficulties`,`phaseUseFlags`,`PhaseId`,`PhaseGroup`,`terrainSwapMap`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`animprogress`,`state`,`ScriptName`,`VerifiedBuild`) VALUES
(21311081,61934,0,0,0,'0',0,0,0,-1,-5674.4,-2870.6,476.5,0.2269,0,0,0.113203,0.993572,900,100,1,'',0), -- Brazier of the Dormant Flame (Loch Modan)
(21311082,63674,0,0,0,'0',0,0,0,-1,-5701.62,-2876.42,457.478,0,0,0,0,1,120,255,1,'',0);           -- Shaman Shrine focus at Braldir Ashmantle

DELETE FROM `creature` WHERE `guid`=21311080;
INSERT INTO `creature` (`guid`,`id`,`map`,`zoneId`,`areaId`,`spawnDifficulties`,`phaseUseFlags`,`PhaseId`,`PhaseGroup`,`terrainSwapMap`,`modelid`,`equipment_id`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`currentwaypoint`,`curHealthPct`,`MovementType`,`npcflag`,`unit_flags`,`unit_flags2`,`unit_flags3`,`ScriptName`,`VerifiedBuild`) VALUES
(21311080,5893,0,0,0,'0',0,0,0,-1,0,0,-5672.6,-2875.3,476.6,3.5,10,0,0,100,0,NULL,NULL,NULL,NULL,'',0); -- Minor Manifestation of Fire (Loch Modan), 10 s respawn like the Durotar one

-- The elemental is only seen with the sapta (Elemental Spirit Invisibility 8203, invisibility type 1; Sapta Sight 8898
-- detects type 1). Kept as it is if the template already has an addon row (the Durotar quest uses the same NPC).
INSERT IGNORE INTO `creature_template_addon` (`entry`,`PathId`,`mount`,`MountCreatureID`,`StandState`,`AnimTier`,`VisFlags`,`SheathState`,`PvPFlags`,`emote`,`aiAnimKit`,`movementAnimKit`,`meleeAnimKit`,`visibilityDistanceType`,`auras`) VALUES
(5893,0,0,0,0,0,0,1,0,0,0,0,0,0,'8203');
