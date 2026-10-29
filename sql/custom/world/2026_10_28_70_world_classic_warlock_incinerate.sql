-- Classic 1.60 (WoW Forever) warlock talent Incinerate (412758 and its ranks): more damage to a target with the warlock's Immolate
-- (src/server/scripts/Custom/Classic/world/talents/classic_talents_warlock_druid.cpp)

DELETE FROM `spell_script_names` WHERE `ScriptName` = 'classic_talent_warl_incinerate';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(412758, 'classic_talent_warl_incinerate'),
(1293812, 'classic_talent_warl_incinerate'),
(1293813, 'classic_talent_warl_incinerate');
