-- 1301404: nature damage of the Al'Aketh Spiritcaller (252763, Shrine of Akir on Zephras Isle) has an infinite duration in the client
-- data; the script ends it when the Spiritcaller is gone, dead or out of combat with the player (it also clears auras already stuck
-- on characters at their next tick). Safe to run again.
DELETE FROM `spell_script_names` WHERE `spell_id` = 1301404 AND `ScriptName` = 'classic_spell_alaketh_spiritcaller_dot';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(1301404, 'classic_spell_alaketh_spiritcaller_dot');
