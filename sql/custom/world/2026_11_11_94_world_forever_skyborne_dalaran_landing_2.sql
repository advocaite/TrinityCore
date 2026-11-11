-- Dalaran: Skyborne Portal to Dalaran 654168 in the Stormwind Wizard's Sanctum (1278903 -> teleport 1278902) still put players under
-- the ground (Discord, Vokshus 2026-11-11 on 0.1.34, VoldemarGreed 2026-11-09). The 2026_11_08_45 fix only raised the guessed spot
-- (455.13 444.87, 4.7 yards from the Skyborne Portal to Stormwind 631299) by half a yard. No sniff has ever shown anything standing
-- there, and the ground round the portal is not level (the sniffed creatures there stand at z 110.07 to 110.83), so that spot can lie
-- under a rise of the ground.
-- Now the player lands where a creature stood in the official sniff (259084, 2026_10_03_00, at 448.96 439.35 110.829, 7 yards from
-- the portal), half a yard above it, facing the portal.
-- INFERRED: the spot (the Dalaran side of this teleport is not sniffed). Safe to run again.
DELETE FROM `spell_target_position` WHERE `ID` = 1278902 AND `EffectIndex` = 0;
INSERT INTO `spell_target_position` (`ID`,`EffectIndex`,`OrderIndex`,`MapID`,`PositionX`,`PositionY`,`PositionZ`,`Orientation`,`VerifiedBuild`) VALUES
(1278902,0,0,0,448.96,439.35,111.33,1.37,0);
