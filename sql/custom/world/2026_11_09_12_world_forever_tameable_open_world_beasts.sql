-- Follow-up to 2026_11_09_10: on the dev world 6111 beasts have a pet family but no tameable flag. In Blizzard's answers in the
-- players' caches of 11-08, 248 of 250 classic beasts with a family are tameable (the two that are not: 10992 Enraged Panther,
-- a quest beast, and 211967 Leech Stalker Corpse). Most of the 6111 are retail beasts that never spawn on our maps.
-- Rule (INFERRED, there is no full source: the client has no table with these flags): a beast is made tameable when
--   - it has one of the classic hunter pet families (wolf, cat, spider, bear, boar, crocolisk, carrion bird, crab, gorilla, raptor,
--     tallstrider, scorpid, turtle, bat, hyena, owl, wind serpent),
--   - it spawns in the open world (Eastern Kingdoms or Kalimdor), not in a dungeon or raid,
--   - it is not a boss, has no NPC menu and is not one of the two beasts above.
-- Only the tameable bit is added. Safe to run more than once.
UPDATE `creature_template_difficulty` ctd
  JOIN `creature_template` ct ON ct.`entry` = ctd.`Entry`
   SET ctd.`TypeFlags` = ctd.`TypeFlags` | 1
 WHERE ctd.`DifficultyID` = 0 AND (ctd.`TypeFlags` & 1) = 0
   AND ct.`type` = 1 AND ct.`family` IN (1,2,3,4,5,6,7,8,9,11,12,20,21,24,25,26,27)
   AND ct.`Classification` <> 3 AND ct.`npcflag` = 0
   AND ct.`entry` NOT IN (10992, 211967)
   AND EXISTS (SELECT 1 FROM `creature` c WHERE c.`id` = ct.`entry` AND c.`map` IN (0, 1));
