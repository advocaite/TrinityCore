-- Windsong Standing Stones (Zephras): High Order Apprentices (257521, faction 3673, enemy of the monster group) attacked the
-- Wind Sprites (252481, faction 634) and both died with no player around; the sprites also cast Fly Away! (1253200,
-- invisibility + fear) on every spawn and reset, so they vanished. Both become immune to NPCs; the sprites lose the reset cast.
UPDATE `creature_template` SET `unit_flags` = `unit_flags` | 0x200 WHERE `entry` IN (257521, 252481);   -- UNIT_FLAG_IMMUNE_TO_NPC
DELETE FROM `smart_scripts` WHERE `entryorguid` = 252481 AND `source_type` = 0 AND `id` = 0;
UPDATE `creature_template` SET `AIName` = '' WHERE `entry` = 252481;
