-- Classic 1.60: Blade Flurry (13877). The client spell is only the haste aura; the extra strike on a second enemy was never cast.
-- The retail script bound to it needs the retail strike spell 22482 (not in the client data) and never ran.
-- C++: Custom/Classic/world/talents/classic_rogue_abilities.cpp (classic_spell_rog_blade_flurry). Safe to run again.
-- spell_proc: melee swing (0x4) and melee ability (0x10) done by the rogue, hit phase, every hit.
DELETE FROM `spell_script_names` WHERE (`spell_id` = 13877 AND `ScriptName` IN ('spell_rog_blade_flurry', 'classic_spell_rog_blade_flurry'));
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES (13877, 'classic_spell_rog_blade_flurry');
DELETE FROM `spell_proc` WHERE `SpellId` = 13877;
INSERT INTO `spell_proc` (`SpellId`,`SchoolMask`,`SpellFamilyName`,`SpellFamilyMask0`,`SpellFamilyMask1`,`SpellFamilyMask2`,`SpellFamilyMask3`,`ProcFlags`,`ProcFlags2`,`SpellTypeMask`,`SpellPhaseMask`,`HitMask`,`AttributesMask`,`DisableEffectsMask`,`ProcsPerMinute`,`Chance`,`Cooldown`,`Charges`) VALUES
(13877,0,0,0,0,0,0,20,0,0,2,0,0,0,0,100,0,0);
