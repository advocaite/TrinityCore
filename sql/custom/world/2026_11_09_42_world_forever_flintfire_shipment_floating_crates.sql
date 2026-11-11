-- Flintfire's Shipment 672508 (quest 98321): players found crates hanging in the air around Kharanos (Discord 2026-11-09, map 41,45 and
-- 47,52). The 12 floating ones are the spots 2026_11_02_29 put back from another world database: their heights were not measured on our
-- terrain (evenly spaced rows, 2 decimals). The 6 crates from the official sniff (guid 21201494-21201499) stay, 2 min respawn, enough
-- for the 8 the quest needs. Safe to run again.
DELETE FROM `gameobject` WHERE `id` = 672508 AND `guid` BETWEEN 9518458 AND 9518471;
