-- Forever-only creatures that the 2026_10_26_35 NPC port left on a default faction (35 friendly to all, or 7 neutral) where
-- Wowhead Forever shows a different reaction to Alliance / Horde. Only rows with a clear Wowhead reaction and a matching
-- vanilla neighbour or role are changed. Safe to run again.

-- Magram Necrokhan (Desolace): hostile on Wowhead Forever; 133 is the VMaNGOS Magram faction its camp uses.
UPDATE `creature_template` SET `faction` = 133 WHERE `entry` = 253274;

-- Alliance quest givers, friendly to Alliance and hostile to Horde on Wowhead Forever.
-- Essene Villard stands with Comar Villard (123); Berdun Cliffbrew is a dwarf innkeeper (55 Ironforge); Father Tuttle 12.
UPDATE `creature_template` SET `faction` = 123 WHERE `entry` = 271322;
UPDATE `creature_template` SET `faction` = 55 WHERE `entry` = 240482;
UPDATE `creature_template` SET `faction` = 12 WHERE `entry` = 255597;

-- Damin Dawnshadow and Faluris: friendly to both factions on Wowhead Forever, neutral (7) here.
UPDATE `creature_template` SET `faction` = 35 WHERE `entry` IN (274780, 274927);

-- Shore Crawler (Forever copy): neutral on Wowhead Forever, friendly here; 7 as every vanilla crawler.
UPDATE `creature_template` SET `faction` = 7 WHERE `entry` = 256807;
