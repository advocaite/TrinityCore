-- Classic 1.60: Relentless Strikes (14179). The client aura only names the energy dummy 14181; nothing restored the energy.
-- C++: Custom/Classic/world/talents/classic_rogue_abilities.cpp (classic_spell_rog_relentless_strikes). Safe to run again.
-- spell_proc: melee abilities done by the rogue, hit phase; the script keeps only finishing moves (combo points spent) and rolls the chance.
DELETE FROM `spell_script_names` WHERE `spell_id` = 14179 AND `ScriptName` = 'classic_spell_rog_relentless_strikes';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES (14179, 'classic_spell_rog_relentless_strikes');
DELETE FROM `spell_proc` WHERE `SpellId` = 14179;
INSERT INTO `spell_proc` (`SpellId`,`SchoolMask`,`SpellFamilyName`,`SpellFamilyMask0`,`SpellFamilyMask1`,`SpellFamilyMask2`,`SpellFamilyMask3`,`ProcFlags`,`ProcFlags2`,`SpellTypeMask`,`SpellPhaseMask`,`HitMask`,`AttributesMask`,`DisableEffectsMask`,`ProcsPerMinute`,`Chance`,`Cooldown`,`Charges`) VALUES
(14179,0,0,0,0,0,0,16,0,0,2,0,0,0,0,100,0,0);
