-- WoW Forever boat routes missing from the world (client 70630 TaxiPath/TaxiPathNode, the official sniff of build 70124,
-- and the Forever route list: Stormwind Harbor <-> Auberdine, the Auberdine -> Menethil -> Southshore -> Auberdine loop,
-- Steamwheedle Port <-> Powderfuse Port).
--
--   "Mistbound" 656308 (template from the sniff, 2026_10_01_03), taxi path 11616:
--     Auberdine dock (6548.3, 942.2) <-> Stormwind Harbor, southern-most dock (-8654.5, 1344.4). It had no `transports` row.
--   "The Bravery" 176310 moves from the vanilla Menethil <-> Auberdine path 295 to path 11167, the same route with a
--     stop at Southshore (-1102.6, -554.7). The Menethil and Auberdine stops are unchanged.
--   Steamwheedle Port (Tanaris, -6932.6, -4950.8) <-> Powderfuse Port (Riverglades, -8232.3, -5800.8), taxi path 11391.
--     No sniffed template yet: 900002 is ours, copied from the vanilla boat 176310 (model 3015).
-- Safe to run again.

UPDATE `gameobject_template` SET `Data0` = 11167 WHERE `entry` = 176310 AND `Data0` = 295;

DELETE FROM `gameobject_template` WHERE `entry` = 900002;
DROP TEMPORARY TABLE IF EXISTS `tmp_forever_boat`;
CREATE TEMPORARY TABLE `tmp_forever_boat` AS SELECT * FROM `gameobject_template` WHERE `entry` = 176310;
UPDATE `tmp_forever_boat` SET `entry` = 900002, `name` = 'Boat (Steamwheedle Port - Powderfuse Port)', `Data0` = 11391, `VerifiedBuild` = 0;
INSERT INTO `gameobject_template` SELECT * FROM `tmp_forever_boat`;
DROP TEMPORARY TABLE `tmp_forever_boat`;

DELETE FROM `gameobject_template_addon` WHERE `entry` IN (656308, 900002);
INSERT INTO `gameobject_template_addon` (`entry`, `faction`, `flags`) VALUES (656308, 0, 40), (900002, 0, 40);

DELETE FROM `transports` WHERE `guid` IN (58, 59) OR `entry` IN (656308, 900002);
INSERT INTO `transports` (`guid`, `entry`, `name`, `phaseUseFlags`, `phaseid`, `phasegroup`, `ScriptName`) VALUES
(58, 656308, 'Auberdine, Darkshore and Stormwind Harbor ("Mistbound")', 0, 0, 0, ''),
(59, 900002, 'Steamwheedle Port, Tanaris and Powderfuse Port, Riverglades', 0, 0, 0, '');
UPDATE `transports` SET `name` = 'Menethil Harbor, Wetlands, Southshore, Hillsbrad Foothills and Auberdine, Darkshore ("The Bravery")' WHERE `entry` = 176310;
