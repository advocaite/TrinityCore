-- Classic 1.60 (WoW Forever): the yearly world events back on their calendar dates (follow-up to 2026_10_27_50, Hallow's End).
--
-- The VMaNGOS start dates (2020 calendar) repeat every 525600 minutes, i.e. 365 days, so each 29 February since pulls an event a
-- day earlier: in 2026 Winter Veil starts 15 Dec, its gifts appear on 24 Dec and New Year's Eve fires on 30 Dec. Moving the base
-- year to 2026 with the VMaNGOS day and hour puts every event back on its date for 2026 and 2027; they slip again after
-- 29 Feb 2028. Lengths and end dates stay. The one change of date: event 41 'July 4th Fireworks Spectacular' was on 4 June in
-- VMaNGOS; it is on 4 July here, as its name and Blizzard's calendar say.
-- Not touched: Lunar Festival and Chinese New Year keep VMaNGOS's fixed date (the real ones follow the moon); events with no
-- start date (Darkmoon Faire has its own schedule, 2026_10_25_36; Noblegarden, Scourge Invasion and the script-started events).
UPDATE `game_event` SET `start_time` = '2026-06-21 20:00:00' WHERE `eventEntry` = 1;   -- Midsummer Fire Festival
UPDATE `game_event` SET `start_time` = '2026-12-16 23:00:00' WHERE `eventEntry` = 2;   -- Feast of Winter Veil
UPDATE `game_event` SET `start_time` = '2026-01-24 22:00:00' WHERE `eventEntry` = 7;   -- Lunar Festival
UPDATE `game_event` SET `start_time` = '2026-02-11 22:00:00' WHERE `eventEntry` = 8;   -- Love is in the Air
UPDATE `game_event` SET `start_time` = '2026-05-01 20:00:00' WHERE `eventEntry` = 10;  -- Children's Week
UPDATE `game_event` SET `start_time` = '2026-09-29 22:00:00' WHERE `eventEntry` = 11;  -- Harvest Festival
UPDATE `game_event` SET `start_time` = '2026-12-25 00:00:00' WHERE `eventEntry` = 21;  -- Feast of Winter Veil: Gifts
UPDATE `game_event` SET `start_time` = '2026-12-31 06:00:00' WHERE `eventEntry` = 34;  -- New Year's Eve
UPDATE `game_event` SET `start_time` = '2026-02-08 06:00:00' WHERE `eventEntry` = 38;  -- Chinese New Year
UPDATE `game_event` SET `start_time` = '2026-07-04 00:00:00', `end_time` = '2037-07-05 02:00:00' WHERE `eventEntry` = 41; -- July 4th Fireworks
UPDATE `game_event` SET `start_time` = '2026-09-30 00:00:00' WHERE `eventEntry` = 42;  -- September 30th Peon Day
UPDATE `game_event` SET `start_time` = '2026-03-20 10:00:00' WHERE `eventEntry` = 45;  -- Fishing: summer period
UPDATE `game_event` SET `start_time` = '2026-09-23 09:00:00' WHERE `eventEntry` = 46;  -- Fishing: winter period
UPDATE `game_event` SET `start_time` = '2026-09-08 04:00:00' WHERE `eventEntry` = 103; -- Event PVP Silithus
