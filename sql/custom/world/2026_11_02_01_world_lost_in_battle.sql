-- Lost in Battle (4921): the Beaten Corpse (10668) shows gossip menu 2881, but its quest credit script waited for an option of
-- menu 2871, so "I inspect the body further." never gave credit (KailON, 2026-11-02). Safe to run again.
UPDATE `smart_scripts` SET `event_param1` = 2881 WHERE `entryorguid` = 10668 AND `source_type` = 0 AND `id` = 0 AND `event_type` = 62;
