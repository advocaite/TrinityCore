-- Classic 1.60: Reckoning (paladin talent 20177) stores extra attacks from blocks and from non-periodic critical strikes taken, and the next
-- swing uses them (classic_talents_priest_paladin.cpp). The script keeps the proc cooldown for blocks only, as the tooltip says, so the
-- row's cooldown is 1 ms (0 would take the client's 1.5 sec for both). Safe to run again.
DELETE FROM `spell_script_names` WHERE `spell_id` IN (20177, 1326084) AND `ScriptName` IN ('classic_spell_pal_reckoning', 'classic_spell_pal_reckoning_attacks');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(20177, 'classic_spell_pal_reckoning'),
(1326084, 'classic_spell_pal_reckoning_attacks');

DELETE FROM `spell_proc` WHERE `SpellId` = 20177;
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `SpellFamilyMask3`, `ProcFlags`, `ProcFlags2`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(20177, 0, 0, 0, 0, 0, 0, 0x202A8, 0, 0, 0, 0x2042, 0, 0, 0, 100, 1, 0);
