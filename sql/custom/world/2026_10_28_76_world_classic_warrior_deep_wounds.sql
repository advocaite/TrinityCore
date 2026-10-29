-- Classic 1.60: Deep Wounds (warrior talent 12834) makes melee critical strikes bleed the target with 412609
-- (classic_talents_warrior_rogue_hunter.cpp). Its client proc data has no hit mask: only critical strikes. Safe to run again.
DELETE FROM `spell_script_names` WHERE `spell_id` IN (12834, 412609) AND `ScriptName` IN ('classic_spell_warr_deep_wounds', 'classic_spell_warr_deep_wound_bleed');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(12834, 'classic_spell_warr_deep_wounds'),
(412609, 'classic_spell_warr_deep_wound_bleed');

DELETE FROM `spell_proc` WHERE `SpellId` = 12834;
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `SpellFamilyMask3`, `ProcFlags`, `ProcFlags2`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(12834, 0, 0, 0, 0, 0, 0, 0x14, 0, 1, 2, 0x2, 0, 0, 0, 100, 0, 0);
