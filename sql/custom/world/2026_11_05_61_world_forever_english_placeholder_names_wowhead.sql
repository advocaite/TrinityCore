-- Four spawned NPCs/objects showed a placeholder or empty English name: Brother Milius and Sulhasa had "[UNUSED]" in front
-- of their name, and two campfires had no name at all. Names from Wowhead (Forever, English), fetched by the gap-fill tool.
-- Each row is changed only while it still has the old name. Safe to run again.
SET NAMES utf8mb4;
UPDATE `creature_template` SET `name` = 'Brother Milius' WHERE `entry` = 150 AND `name` = '[UNUSED] Brother Milius';
UPDATE `creature_template` SET `name` = 'Sulhasa' WHERE `entry` = 14242 AND `name` = '[UNUSED] Sulhasa';
UPDATE `gameobject_template` SET `name` = 'Campfire' WHERE `entry` IN (56809, 56810) AND (`name` IS NULL OR `name` = '');
