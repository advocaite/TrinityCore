-- Stratholme: Baron Rivendare no longer evades the moment he is pulled (fix found by SIN).
-- VMaNGOS event 1044004 (migration 20230321213656) is "evade if pulled outside of room": a 1 ms combat timer that evades only
-- while VMaNGOS condition 10440 holds (type 54, within 4 yd of 4032 -3368 115 at the room's door; INFERRED: flag 2 makes it
-- the target). The converter dropped the condition, so the imported row (10440 id 5, SMART_ACTION_EVADE) evaded on every pull
-- and the Baron could not be fought.
-- TrinityCore has no "near a position" condition for smart events, so the row goes; the door rule needs a room boundary in the
-- Stratholme instance script (not done yet). Safe to run again.
DELETE FROM `smart_scripts` WHERE `entryorguid` = 10440 AND `source_type` = 0 AND `id` = 5 AND `event_type` = 0 AND `action_type` = 24;
