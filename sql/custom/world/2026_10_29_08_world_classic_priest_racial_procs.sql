-- Classic 1.60: Touch of Weakness (2652, 19261, 19262, 19264, 19265, 19266) and Shadowguard (18137, 19308, 19309, 19310, 19311, 19312)
-- cast their rank's effect spell at the attacker (classic_talents_priest_paladin.cpp). Touch of Weakness is used by the next melee attack:
-- the client data gives it no charge, so the row gives it one. Safe to run again.
DELETE FROM `spell_script_names` WHERE `spell_id` IN (2652, 19261, 19262, 19264, 19265, 19266, 18137, 19308, 19309, 19310, 19311, 19312) AND `ScriptName` = 'classic_spell_pri_racial_proc';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(2652, 'classic_spell_pri_racial_proc'),
(19261, 'classic_spell_pri_racial_proc'),
(19262, 'classic_spell_pri_racial_proc'),
(19264, 'classic_spell_pri_racial_proc'),
(19265, 'classic_spell_pri_racial_proc'),
(19266, 'classic_spell_pri_racial_proc'),
(18137, 'classic_spell_pri_racial_proc'),
(19308, 'classic_spell_pri_racial_proc'),
(19309, 'classic_spell_pri_racial_proc'),
(19310, 'classic_spell_pri_racial_proc'),
(19311, 'classic_spell_pri_racial_proc'),
(19312, 'classic_spell_pri_racial_proc');

DELETE FROM `spell_proc` WHERE `SpellId` IN (2652, 19261, 19262, 19264, 19265, 19266);
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `SpellFamilyMask3`, `ProcFlags`, `ProcFlags2`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(2652, 0, 0, 0, 0, 0, 0, 0x28, 0, 0, 0, 0, 0, 0, 0, 100, 0, 1),
(19261, 0, 0, 0, 0, 0, 0, 0x28, 0, 0, 0, 0, 0, 0, 0, 100, 0, 1),
(19262, 0, 0, 0, 0, 0, 0, 0x28, 0, 0, 0, 0, 0, 0, 0, 100, 0, 1),
(19264, 0, 0, 0, 0, 0, 0, 0x28, 0, 0, 0, 0, 0, 0, 0, 100, 0, 1),
(19265, 0, 0, 0, 0, 0, 0, 0x28, 0, 0, 0, 0, 0, 0, 0, 100, 0, 1),
(19266, 0, 0, 0, 0, 0, 0, 0x28, 0, 0, 0, 0, 0, 0, 0, 100, 0, 1);
