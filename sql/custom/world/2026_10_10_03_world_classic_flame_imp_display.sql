-- Molten Core Flame Imp (11669, 50 spawns) uses CreatureDisplayID 12190, which the 70291 client cannot draw (`.morph 12190`
-- shows the blue-white checkered box). Move only the Flame Imp to display 4449 (the plain imp model, confirmed with `.morph 4449`).
-- Other 12190 users are left alone. Safe to run again.
UPDATE `creature_template_model` SET `CreatureDisplayID`=4449 WHERE `CreatureID`=11669 AND `CreatureDisplayID`=12190;
-- Only if 4449 has no creature_model_info row: copy the size values of the old display.
INSERT INTO `creature_model_info` (`DisplayID`,`BoundingRadius`,`CombatReach`,`VerifiedBuild`)
SELECT 4449,`BoundingRadius`,`CombatReach`,`VerifiedBuild` FROM `creature_model_info` WHERE `DisplayID`=12190
AND NOT EXISTS (SELECT 1 FROM (SELECT `DisplayID` FROM `creature_model_info` WHERE `DisplayID`=4449) x);
