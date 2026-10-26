-- Classic 1.60: Power Word: Shield (17) puts Weakened Soul (6788) on the target for 15 sec, and a target with Weakened Soul cannot be
-- shielded again. The 1.60 data does not apply the debuff, so the shield could be recast on the same target at once.
-- classic_spell_pri_power_word_shield (classic_spell_scripts.cpp) applies it on hit and refuses the cast while it is present.
DELETE FROM `spell_script_names` WHERE `ScriptName` = 'classic_spell_pri_power_word_shield';
INSERT INTO `spell_script_names` (`spell_id`,`ScriptName`) VALUES
(17,'classic_spell_pri_power_word_shield');
