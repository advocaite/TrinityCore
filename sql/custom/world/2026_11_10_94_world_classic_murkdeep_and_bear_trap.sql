-- Two Darkshore quests a player reported (Discord, 2026-11-10):
-- WANTED: Murkdeep! (4740): Murkdeep never came. In vanilla he has no spawn: a player on the quest who walks into the murloc
--   camp on the coast (area trigger 1966, still in the 1.60 client data) calls up three waves of Greymist murlocs and then
--   Murkdeep. That event is C++ (VMaNGOS darkshore.cpp, now classic_npc_murkdeep / classic_at_murloc_camp) and was not ported.
-- Plagued Lands (2118): the Night Elven Bear Trap from Tharnariun's Hope caught nothing. The trap casts Bear Captured in Trap
--   (9439) on the nearest Rabid Thistle Bear (target type "nearby entry"), which needs a conditions row naming the creature;
--   there was none, so the spell found no target and the bear script (classic_npc_rabid_thistle_bear) was never reached.
-- Safe to run again.

UPDATE `creature_template` SET `ScriptName` = 'classic_npc_murkdeep' WHERE `entry` = 10323;
DELETE FROM `areatrigger_scripts` WHERE `entry` = 1966;
INSERT INTO `areatrigger_scripts` (`entry`,`ScriptName`) VALUES (1966,'classic_at_murloc_camp');

DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId` = 13 AND `SourceEntry` = 9439;
INSERT INTO `conditions` (`SourceTypeOrReferenceId`, `SourceGroup`, `SourceEntry`, `SourceId`, `ElseGroup`, `ConditionTypeOrReference`, `ConditionTarget`, `ConditionValue1`, `ConditionValue2`, `ConditionValue3`, `NegativeCondition`, `ErrorType`, `ErrorTextId`, `ScriptName`, `Comment`) VALUES
(13, 1, 9439, 0, 0, 31, 0, 3, 2164, 0, 0, 0, 0, '', 'Bear Captured in Trap - target Rabid Thistle Bear');
