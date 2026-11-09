-- Classic 1.60 (WoW Forever): SmartAI casts of spell ids the 70532 client does not have (server audit 2026-10-24, finding 11).
--
-- 171 spawned vanilla templates carry a SmartAI cast of a retail spell id (guards 95826, bears 75002, hyenas 82797 ...), which
-- fails silently. Only one of them has a vanilla replacement in the data: Mushgog (11447, Dire Maul arena) casts 81314 "Spore Cloud",
-- and VMaNGOS EventAI casts 22948 Spore Cloud, which the client has. The others have no VMaNGOS cast for the same creature, so
-- they stay as they are until a 70532 equivalent is known (the Death Talon 33001-33005 and Antu'sul 11314 rows are VMaNGOS's own
-- dummy ids from its EventAI conversion, next to the real casts, and are left alone too).

UPDATE `smart_scripts` SET `action_param1` = 22948, `comment` = 'Mushgog - In Combat - Cast ''Spore Cloud'' (22948, VMaNGOS)'
 WHERE `entryorguid` = 11447 AND `source_type` = 0 AND `id` = 1 AND `action_param1` = 81314;
