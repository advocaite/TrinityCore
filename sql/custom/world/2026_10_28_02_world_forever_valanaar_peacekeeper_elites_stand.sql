-- Peacekeeper Elite (252477), the guards who patrol Valanaar (Zephyris), lay on the ground and slid along their paths.
-- 2026_10_02_00 gave them an out of combat self cast of Permanent Feign Death (1288546), taken from the sniff's corpses, the same
-- import bug 2026_10_05_04 already removed from the Valanaar Peacekeepers (253474). 2026_10_27_73 only stopped the recast.
-- Official sniffs 70245-70425 (Azeroth project, Sakhwoow/WoWForever, GPL-2): the Elites stand with Vigilant (1310595 1326064)
-- and no Feign Death; spawn 43000228 already has those auras. Report: KailON (Discord, 2026-10-27). Safe to run again.
DELETE FROM `smart_scripts` WHERE `entryorguid` = 252477 AND `source_type` = 0;
UPDATE `creature_template` SET `AIName` = '' WHERE `entry` = 252477 AND `AIName` = 'SmartAI';
INSERT INTO `creature_template_addon` (`entry`,`auras`) VALUES (252477,'1310595 1326064') ON DUPLICATE KEY UPDATE `auras` = '1310595 1326064', `StandState` = 0;
-- The two patrolling Elites have their own creature_addon (path), which replaces the template's, so they get the auras there too.
UPDATE `creature_addon` SET `auras` = '1310595 1326064' WHERE `auras` IS NULL AND `guid` IN (SELECT `guid` FROM `creature` WHERE `id` = 252477);
