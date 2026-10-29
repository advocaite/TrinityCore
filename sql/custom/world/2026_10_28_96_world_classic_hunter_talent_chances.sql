-- Classic 1.60: hunter talents whose chance is their rank value proc at that chance, from what their tooltips name
-- (classic_talents_warrior_rogue_hunter.cpp). Their client proc chance was 100%, from any melee or ranged attack:
--   19228 Improved Wing Clip: Wing Clip (family flag 0x40)
--   19407 Improved Concussive Shot: Concussive Shot (0x200); its stun 19410 has the Self Only range: SpellMgr.cpp gives it any range
--   440529 Resourcefulness (effect 1): critical strikes of melee and ranged abilities
-- Safe to run again.
DELETE FROM `spell_script_names` WHERE `spell_id` IN (19228, 19407, 440529) AND `ScriptName` IN ('classic_spell_hun_talent_chance', 'classic_spell_hun_resourcefulness');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(19228, 'classic_spell_hun_talent_chance'),
(19407, 'classic_spell_hun_talent_chance'),
(440529, 'classic_spell_hun_resourcefulness');

DELETE FROM `spell_proc` WHERE `SpellId` IN (19228, 19407, 440529);
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `SpellFamilyMask3`, `ProcFlags`, `ProcFlags2`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(19228, 0, 9, 0x40, 0, 0, 0, 0x10, 0, 0x1, 0x2, 0, 0, 0, 0, 100, 0, 0),
(19407, 0, 9, 0x200, 0, 0, 0, 0x100, 0, 0x7, 0x2, 0, 0, 0, 0, 100, 0, 0),
(440529, 0, 0, 0, 0, 0, 0, 0x110, 0, 0x1, 0x2, 0x2, 0, 0x1, 0, 100, 0, 0);
