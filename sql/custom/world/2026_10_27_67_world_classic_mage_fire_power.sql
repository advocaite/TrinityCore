-- Classic 1.60 (WoW Forever) mage talent Fire Power (11124): Fire damage bonus per Fire effect on the target
-- (src/server/scripts/Custom/Classic/world/talents/classic_talents_mage_shaman.cpp)

DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('classic_spell_mage_fire_power', 'classic_spell_mage_fire_power_periodic');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(133, 'classic_spell_mage_fire_power'), -- Fireball
(143, 'classic_spell_mage_fire_power'), -- Fireball
(145, 'classic_spell_mage_fire_power'), -- Fireball
(3140, 'classic_spell_mage_fire_power'), -- Fireball
(8400, 'classic_spell_mage_fire_power'), -- Fireball
(8401, 'classic_spell_mage_fire_power'), -- Fireball
(8402, 'classic_spell_mage_fire_power'), -- Fireball
(10148, 'classic_spell_mage_fire_power'), -- Fireball
(10149, 'classic_spell_mage_fire_power'), -- Fireball
(10150, 'classic_spell_mage_fire_power'), -- Fireball
(10151, 'classic_spell_mage_fire_power'), -- Fireball
(25306, 'classic_spell_mage_fire_power'), -- Fireball
(2136, 'classic_spell_mage_fire_power'), -- Fire Blast
(2137, 'classic_spell_mage_fire_power'), -- Fire Blast
(2138, 'classic_spell_mage_fire_power'), -- Fire Blast
(8412, 'classic_spell_mage_fire_power'), -- Fire Blast
(8413, 'classic_spell_mage_fire_power'), -- Fire Blast
(10197, 'classic_spell_mage_fire_power'), -- Fire Blast
(10199, 'classic_spell_mage_fire_power'), -- Fire Blast
(2948, 'classic_spell_mage_fire_power'), -- Scorch
(8444, 'classic_spell_mage_fire_power'), -- Scorch
(8445, 'classic_spell_mage_fire_power'), -- Scorch
(8446, 'classic_spell_mage_fire_power'), -- Scorch
(10205, 'classic_spell_mage_fire_power'), -- Scorch
(10206, 'classic_spell_mage_fire_power'), -- Scorch
(10207, 'classic_spell_mage_fire_power'), -- Scorch
(11366, 'classic_spell_mage_fire_power'), -- Pyroblast
(12505, 'classic_spell_mage_fire_power'), -- Pyroblast
(12522, 'classic_spell_mage_fire_power'), -- Pyroblast
(12523, 'classic_spell_mage_fire_power'), -- Pyroblast
(12524, 'classic_spell_mage_fire_power'), -- Pyroblast
(12525, 'classic_spell_mage_fire_power'), -- Pyroblast
(12526, 'classic_spell_mage_fire_power'), -- Pyroblast
(18809, 'classic_spell_mage_fire_power'), -- Pyroblast
(2120, 'classic_spell_mage_fire_power'), -- Flamestrike
(2121, 'classic_spell_mage_fire_power'), -- Flamestrike
(8422, 'classic_spell_mage_fire_power'), -- Flamestrike
(8423, 'classic_spell_mage_fire_power'), -- Flamestrike
(10215, 'classic_spell_mage_fire_power'), -- Flamestrike
(10216, 'classic_spell_mage_fire_power'), -- Flamestrike
(11113, 'classic_spell_mage_fire_power'), -- Blast Wave
(13018, 'classic_spell_mage_fire_power'), -- Blast Wave
(13019, 'classic_spell_mage_fire_power'), -- Blast Wave
(13020, 'classic_spell_mage_fire_power'), -- Blast Wave
(13021, 'classic_spell_mage_fire_power'), -- Blast Wave
(401502, 'classic_spell_mage_fire_power'), -- Frostfire Bolt
(1237312, 'classic_spell_mage_fire_power'), -- Frostfire Bolt
(1237313, 'classic_spell_mage_fire_power'), -- Frostfire Bolt
(133, 'classic_spell_mage_fire_power_periodic'), -- Fireball
(143, 'classic_spell_mage_fire_power_periodic'), -- Fireball
(145, 'classic_spell_mage_fire_power_periodic'), -- Fireball
(3140, 'classic_spell_mage_fire_power_periodic'), -- Fireball
(8400, 'classic_spell_mage_fire_power_periodic'), -- Fireball
(8401, 'classic_spell_mage_fire_power_periodic'), -- Fireball
(8402, 'classic_spell_mage_fire_power_periodic'), -- Fireball
(10148, 'classic_spell_mage_fire_power_periodic'), -- Fireball
(10149, 'classic_spell_mage_fire_power_periodic'), -- Fireball
(10150, 'classic_spell_mage_fire_power_periodic'), -- Fireball
(10151, 'classic_spell_mage_fire_power_periodic'), -- Fireball
(25306, 'classic_spell_mage_fire_power_periodic'), -- Fireball
(11366, 'classic_spell_mage_fire_power_periodic'), -- Pyroblast
(12505, 'classic_spell_mage_fire_power_periodic'), -- Pyroblast
(12522, 'classic_spell_mage_fire_power_periodic'), -- Pyroblast
(12523, 'classic_spell_mage_fire_power_periodic'), -- Pyroblast
(12524, 'classic_spell_mage_fire_power_periodic'), -- Pyroblast
(12525, 'classic_spell_mage_fire_power_periodic'), -- Pyroblast
(12526, 'classic_spell_mage_fire_power_periodic'), -- Pyroblast
(18809, 'classic_spell_mage_fire_power_periodic'), -- Pyroblast
(2120, 'classic_spell_mage_fire_power_periodic'), -- Flamestrike
(2121, 'classic_spell_mage_fire_power_periodic'), -- Flamestrike
(8422, 'classic_spell_mage_fire_power_periodic'), -- Flamestrike
(8423, 'classic_spell_mage_fire_power_periodic'), -- Flamestrike
(10215, 'classic_spell_mage_fire_power_periodic'), -- Flamestrike
(10216, 'classic_spell_mage_fire_power_periodic'), -- Flamestrike
(401502, 'classic_spell_mage_fire_power_periodic'), -- Frostfire Bolt
(1237312, 'classic_spell_mage_fire_power_periodic'), -- Frostfire Bolt
(1237313, 'classic_spell_mage_fire_power_periodic'); -- Frostfire Bolt
