-- Chol'aruk the Ravener (267307) stays in his cave under Thorn Hill (an indoor floor at z 80, GM .gps by the owner, 30 Oct 2026):
-- his spawn is the spot the owner moved him to with .npc move while standing on the cave floor. The way into the cave is a separate
-- open question (the navmesh cave floor is split into islands that connect to nothing; a public wiki lists the entrance at map 58, 25.5).
UPDATE `creature` SET `position_x` = -231.451, `position_y` = -3202.550, `position_z` = 79.808 WHERE `guid` = 26730700 AND `id` = 267307;
