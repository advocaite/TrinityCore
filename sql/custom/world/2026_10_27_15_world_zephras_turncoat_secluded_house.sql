-- The Turncoat (92643): "Find the secluded house in Shen'dar Highlands" (255013) only came from a 10 yard sphere at the spot
-- where the sniffing player was credited (2706.5 1100.7 769.9, 23 yards below and 70 yards west of the house). A player who
-- walked in another way never got it, and because "Find the Al'Aketh Turncoat" is sequenced after it, the Dead Cultist could
-- not be talked to for the quest (Discord report: the NPC lying in the house can't be interacted with).
-- The trigger now covers the area the quest map marks for that objective: the client's quest POI blob 1 for objective
-- 466639 (12 points), as a polygon around the same spawn point, 40 yards up and down.
UPDATE `areatrigger_create_properties` SET `Shape` = 3, `ShapeData0` = 40, `ShapeData1` = 40, `ShapeData2` = 0, `ShapeData3` = 0,
    `ShapeData4` = 0, `ShapeData5` = 0, `ShapeData6` = 0, `ShapeData7` = 0 WHERE `Id` = 43255 AND `IsCustom` = 0;
DELETE FROM `areatrigger_create_properties_polygon_vertex` WHERE `AreaTriggerCreatePropertiesId` = 43255 AND `IsCustom` = 0;
INSERT INTO `areatrigger_create_properties_polygon_vertex` (`AreaTriggerCreatePropertiesId`, `IsCustom`, `Idx`, `VerticeX`, `VerticeY`, `VerticeTargetX`, `VerticeTargetY`, `VerifiedBuild`) VALUES
(43255, 0, 0, -1.5, -66.7, NULL, NULL, 0),
(43255, 0, 1, 43.5, -66.7, NULL, NULL, 0),
(43255, 0, 2, 71.5, -66.7, NULL, NULL, 0),
(43255, 0, 3, 127.5, -55.7, NULL, NULL, 0),
(43255, 0, 4, 133.5, -21.7, NULL, NULL, 0),
(43255, 0, 5, 133.5, 23.3, NULL, NULL, 0),
(43255, 0, 6, 127.5, 68.3, NULL, NULL, 0),
(43255, 0, 7, 71.5, 68.3, NULL, NULL, 0),
(43255, 0, 8, 26.5, 68.3, NULL, NULL, 0),
(43255, 0, 9, -7.5, 51.3, NULL, NULL, 0),
(43255, 0, 10, -29.5, 23.3, NULL, NULL, 0),
(43255, 0, 11, -35.5, -32.7, NULL, NULL, 0);
