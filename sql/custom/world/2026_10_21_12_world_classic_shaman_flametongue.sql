-- Classic 1.60: Flametongue Weapon and Flametongue Totem procs. The client spells are a dummy effect nothing handled; the script casts the
-- Flametongue Attack spell 10444 (behaviour from vMaNGOS spell_shaman_flametongue_proc_dummy). Safe to run again.
-- C++: Custom/Classic/world/talents/classic_talents_mage_shaman.cpp (classic_spell_sha_flametongue_proc).
DELETE FROM `spell_script_names` WHERE `ScriptName` = 'classic_spell_sha_flametongue_proc';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(8026, 'classic_spell_sha_flametongue_proc'),
(8028, 'classic_spell_sha_flametongue_proc'),
(8029, 'classic_spell_sha_flametongue_proc'),
(10445, 'classic_spell_sha_flametongue_proc'),
(16343, 'classic_spell_sha_flametongue_proc'),
(16344, 'classic_spell_sha_flametongue_proc'),
(8248, 'classic_spell_sha_flametongue_proc'),
(8253, 'classic_spell_sha_flametongue_proc'),
(10523, 'classic_spell_sha_flametongue_proc'),
(16389, 'classic_spell_sha_flametongue_proc');
