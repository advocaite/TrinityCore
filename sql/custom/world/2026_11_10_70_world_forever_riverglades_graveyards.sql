-- Riverglades (zone 16591): the zone had no graveyard and no spirit healer, so a player who died there was sent to the
-- default graveyard far away (Westfall / The Barrens). Four graveyards with a Spirit Healer each, one per town.
-- ESTIMATED positions: no sniff has the real release points. Each spot is 6 yd from one of our town NPCs and uses that NPC's height;
-- the healer stands next to it. Check in game by dying near each town, and swap in the real spots when a sniff has them.
-- Farholde Keep = Alliance only, Rog'mar = Horde only, Powderfuse Port and Turner's Logging Camp = both.
-- Safe to run again. world_safe_locs 1101200-1101203, creature guids 47220000-47220003.
DELETE FROM `world_safe_locs` WHERE `ID` BETWEEN 1101200 AND 1101203;
INSERT INTO `world_safe_locs` (`ID`,`MapID`,`LocX`,`LocY`,`LocZ`,`Facing`,`Comment`) VALUES
(1101200,0,-9008.5,-5063.6,39.8,3.14,'Forever: Riverglades, Farholde Keep GY (ESTIMATED)'),
(1101201,0,-7766.9,-4801.7,20.1,3.14,'Forever: Riverglades, Rog''mar GY (ESTIMATED)'),
(1101202,0,-8180.8,-5606.8,3.8,3.14,'Forever: Riverglades, Powderfuse Port GY (ESTIMATED)'),
(1101203,0,-8445.9,-3889.9,21.6,3.14,'Forever: Riverglades, Turner''s Logging Camp GY (ESTIMATED)');
DELETE FROM `graveyard_zone` WHERE `ID` BETWEEN 1101200 AND 1101203;
INSERT INTO `graveyard_zone` (`ID`,`GhostZone`,`Comment`) VALUES
(1101200,16591,'Riverglades - Farholde Keep (Alliance)'),
(1101201,16591,'Riverglades - Rog''mar (Horde)'),
(1101202,16591,'Riverglades - Powderfuse Port'),
(1101203,16591,'Riverglades - Turner''s Logging Camp');
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId`=27 AND `SourceGroup`=16591 AND `SourceEntry` BETWEEN 1101200 AND 1101203;
INSERT INTO `conditions` (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,`ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`) VALUES
(27,16591,1101200,0,0,6,0,469,0,0,0,0,0,'','Riverglades Farholde Keep graveyard: Alliance only'),
(27,16591,1101201,0,0,6,0,67,0,0,0,0,0,'','Riverglades Rog''mar graveyard: Horde only');
DELETE FROM `creature` WHERE `guid` BETWEEN 47220000 AND 47220003;
INSERT INTO `creature` (`guid`,`id`,`map`,`zoneId`,`areaId`,`spawnDifficulties`,`phaseUseFlags`,`PhaseId`,`PhaseGroup`,`terrainSwapMap`,`modelid`,`equipment_id`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`currentwaypoint`,`curHealthPct`,`MovementType`,`npcflag`,`unit_flags`,`unit_flags2`,`unit_flags3`,`ScriptName`,`StringId`,`VerifiedBuild`) VALUES
(47220000,6491,0,0,0,'0',0,0,0,-1,0,0,-9005.5,-5060.6,39.8,3.93,25,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0),
(47220001,6491,0,0,0,'0',0,0,0,-1,0,0,-7763.9,-4798.7,20.1,3.93,25,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0),
(47220002,6491,0,0,0,'0',0,0,0,-1,0,0,-8177.8,-5603.8,3.8,3.93,25,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0),
(47220003,6491,0,0,0,'0',0,0,0,-1,0,0,-8442.9,-3886.9,21.6,3.93,25,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0);
