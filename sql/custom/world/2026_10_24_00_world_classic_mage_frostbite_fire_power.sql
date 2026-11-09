-- Classic 1.60 client 70532: two mage talents of the 70532 design that did nothing on the server.
--   Frostbite 11071: a proc trigger of Frostbite 12494 (freeze) with no class mask and the talent chance (5/10/15) as the effect amount;
--     it never fired. classic_spell_mage_frostbite rolls the amount, the row limits it to the mage's Chill spells (family flag 0x100000)
--     that hit.
--   Fire Power 11124: a dummy, "+$s1% Fire damage per your Fire effects on the target, up to 4". classic_spell_mage_fire_power counts the
--     mage's Fire-only auras on the target, bound to the mage's Fire damage spells (direct damage and damage over time). Safe to run again.
DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('classic_spell_mage_frostbite', 'classic_spell_mage_fire_power');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(11071, 'classic_spell_mage_frostbite'),
-- Fireball
(133, 'classic_spell_mage_fire_power'), (143, 'classic_spell_mage_fire_power'), (145, 'classic_spell_mage_fire_power'),
(3140, 'classic_spell_mage_fire_power'), (8400, 'classic_spell_mage_fire_power'), (8401, 'classic_spell_mage_fire_power'),
(8402, 'classic_spell_mage_fire_power'), (10148, 'classic_spell_mage_fire_power'), (10149, 'classic_spell_mage_fire_power'),
(10150, 'classic_spell_mage_fire_power'), (10151, 'classic_spell_mage_fire_power'), (25306, 'classic_spell_mage_fire_power'),
-- Fire Blast
(2136, 'classic_spell_mage_fire_power'), (2137, 'classic_spell_mage_fire_power'), (2138, 'classic_spell_mage_fire_power'),
(8412, 'classic_spell_mage_fire_power'), (8413, 'classic_spell_mage_fire_power'), (10197, 'classic_spell_mage_fire_power'),
(10199, 'classic_spell_mage_fire_power'),
-- Scorch
(2948, 'classic_spell_mage_fire_power'), (8444, 'classic_spell_mage_fire_power'), (8445, 'classic_spell_mage_fire_power'),
(8446, 'classic_spell_mage_fire_power'), (10205, 'classic_spell_mage_fire_power'), (10206, 'classic_spell_mage_fire_power'),
(10207, 'classic_spell_mage_fire_power'),
-- Flamestrike
(2120, 'classic_spell_mage_fire_power'), (2121, 'classic_spell_mage_fire_power'), (8422, 'classic_spell_mage_fire_power'),
(8423, 'classic_spell_mage_fire_power'), (10215, 'classic_spell_mage_fire_power'), (10216, 'classic_spell_mage_fire_power'),
-- Pyroblast
(11366, 'classic_spell_mage_fire_power'), (12505, 'classic_spell_mage_fire_power'), (12522, 'classic_spell_mage_fire_power'),
(12523, 'classic_spell_mage_fire_power'), (12524, 'classic_spell_mage_fire_power'), (12525, 'classic_spell_mage_fire_power'),
(12526, 'classic_spell_mage_fire_power'), (18809, 'classic_spell_mage_fire_power'),
-- Blast Wave
(11113, 'classic_spell_mage_fire_power'), (13018, 'classic_spell_mage_fire_power'), (13019, 'classic_spell_mage_fire_power'),
(13020, 'classic_spell_mage_fire_power'), (13021, 'classic_spell_mage_fire_power'),
-- Living Bomb (damage over time, explosion)
(400613, 'classic_spell_mage_fire_power'), (400614, 'classic_spell_mage_fire_power');

DELETE FROM `spell_proc` WHERE `SpellId` = 11071;
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `SpellFamilyMask3`,
`ProcFlags`, `ProcFlags2`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(11071, 0, 3, 0x100000, 0, 0, 0, 0x10000, 0, 1, 2, 0, 0, 0, 0, 100, 0, 0); -- the mage's Chill spells, damage hits
