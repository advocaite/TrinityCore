-- Classic 1.60: The Defias Brotherhood (155) now completes when The Defias Traitor (467) reaches Moonbrook.
-- Player report 2026-11-13 (bug-report forum, voldemargreed): the escort reaches the destination but the quest never completes.
-- 1) The quest-accept action list started the escort path with the traitor himself as the escort target, so SmartAI never
--    stored the player and had nobody to give the quest to at the end of the path. The player who took the quest is the target now.
-- 2) At node 94 the traitor despawned 4 seconds into his 10 second stop, before the last node (95), so the path never ended.
--    The despawn is taken out: he finishes the path, and the escort start (despawn time 5000) despawns him after it ends.
-- He still walks the whole way: SmartAI paths take walk or run from the path, not from the converted VMaNGOS "set run" step;
-- that is a core follow-up for all converted escorts. Safe to run again.
UPDATE `smart_scripts` SET `target_type` = 7, `action_param5` = 5000
  WHERE `entryorguid` = 950000004 AND `source_type` = 9 AND `id` = 3 AND `action_type` = 53 AND `action_param2` = 810004670;
DELETE FROM `smart_scripts` WHERE `entryorguid` = 950001111 AND `source_type` = 9 AND `id` = 1 AND `action_type` = 41;
