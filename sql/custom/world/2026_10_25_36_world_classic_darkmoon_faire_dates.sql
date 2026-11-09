-- Classic 1.60 (WoW Forever): the Darkmoon Faire never starts (server audit 2026-10-24, finding 13).
--
-- game_event 4 / 5 (the Faire in Elwynn / Mulgore) and 23 / 24 (the buildings, up three days earlier) have NULL start times and
-- holidayStage 0, so GameEventMgr neither reads a date from the holiday table nor starts them: 1,353 creature and 10,563 object
-- event rows wait on them forever. VMaNGOS has the same empty dates (its Faire is scripted elsewhere), so the schedule below is
-- chosen, not sniffed: the vanilla rhythm of one Faire a month alternating between the two sites, open Monday to Sunday with the
-- setup from the Friday before. Elwynn first for launch week (2026-11-02), Mulgore from 2026-12-07, then every 60 days each.
-- occurence and length are minutes.

UPDATE `game_event` SET `start_time` = '2026-11-02 00:00:00', `end_time` = '2037-12-31 23:59:59', `occurence` = 86400, `length` = 10080 WHERE `eventEntry` = 4;
UPDATE `game_event` SET `start_time` = '2026-10-30 00:00:00', `end_time` = '2037-12-31 23:59:59', `occurence` = 86400, `length` = 14400 WHERE `eventEntry` = 23;
UPDATE `game_event` SET `start_time` = '2026-12-07 00:00:00', `end_time` = '2037-12-31 23:59:59', `occurence` = 86400, `length` = 10080 WHERE `eventEntry` = 5;
UPDATE `game_event` SET `start_time` = '2026-12-04 00:00:00', `end_time` = '2037-12-31 23:59:59', `occurence` = 86400, `length` = 14400 WHERE `eventEntry` = 24;
