-- Classic 1.60: event spawns whose creature_equip_template set 1 does not exist in this world.
-- The converted event spawns (guids 20,000,001-20,999,999, 2026_10_17_52 and 2026_10_17_53) carry equipment_id 1 wherever VMaNGOS
-- gave the spawn equipment. TrinityCore reads that as "the entry's set 1 in creature_equip_template"; for an entry without one the
-- loader logs "equipment_id 1 not found in table creature_equip_template, set to no equipment" at every start. Those spawns get 0
-- here, so the log stays clean. Data-driven: whichever sets exist when this runs decide, and running it again changes nothing.
UPDATE `creature` AS `c`
LEFT JOIN `creature_equip_template` AS `e` ON `e`.`CreatureID` = `c`.`id` AND `e`.`ID` = 1
SET `c`.`equipment_id` = 0
WHERE `c`.`guid` BETWEEN 20000001 AND 20999999 AND `c`.`equipment_id` = 1 AND `e`.`CreatureID` IS NULL;
