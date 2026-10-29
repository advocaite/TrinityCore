-- Classic 1.60 (WoW Forever) warlock talent Pyroclasm (18073): Soul Fire and the ticks of Rain of Fire and Hellfire can stun
-- (src/server/scripts/Custom/Classic/world/talents/classic_talents_warlock_druid.cpp)

DELETE FROM `spell_script_names` WHERE `ScriptName` = 'classic_talent_warl_pyroclasm';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(18073, 'classic_talent_warl_pyroclasm');

DELETE FROM `spell_proc` WHERE `SpellId` = 18073;
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `SpellFamilyMask3`, `ProcFlags`, `ProcFlags2`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(18073, 0, 5, 0x60, 0x180, 0, 0, 0x50000, 0, 1, 2, 0, 0, 0, 0, 0, 0, 0); -- Pyroclasm: Soul Fire and Hellfire tick hits, Rain of Fire ticks (the script rolls the chance)
