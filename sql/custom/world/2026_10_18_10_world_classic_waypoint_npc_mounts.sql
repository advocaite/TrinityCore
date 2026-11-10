-- Waypoint NPCs whose per-spawn creature_addon row (mount 0) replaced the template addon and hid their mount.
-- Copy the template mount onto the spawn row. Only for the guids below, only where mount is still 0.
UPDATE `creature_addon` ca
JOIN `creature` c ON c.`guid` = ca.`guid`
JOIN `creature_template_addon` cta ON cta.`entry` = c.`id`
SET ca.`mount` = cta.`mount`
WHERE ca.`mount` = 0 AND cta.`mount` <> 0
  AND ca.`guid` IN (20068922, 20092287, 20190232, 20190233, 20190234, 20190235, 20285061, 20046216,
                    20043201, 20052348, 20053224, 20053203, 20053204, 20053201);
