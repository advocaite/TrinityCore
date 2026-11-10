-- Classic 1.60: the four Dragons of Nightmare (Ysondre, Lethon, Emeriss, Taerar) are spawns of game event 66, which was a world event
-- (state 1, started only by GM command), so they never appeared. VMaNGOS keeps the event on and turns it off only between rounds; here
-- the event is always on and the round timer is the spawns' respawn time, driven by classic_world_event_dragons_of_nightmare
-- (all four stay dead until the last one dies, then all four return together 4-7 days later). occurence = length = always active.
UPDATE `game_event` SET `world_event` = 0, `start_time` = '2000-01-01 00:00:00', `end_time` = '2037-12-31 23:59:59',
    `occurence` = 525600, `length` = 525600 WHERE `eventEntry` = 66;
