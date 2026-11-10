-- Grund and Gozwin (97277, Coldridge Valley, level 6, from level 4): its giver could not be found.
-- Forever reuses two unused vanilla templates for the camp, 2756 (UNUSED Grund Drokda) and 6046 ([UNUSED] Gozwin Vilesprocket).
-- Our world has the retail rows for both entries, so the quest giver showed as "Chao-Ju" and Gozwin had only a retail model the
-- client does not have (5259), so he was a white box next to him. Wowhead Forever: Grund Drokda and Gozwin Halfsprocket, level 5,
-- friendly to the Alliance, Dun Morogh. Report: sicko88 (RageZone, 2026-10-27), "the book and snow leopard quest doesn't exist".
-- Models: 3428 (set by 2026_10_25_30) for Grund, 4775 for Gozwin, the vanilla models of both templates; both are in the 70634 client.
UPDATE `creature_template` SET `name`='Grund Drokda', `femaleName`=NULL, `subname`=NULL, `faction`=55, `npcflag`=(`npcflag` | 2) WHERE `entry`=2756;
UPDATE `creature_template` SET `name`='Gozwin Halfsprocket', `femaleName`=NULL, `subname`=NULL, `faction`=64 WHERE `entry`=6046;
DELETE FROM `creature_template_model` WHERE `CreatureID`=6046;
INSERT INTO `creature_template_model` (`CreatureID`,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`) VALUES
(6046,0,4775,1,1,70634);
