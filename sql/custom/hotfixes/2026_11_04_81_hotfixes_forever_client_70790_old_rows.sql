-- WoW Forever client 1.60.1.70790 (Battle.net, 4 November 2026 18:49 UK) changed records that old hotfix rows from the 70124 sniff
-- import still override, so a 70790 player would keep the old version:
--   Raging Blows (warrior talent 1310315): 70790 makes it Improved Whirlwind (name, tooltip and effect 1345686).
--   Booming Voice (12321): effect 1360666, 70790 adds a second spell class mask.
--   Talent tree node positions 105928 and 113569 (70790 moved them).
-- Dropping these rows lets the server and client use the 70790 game data. Safe to run again.
DELETE FROM `hotfix_data` WHERE (`TableHash`,`RecordId`) IN ((1187407512,1310315),(3776013982,1310315),(4030871717,1345686),(4030871717,1360666),(3779276131,105928),(3779276131,113569)) AND `VerifiedBuild` = 70124;
DELETE FROM `spell_name` WHERE `ID` = 1310315 AND `VerifiedBuild` = 70124;
DELETE FROM `hotfix_blob` WHERE `TableHash` = 3776013982 AND `RecordId` = 1310315 AND `VerifiedBuild` = 70124;
DELETE FROM `spell_effect` WHERE `ID` IN (1345686, 1360666) AND `VerifiedBuild` = 70124;
DELETE FROM `trait_node` WHERE `ID` IN (105928, 113569) AND `VerifiedBuild` = 70124;
