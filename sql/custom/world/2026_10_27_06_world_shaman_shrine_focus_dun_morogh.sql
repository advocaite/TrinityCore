-- Earth Sapta (Sapta Sight 8202; the other Saptas 8898-8900 and 9735 too) needs spell focus 63, Shaman Shrine (SpellCastingRequirements,
-- 70532 client). The Shaman Shrine templates 100028 and 101748-101751 are only the visible shrines (type 10); each of their spots also
-- has the focus 63674 (type 8, radius 50). Dun Morogh has no shrine and no focus, yet Teo Hammerstorm's dwarf shamans (Call of Earth
-- 94373-94375) drink the Sapta there: the third step ends at the Minor Manifestation of Earth (5891), which the sniff spawns at
-- -5908.11 576.529 500.644 (guid 21200024, 2026_10_03_00). As at every other Manifestation, the focus stands on that spot.
-- (An earlier version of this file also took a Dun Morogh and two Westfall foci from another project; those rows are removed.)
-- Safe to run again.
DELETE FROM `gameobject` WHERE `guid` IN (7650001,11490001,11490002,21200025);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`zoneId`,`areaId`,`spawnDifficulties`,`phaseUseFlags`,`PhaseId`,`PhaseGroup`,`terrainSwapMap`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`animprogress`,`state`,`ScriptName`,`VerifiedBuild`) VALUES
(21200025,63674,0,0,0,'0',0,0,0,-1,-5908.11,576.529,500.644,0,0,0,0,1,120,255,1,'',0); -- Dun Morogh, Minor Manifestation of Earth
