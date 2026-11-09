-- Sentinel Hill graveyard (Westfall): the spirit healer point sat 3.5 y under the terrain (floor 35.5); the core lifted it on every use.
UPDATE `world_safe_locs` SET `LocZ` = 36.0 WHERE `ID` = 4 AND `LocZ` < 35;
