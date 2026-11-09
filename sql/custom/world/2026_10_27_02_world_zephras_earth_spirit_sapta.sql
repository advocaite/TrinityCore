-- Call of Earth (92467, Skyborne shaman, Zephras Isle): "drink the sapta at the mountain peak" and the Minor Manifestation of Earth
-- (251166) appears to take the quest. In 0.1.19 he had no spawn at all; 2026_10_26_31 added the other Forever server's spawn (on the Wowhead Forever
-- pin, 49.6 24). He was then visible to everyone, so the Earth Sapta (6635, Sapta Sight 8202: see invisibility type 1) did nothing.
-- Give him the elemental spirit invisibility the vanilla Minor Manifestation of Earth (5891) has for the same quest step:
-- Elemental Spirit Invisibility 8203 (invisibility type 1). INFERRED from 5891: no Forever sniff of 251166's auras.
DELETE FROM `creature_template_addon` WHERE `entry`=251166;
INSERT INTO `creature_template_addon` (`entry`,`PathId`,`mount`,`MountCreatureID`,`StandState`,`AnimTier`,`VisFlags`,`SheathState`,`PvPFlags`,`emote`,`aiAnimKit`,`movementAnimKit`,`meleeAnimKit`,`visibilityDistanceType`,`auras`) VALUES
(251166,0,0,0,0,0,0,1,0,0,0,0,0,0,'8203');
