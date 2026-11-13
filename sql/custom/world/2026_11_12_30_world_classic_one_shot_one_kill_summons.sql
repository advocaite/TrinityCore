-- One Shot. One Kill. (5713) did not complete after the waves (player report 2026-11-13, Discord "bugs 2" item 6). The Blackwood
-- Trackers and Marosh the Devious were summoned with a 60 s despawn that runs even in combat (TEMPSUMMON_TIMED_DESPAWN, 3).
-- Marosh needs most of that minute to walk up to Sentinel Aynasha, so he vanished during the fight, never died, and never sent
-- her the success signal. VMaNGOS summons all six with despawn type 1 (quest_start_scripts 5713, dataint4 = 1, migration
-- 20210317142213): the 60 s only count while the creature is out of combat, and it goes when it dies. TrinityCore's
-- TEMPSUMMON_TIMED_OR_DEAD_DESPAWN (1) is the same.
-- Safe to run again.

UPDATE `smart_scripts` SET `action_param2` = 1
WHERE `entryorguid` = 950000245 AND `source_type` = 9 AND `id` BETWEEN 5 AND 10 AND `action_type` = 12 AND `action_param1` IN (11713,11714);
