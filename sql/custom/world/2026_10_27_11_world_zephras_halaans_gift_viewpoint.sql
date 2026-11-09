-- The Anchors of Zephras (94414): Halaan Hawk-Eye's option makes the player cast Halaan's Gift 1272014 (effect 0 far sight to
-- a DB destination, effect 1 a dummy aura on the caster). With no spell_target_position row the cast failed, so the aura that
-- completes "View the Anchor Pylon" (criteria tree 221135: gain aura 1272014) never came.
-- Viewpoint INFERRED: no sniff has the destination. It sits 25 yards above Halaan (257554, guid 21101633) and looks along the
-- facing the spell itself carries (SpellEffect 1285053 EffectPos_facing 5.7032), which is where the log text puts the pylon
-- "in the distance". Move it once a GM has seen the pylon (GM-TESTS).
DELETE FROM `spell_target_position` WHERE `ID` = 1272014;
INSERT INTO `spell_target_position` (`ID`, `EffectIndex`, `OrderIndex`, `MapID`, `PositionX`, `PositionY`, `PositionZ`, `Orientation`, `VerifiedBuild`) VALUES
(1272014, 0, 0, 2991, 4064.57, 1794.97, 1045.22, 5.7032, 0);
