-- Classic 1.60 (WoW Forever): trainer lines for spells the 70532 client does not have (server audit 2026-10-24, finding 8).
--
-- 52 trainer_spell rows on spawned trainers (the 1000000+ profession lists and two class lists) teach spells absent from the client:
-- the old potion recipes 2337 (Minor Healing Potion), 3447 (Healing Potion), 7181 (Greater Healing Potion), 11457 (Superior
-- Healing Potion), 2855 and 17347 / 17348 (old profession rank spells) and Elune's Grace 19289-19293. The client shows a line
-- that can never be bought. The same ids are removed from every trainer list, spawned or not.

DELETE FROM `trainer_spell` WHERE `SpellId` IN (2337, 3447, 7181, 11457, 2855, 17347, 17348, 19289, 19290, 19291, 19292, 19293);
