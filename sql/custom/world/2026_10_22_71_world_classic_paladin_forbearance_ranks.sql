-- Classic 1.60 (WoW Forever) paladin Forbearance: Divine Shield, Blessing of Protection and Lay on Hands put Forbearance on the target and are
-- refused on a target that has it. The retail scripts for this do not work on the Classic spell data and are bound to rank 1 only.
-- C++: Custom/Classic/world/talents/classic_talents_priest_paladin.cpp. Safe to run again.
DELETE FROM `spell_script_names` WHERE `ScriptName` = 'classic_spell_pal_forbearance';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(642, 'classic_spell_pal_forbearance'),
(1020, 'classic_spell_pal_forbearance'),
(1022, 'classic_spell_pal_forbearance'),
(5599, 'classic_spell_pal_forbearance'),
(10278, 'classic_spell_pal_forbearance'),
(633, 'classic_spell_pal_forbearance'),
(2800, 'classic_spell_pal_forbearance'),
(10310, 'classic_spell_pal_forbearance');
