-- Classic 1.60 (WoW Forever): Hallow's End check (2026-10-27).
--
-- 1. Dates. game_event 12 starts 2020-10-18 20:00 and repeats every 525600 minutes (365 days), so every leap year pulls it a day
--    earlier: since 2024 it has run 17 Oct 20:00 to 31 Oct 20:00 and ended on the evening of Hallow's End itself. The vanilla and
--    Forever festival runs 18 Oct to 1 Nov. The new start puts 2026 and 2027 back on 18 Oct to 1 Nov 20:00 (it slips again after
--    29 Feb 2028). The festival is on now, so it simply keeps running a day longer.
UPDATE `game_event` SET `start_time` = '2026-10-18 20:00:00' WHERE `eventEntry` = 12;

-- 2. Jessica Chambers (16256, the Light's Hope Chapel innkeeper) shows "Trick or Treat!" (menu 7173 option 0, gated on event 12
--    and no Tricked or Treated aura like every other innkeeper), but the VMaNGOS script conversion gave her no gossip-select handler,
--    so picking the option did nothing. Same two rows as the other 44 innkeepers: close the gossip, cast Trick or Treat at the player.
DELETE FROM `smart_scripts` WHERE `entryorguid` = 16256 AND `source_type` = 0 AND `id` IN (4, 5);
INSERT INTO `smart_scripts` (`entryorguid`, `source_type`, `id`, `link`, `Difficulties`, `event_type`, `event_phase_mask`, `event_chance`, `event_flags`, `event_param1`, `event_param2`, `event_param3`, `event_param4`, `event_param5`, `event_param_string`, `action_type`, `action_param1`, `action_param2`, `action_param3`, `action_param4`, `action_param5`, `action_param6`, `action_param7`, `action_param_string`, `target_type`, `target_param1`, `target_param2`, `target_param3`, `target_param4`, `target_param_string`, `target_x`, `target_y`, `target_z`, `target_o`, `comment`) VALUES
(16256, 0, 4, 5, '', 62, 0, 100, 0, 7173, 0, 0, 0, 0, '', 72, 0, 0, 0, 0, 0, 0, 0, '', 7, 0, 0, 0, 0, '', 0, 0, 0, 0, 'Jessica Chambers - On Gossip Option 0 Selected - Close Gossip'),
(16256, 0, 5, 0, '', 61, 0, 100, 0, 0, 0, 0, 0, 0, '', 11, 24751, 0, 0, 0, 0, 0, 0, '', 7, 0, 0, 0, 0, '', 0, 0, 0, 0, 'Jessica Chambers - Linked - Cast Trick or Treat at the player');
UPDATE `creature_template` SET `AIName` = 'SmartAI' WHERE `entry` = 16256;
