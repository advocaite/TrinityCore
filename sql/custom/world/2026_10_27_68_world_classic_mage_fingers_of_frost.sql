-- Classic 1.60 (WoW Forever) mage talent Fingers of Frost (400647): procs from Chill effects only, once per chill
-- (src/server/scripts/Custom/Classic/world/talents/classic_talents_mage_shaman.cpp)

DELETE FROM `spell_proc` WHERE `SpellId` = 400647;
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `SpellFamilyMask3`, `ProcFlags`, `ProcFlags2`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(400647, 0, 3, 0x100000, 0, 0, 0, 0x10000, 0, 7, 2, 0, 0, 0, 0, 0, 0, 0); -- Fingers of Frost: the mage's Chill effects (the script rolls the chance)
