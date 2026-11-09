-- Classic 1.60: 2026_10_25_36 gave the Darkmoon Faire (events 4 / 5) and its buildings (23 / 24) a monthly schedule, but they kept
-- world_event = 1. The core reads that column as the event state, and 1 is GAMEEVENT_WORLD_INACTIVE: an inactive world event with
-- no prerequisite events starts only by GM command, so the dates were never used. State 0 (normal) makes the clock run them.
UPDATE `game_event` SET `world_event` = 0 WHERE `eventEntry` IN (4, 5, 23, 24);
