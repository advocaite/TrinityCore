-- Classic 1.60: Improved Starfire (16850). Effect 1 is a proc trigger spell (stun 16922) with a proc chance of 100 and the talent rank in the amount;
-- the script rolls the rank chance (C++: Custom/Classic/world/talents/classic_talents_warlock_druid.cpp, classic_spell_dru_improved_starfire).
-- spell_proc: Starfire (druid family flag 0x4) damage hits. Safe to run again.
DELETE FROM `spell_script_names` WHERE `spell_id` = 16850 AND `ScriptName` = 'classic_spell_dru_improved_starfire';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES (16850, 'classic_spell_dru_improved_starfire');
DELETE FROM `spell_proc` WHERE `SpellId` = 16850;
INSERT INTO `spell_proc` (`SpellId`,`SchoolMask`,`SpellFamilyName`,`SpellFamilyMask0`,`SpellFamilyMask1`,`SpellFamilyMask2`,`SpellFamilyMask3`,`ProcFlags`,`ProcFlags2`,`SpellTypeMask`,`SpellPhaseMask`,`HitMask`,`AttributesMask`,`DisableEffectsMask`,`ProcsPerMinute`,`Chance`,`Cooldown`,`Charges`) VALUES
(16850,0,7,4,0,0,0,0x10000,0,1,2,0,0,0,0,100,0,0);
