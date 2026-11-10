-- Classic 1.60: High Priest Thekal, Zealot Lor'Khan and Zealot Zath run from classic scripts
-- (classic_instance_zulgurub.cpp) instead of the SmartAI conversion of the VMaNGOS EventAI, which healed each of the three
-- to full every 10 s, never resurrected anyone and gave Thekal no tiger phase. The scripts follow
-- VMaNGOS sql/migrations/20240408140703_world.sql (feign death at 1%, Resurrection every 10 s, tiger at all three down).
UPDATE `creature_template` SET `AIName` = '', `ScriptName` = 'classic_boss_thekal' WHERE `entry` = 14509;
UPDATE `creature_template` SET `AIName` = '', `ScriptName` = 'classic_npc_zealot_lorkhan' WHERE `entry` = 11347;
UPDATE `creature_template` SET `AIName` = '', `ScriptName` = 'classic_npc_zealot_zath' WHERE `entry` = 11348;
DELETE FROM `smart_scripts` WHERE `entryorguid` IN (14509, 11347, 11348) AND `source_type` = 0;
-- their timed action lists (feign death, resurrection, tiger phase), used by nothing else
DELETE FROM `smart_scripts` WHERE `entryorguid` IN (950001272, 950001273, 950001274, 950001275, 950001296, 950001297, 950001298) AND `source_type` = 9;
