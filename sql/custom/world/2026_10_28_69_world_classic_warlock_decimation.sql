-- Classic 1.60 (WoW Forever) warlock talent Decimation (440870): Shadow Bolt and Searing Pain cast at a target below the health threshold
-- give the Soul Fire buff (440873), and only those get the damage bonus
-- (src/server/scripts/Custom/Classic/world/talents/classic_talents_warlock_druid.cpp)

DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('classic_talent_warl_decimation', 'classic_talent_warl_decimation_cast');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(440870, 'classic_talent_warl_decimation'),
(686, 'classic_talent_warl_decimation_cast'),
(695, 'classic_talent_warl_decimation_cast'),
(705, 'classic_talent_warl_decimation_cast'),
(1088, 'classic_talent_warl_decimation_cast'),
(1106, 'classic_talent_warl_decimation_cast'),
(7641, 'classic_talent_warl_decimation_cast'),
(11659, 'classic_talent_warl_decimation_cast'),
(11660, 'classic_talent_warl_decimation_cast'),
(11661, 'classic_talent_warl_decimation_cast'),
(25307, 'classic_talent_warl_decimation_cast'),
(5676, 'classic_talent_warl_decimation_cast'),
(17919, 'classic_talent_warl_decimation_cast'),
(17920, 'classic_talent_warl_decimation_cast'),
(17921, 'classic_talent_warl_decimation_cast'),
(17922, 'classic_talent_warl_decimation_cast'),
(17923, 'classic_talent_warl_decimation_cast');
