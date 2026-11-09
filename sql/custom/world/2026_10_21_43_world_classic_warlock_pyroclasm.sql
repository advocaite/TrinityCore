-- Classic 1.60 Pyroclasm (18073): the client spell is a dummy (effect 0, the chance) and a proc trigger of 18350, a spell with only a dummy effect
-- that nothing handled, so the stun 18093 was never cast. classic_talent_warl_pyroclasm rolls the amount on Soul Fire and Hellfire (family flag 0x40)
-- and Rain of Fire (0x20) damage; the proc row disables effect 1 (the dummy trigger). Safe to run again.
DELETE FROM `spell_script_names` WHERE `ScriptName` = 'classic_talent_warl_pyroclasm';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(18073, 'classic_talent_warl_pyroclasm');

DELETE FROM `spell_proc` WHERE `SpellId` = 18073;
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `SpellFamilyMask3`,
`ProcFlags`, `ProcFlags2`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(18073, 4, 5, 0x60, 0, 0, 0, 0x50000, 0, 1, 2, 0, 0, 0x2, 0, 100, 0, 0); -- fire damage hits and Hellfire ticks
