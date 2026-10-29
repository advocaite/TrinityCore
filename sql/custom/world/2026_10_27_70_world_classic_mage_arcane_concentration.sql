-- Classic 1.60 (WoW Forever) mage talent Arcane Concentration (11213): its chance comes from the talent rank, and the Clearcasting
-- it gives (12536) is used up by the next damage spell (src/server/scripts/Custom/Classic/world/talents/classic_talents_mage_shaman.cpp)

DELETE FROM `spell_script_names` WHERE `ScriptName` = 'classic_spell_mage_arcane_concentration';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(11213, 'classic_spell_mage_arcane_concentration');

DELETE FROM `spell_proc` WHERE `SpellId` IN (11213, 12536);
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `SpellFamilyMask3`, `ProcFlags`, `ProcFlags2`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(11213, 0, 3, 0, 0, 0, 0, 0x10000, 0, 1, 2, 0, 0, 0, 0, 0, 0, 0), -- Arcane Concentration: the mage's damage spell hits (the script rolls the chance)
(12536, 0, 3, 0x20c21af7, 0x29000, 0x10040040, 0x84000, 0x10000, 0, 7, 1, 0, 8, 0, 0, 0, 0, 1); -- Clearcasting: used by the next damage spell whose mana cost it removes
