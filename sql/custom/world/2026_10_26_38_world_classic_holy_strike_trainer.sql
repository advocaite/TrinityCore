-- Classic 1.60 client 70532: Holy Strike rank 1 (679) is a live paladin spell again (level 6, Retribution, every race), but the city
-- paladin trainers (list 29, Undercity included) still had it at ReqLevel 99 from when the client named it zzOLDHoly Strike. A paladin
-- who skipped rank 1 at the starting trainer could not buy it there, and ranks 2-8 need rank 1 first. Level 6 as on the starting list 28.
UPDATE `trainer_spell` SET `ReqLevel` = 6 WHERE `TrainerId` = 29 AND `SpellId` = 679 AND `ReqLevel` = 99;
