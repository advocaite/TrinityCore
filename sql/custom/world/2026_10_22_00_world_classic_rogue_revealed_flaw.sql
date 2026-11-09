-- Classic 1.60: Revealed Flaw (28814, Bonescythe Armor 8 pieces). SpellMgr turns the client's add-target-trigger effect into a proc trigger; the
-- script rolls 20 % per combo point of an Eviscerate that hit and casts 28815 (+100 % crit for the next Backstab, Sinister Strike or Hemorrhage).
-- C++: Custom/Classic/world/talents/classic_rogue_abilities.cpp (classic_spell_rog_revealed_flaw). Safe to run again.
DELETE FROM `spell_script_names` WHERE `spell_id` = 28814 AND `ScriptName` = 'classic_spell_rog_revealed_flaw';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES (28814, 'classic_spell_rog_revealed_flaw');
DELETE FROM `spell_proc` WHERE `SpellId` = 28814;
INSERT INTO `spell_proc` (`SpellId`,`SchoolMask`,`SpellFamilyName`,`SpellFamilyMask0`,`SpellFamilyMask1`,`SpellFamilyMask2`,`SpellFamilyMask3`,`ProcFlags`,`ProcFlags2`,`SpellTypeMask`,`SpellPhaseMask`,`HitMask`,`AttributesMask`,`DisableEffectsMask`,`ProcsPerMinute`,`Chance`,`Cooldown`,`Charges`) VALUES
(28814,0,0,0,0,0,0,16,0,0,4,0,0,0,0,100,0,0);
