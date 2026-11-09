-- Classic 1.60: Darkmaster Gandling's Shadow Portal (17950) and the six room portals ran the stock TrinityCore
-- spell scripts, which do nothing unless the stock Scholomance instance script is loaded. The classic scripts
-- in classic_instance_scholomance.cpp take over: wipe the target's threat, port it into one of the six rooms,
-- close that room's gate and raise three Risen Guardians; the gate opens again when they are dead or gone.
DELETE FROM `spell_script_names` WHERE `spell_id` IN (17950, 17863, 17939, 17943, 17944, 17946, 17948)
    AND `ScriptName` IN ('spell_shadow_portal', 'spell_shadow_portal_rooms', 'classic_spell_gandling_shadow_portal', 'classic_spell_gandling_shadow_portal_room');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(17950, 'classic_spell_gandling_shadow_portal'),
(17863, 'classic_spell_gandling_shadow_portal_room'),
(17939, 'classic_spell_gandling_shadow_portal_room'),
(17943, 'classic_spell_gandling_shadow_portal_room'),
(17944, 'classic_spell_gandling_shadow_portal_room'),
(17946, 'classic_spell_gandling_shadow_portal_room'),
(17948, 'classic_spell_gandling_shadow_portal_room');
