-- Classic 1.60: Expose Prey (hunter talent 1310532) gives Mongoose Bite activations from attacks on targets with the hunter's Hunter's
-- Mark, at the rank's chance (classic_talents_warrior_rogue_hunter.cpp). The activation 1310726 now gives Mongoose Bite's aura state
-- (SpellInfo.cpp); its client proc data let any melee ability use it up: only Mongoose Bite (family flag 0x2). Safe to run again.
DELETE FROM `spell_script_names` WHERE `spell_id` = 1310532 AND `ScriptName` = 'classic_spell_hun_expose_prey';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(1310532, 'classic_spell_hun_expose_prey');

DELETE FROM `spell_proc` WHERE `SpellId` = 1310726;
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `SpellFamilyMask3`, `ProcFlags`, `ProcFlags2`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(1310726, 0, 9, 0x2, 0, 0, 0, 0x10, 0, 0x7, 0x1, 0, 0, 0, 0, 100, 0, 0);
