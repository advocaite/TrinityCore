-- Classic 1.60: vanilla weapons for seven creatures that had no creature_equip_template row and spawned empty-handed
-- (VMaNGOS 1.12 creature_equip_template, highest patch row <= 10). All items exist in the 70532 client. Safe to run again.
DELETE FROM `creature_equip_template` WHERE `CreatureID` IN (1896,3529,3531,3533,4811,5082,6009) AND `ID` = 1;
INSERT INTO `creature_equip_template` (`CreatureID`,`ID`,`ItemID1`,`AppearanceModID1`,`ItemVisual1`,`ItemID2`,`AppearanceModID2`,`ItemVisual2`,`ItemID3`,`AppearanceModID3`,`ItemVisual3`,`VerifiedBuild`) VALUES
(1896,1,5303,0,0,0,0,0,0,0,0,0), -- Moonrage Elder: staff
(3529,1,1903,0,0,0,0,0,0,0,0,0), -- Moonrage Armorer: hammer
(3531,1,5280,0,0,0,0,0,0,0,0,0), -- Moonrage Tailor: dagger
(3533,1,5284,0,0,0,0,0,0,0,0,0), -- Moonrage Leatherworker: dagger
(4811,1,6231,0,0,0,0,0,0,0,0,0), -- Twilight Aquamancer: wand
(5082,1,2705,0,0,0,0,0,0,0,0,0), -- Vincent Hyal: tankard
(6009,1,6231,0,0,0,0,0,0,0,0,0); -- Shadowsworn Dreadweaver: wand
UPDATE `creature` SET `equipment_id` = 1 WHERE `id` IN (1896,3529,3531,3533,4811,5082,6009) AND `equipment_id` = 0;
