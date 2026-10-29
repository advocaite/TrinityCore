-- Classic 1.60: Swift Judgement (paladin talent 1310994) finishes Judgement's cooldown (classic_talents_priest_paladin.cpp), and the next
-- Judgement uses it up (cast phase, its spell modifier; Judgement counts as a helpful or a harmful spell). It had no proc row, so
-- Judgement stayed free until the next death.
-- Safe to run again.
DELETE FROM `spell_script_names` WHERE `spell_id` = 1310994 AND `ScriptName` = 'classic_spell_pal_swift_judgement';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(1310994, 'classic_spell_pal_swift_judgement');

DELETE FROM `spell_proc` WHERE `SpellId` = 1310994;
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `SpellFamilyMask3`, `ProcFlags`, `ProcFlags2`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(1310994, 0, 10, 0x800000, 0, 0, 0, 0x14000, 0, 0x7, 0x1, 0, 0x8, 0, 0, 100, 0, 0);
