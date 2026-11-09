-- Portal To Rohashi Spires (586726, spell 1271396): Confront Lorthuna exists twice (92646 and 93835, one per story line), and
-- the portal's cast condition (2026_10_05_02) only knew 92646, so a player with 93835 stood in the portal and nothing happened
-- (Discord 2026-11-05). The portal now also works while 93835 is in the log. Safe to run again.
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId` = 17 AND `SourceEntry` = 1271396 AND `ElseGroup` = 2;
INSERT INTO `conditions` (`SourceTypeOrReferenceId`, `SourceGroup`, `SourceEntry`, `SourceId`, `ElseGroup`, `ConditionTypeOrReference`, `ConditionTarget`, `ConditionValue1`, `ConditionValue2`, `ConditionValue3`, `ConditionStringValue1`, `NegativeCondition`, `ErrorType`, `ErrorTextId`, `ScriptName`, `Comment`) VALUES
(17, 0, 1271396, 0, 2, 9, 0, 93835, 0, 0, '', 0, 0, 0, '', 'Portal To Rohashi Spires - or the other Confront Lorthuna taken');
