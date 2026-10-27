-- Warlock "The Binding" quests (1471/1504/1689 voidwalker, 1474/1513/1739 succubus, 1795 felhunter): the summoned
-- demon you must slay spawns friendly (faction 35) and cannot be attacked; it must spawn hostile and attack its summoner.
-- Evidence: creature_template 5676/5677/6268 faction 35 (friendly to all); 272 other demons use 90 (Demon, hostile);
-- 6268's own script (626800 id 6 "Start Attacking Closest Player") already fails on faction 35. No static spawns.
-- Rollback: UPDATE creature_template SET faction=35 WHERE entry IN (5676,5677,6268);
--           DELETE FROM smart_scripts WHERE source_type=0 AND ((entryorguid=5676 AND id=4) OR (entryorguid=5677 AND id=2));
UPDATE `creature_template` SET `faction`=90 WHERE `entry` IN (5676,5677,6268);

DELETE FROM `smart_scripts` WHERE `source_type`=0 AND ((`entryorguid`=5676 AND `id`=4) OR (`entryorguid`=5677 AND `id`=2));
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`action_type`,`target_type`,`comment`) VALUES
(5676,0,4,0,54,0,100,0,49,7,'Summoned Voidwalker - On Just Summoned - Start Attacking Summoner'),
(5677,0,2,0,54,0,100,0,49,7,'Summoned Succubus - On Just Summoned - Start Attacking Summoner');
