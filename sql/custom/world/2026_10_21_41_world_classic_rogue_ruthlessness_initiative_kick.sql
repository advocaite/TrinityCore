-- Classic 1.60: Ruthlessness (14156), Initiative (13976), Improved Kick (13754). The client auras are proc trigger spells with a proc chance
-- of 100 and the talent rank in the amount; the scripts roll the rank chance (C++: Custom/Classic/world/talents/classic_rogue_abilities.cpp).
-- spell_proc: melee abilities done by the rogue. Ruthlessness in the finish phase (the finisher has spent its combo points by then), the
-- others in the hit phase on the ability's target. Safe to run again.
DELETE FROM `spell_script_names` WHERE `spell_id` IN (14156, 13976, 13754) AND `ScriptName` IN ('classic_spell_rog_ruthlessness', 'classic_spell_rog_initiative', 'classic_spell_rog_improved_kick');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(14156, 'classic_spell_rog_ruthlessness'),
(13976, 'classic_spell_rog_initiative'),
(13754, 'classic_spell_rog_improved_kick');
DELETE FROM `spell_proc` WHERE `SpellId` IN (14156, 13976, 13754);
INSERT INTO `spell_proc` (`SpellId`,`SchoolMask`,`SpellFamilyName`,`SpellFamilyMask0`,`SpellFamilyMask1`,`SpellFamilyMask2`,`SpellFamilyMask3`,`ProcFlags`,`ProcFlags2`,`SpellTypeMask`,`SpellPhaseMask`,`HitMask`,`AttributesMask`,`DisableEffectsMask`,`ProcsPerMinute`,`Chance`,`Cooldown`,`Charges`) VALUES
(14156,0,0,0,0,0,0,16,0,0,4,0,0,0,0,100,0,0),
(13976,0,0,0,0,0,0,16,0,0,2,0,0,0,0,100,0,0),
(13754,0,0,0,0,0,0,16,0,0,2,0,0,0,0,100,0,0);
