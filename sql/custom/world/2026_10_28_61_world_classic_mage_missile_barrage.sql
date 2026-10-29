-- Classic 1.60 (WoW Forever) mage talent Missile Barrage (400588): its chance depends on the spell, and the buff it gives (400589) is
-- used up by the next Arcane Missiles (src/server/scripts/Custom/Classic/world/talents/classic_talents_mage_shaman.cpp)

DELETE FROM `spell_script_names` WHERE `ScriptName` = 'classic_spell_mage_missile_barrage';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(400588, 'classic_spell_mage_missile_barrage');

DELETE FROM `spell_proc` WHERE `SpellId` IN (400588, 400589);
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `SpellFamilyMask3`, `ProcFlags`, `ProcFlags2`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(400588, 0, 3, 0x20200021, 0x9000, 0, 0, 0x10000, 0, 1, 2, 0, 0, 0, 0, 0, 0, 0), -- Missile Barrage: hits of Arcane Blast, Arcane Missiles, Fireball, Frostbolt, Frostfire Bolt and Arcane Wrath (the script rolls the chance)
(400589, 0, 3, 0x800, 0, 0, 0, 0x10000, 0, 7, 4, 0, 8, 0, 0, 100, 0, 1); -- Missile Barrage (buff): used by the next Arcane Missiles it changes, in the finish phase, once the channel has its duration and missile period (no proc chance in the client data)
