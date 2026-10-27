-- Cirrusfly Queen (251404, Thendal Grove, The Cirrusfly Queen 92463): two sniffed spawns 11 yd apart, so two queens stood
-- side by side. 21100464 has her patrol path; 21100465 is the static copy (a respawn recorded under a new guid). Nothing
-- else references 21100465.
DELETE FROM `creature` WHERE `guid` = 21100465 AND `id` = 251404;
DELETE FROM `creature_addon` WHERE `guid` = 21100465;
