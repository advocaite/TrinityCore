-- Moon Priestess Amara (2151): the per-spawn creature_addon row added for her waypoint path had mount 0.
-- A spawn addon replaces the template addon, so she lost her white sabercat mount (6080, the mount in her template addon).
UPDATE `creature_addon` SET `mount` = 6080 WHERE `guid` = 20046394 AND `mount` = 0
  AND EXISTS (SELECT 1 FROM `creature` WHERE `guid` = 20046394 AND `id` = 2151);
