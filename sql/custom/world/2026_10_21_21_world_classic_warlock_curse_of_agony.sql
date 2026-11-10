-- Classic 1.60: Curse of Agony damage ramp (half, full, one and a half per four ticks), classic_spell_warl_curse_of_agony. Safe to run again.
DELETE FROM `spell_script_names` WHERE `ScriptName` = 'classic_spell_warl_curse_of_agony';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(980, 'classic_spell_warl_curse_of_agony'),
(1014, 'classic_spell_warl_curse_of_agony'),
(6217, 'classic_spell_warl_curse_of_agony'),
(11711, 'classic_spell_warl_curse_of_agony'),
(11712, 'classic_spell_warl_curse_of_agony'),
(11713, 'classic_spell_warl_curse_of_agony');
