-- Classic 1.60: the General Rajaxx wave event of the Ruins of Ahn'Qiraj (VMaNGOS migrations 20221127232342,
-- 20240407052628 and 20240619170801), scripted in classic_instance_ruins_of_ahnqiraj.cpp / classic_ruins_of_ahnqiraj.cpp.
-- Lieutenant General Andorov (15471) and the Kaldorei Elites (15473) get the classic scripts carrying the VMaNGOS spell
-- lists 154710 / 154730 (Strike, Bash, Aura of Command; Mortal Strike, Cleave) and the gossip start of the event.
UPDATE `creature_template` SET `ScriptName` = 'classic_npc_andorov', `AIName` = '' WHERE `entry` = 15471;
UPDATE `creature_template` SET `ScriptName` = 'classic_npc_kaldorei_elite', `AIName` = '' WHERE `entry` = 15473;
DELETE FROM `smart_scripts` WHERE `entryorguid` IN (15471, 15473) AND `source_type` = 0;

-- General Rajaxx (15341): the VMaNGOS spawn script 1534105 (say 11429, react state passive, invincibility) is bound to
-- condition 991 (map 1 only: the Kalimdor spawn of the war effort). In the Ruins (condition 509) he only changes faction,
-- so the SmartAI rows that made him passive and invincible inside the instance are removed; his aggro, death, enrage,
-- kill, evade and spell rows stay.
DELETE FROM `smart_scripts` WHERE `entryorguid` = 15341 AND `source_type` = 0 AND `id` IN (7, 8, 9);
