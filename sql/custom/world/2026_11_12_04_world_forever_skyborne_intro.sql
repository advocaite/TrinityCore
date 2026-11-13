-- Play the Skyborne opening scene on first login for both factions.
-- Source: Classic 1.60.1.70790 SceneScriptPackage.db2, package 4946:
-- 1.60.0_[SRI]_SkyborneRaceIntro_IGC. Tested in game with a Windshaper Shaman;
-- the client played the camera sequence and narration and reported scene completion.

DELETE FROM `scene_template` WHERE `SceneId` = 4946;
INSERT INTO `scene_template` (`SceneId`, `Flags`, `ScriptPackageID`, `Encrypted`, `ScriptName`) VALUES
(4946, 0, 4946, 0, '');

UPDATE `playercreateinfo` SET `intro_scene_id` = 4946
WHERE `race` IN (95, 96)
  AND `intro_movie_id` IS NULL
  AND (`intro_scene_id` IS NULL OR `intro_scene_id` = 4946);
