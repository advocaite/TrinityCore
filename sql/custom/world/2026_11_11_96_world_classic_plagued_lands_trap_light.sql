-- Plagued Lands (2118, Darkshore): the Night Elven Bear Trap from Tharnariun's Hope (7586) left nothing on the ground.
-- The item's spell Placing Bear Trap (9437) places object 109515 (client data). Our template, from VMaNGOS, gives it model
-- 1287, which the 1.60 client draws as spellobject_invisibletrap.m2, so the player saw nothing. The quest says the bear has
-- to cross the light; the light is model 1308 (auraredshort.m2), the one VMaNGOS gives the Night Elven Bear Trap 111148.
-- The trap itself (spell 9439, radius, one charge) is unchanged.
-- Safe to run again.

UPDATE `gameobject_template` SET `displayId` = 1308 WHERE `entry` = 109515;
