-- Classic 1.60: Scarlet Monastery Cathedral, the Ashbringer event never started.
--
-- VMaNGOS reacts to AB Effect 000 (28441, the pulse of a worn Corrupted Ashbringer) hitting Scarlet Commander Mograine (3976) in its
-- instance hook OnCreatureSpellHit: Mograine kneels, the Scarlets in the cathedral turn friendly and Fairbanks' spirit walks in. The
-- port kept the whole sequence in classic_instance_scarlet_monastery (SetGuidData CLASSIC_SM_DATA_ASHBRINGER_MOGRAINE_HIT = 100,
-- EVENT_SM_KNEEL onwards) but nothing called it: Mograine runs SmartAI and his rows had no spell-hit event for 28441. The other
-- Scarlet mobs already react to 28441 in their own rows, and Fairbanks in his script, so only the trigger was missing.
-- One row: on spell hit 28441, set instance guid data 100 to the caster (the instance takes it only from a player, once).
DELETE FROM `smart_scripts` WHERE `entryorguid` = 3976 AND `source_type` = 0 AND `id` = 27;
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`comment`) VALUES
(3976,0,27,0,8,0,100,0,28441,0,0,0,35,100,0,0,7,0,0,0,'Scarlet Commander Mograine - On Spellhit AB Effect 000 - Set Instance Data64 Ashbringer wielder (VMaNGOS OnCreatureSpellHit)');
