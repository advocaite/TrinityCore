-- Stormwind Old Town, Officers' Lounge: the door and the Alliance Military Ranks plaque stood twice in the same spot
-- (player report 2026-11-05: two doors). The old copies (baseline guids 30026262 door 176576, 30026590 plaque 179735) go;
-- the Forever sniff copies (21201820 door 696375, 21201821 plaque 696376, client 70170) stay. The sniff entries get the
-- Russian names the old entries had.
DELETE FROM `gameobject` WHERE `guid` IN (30026262, 30026590) AND `id` IN (176576, 179735);
DELETE FROM `gameobject_addon` WHERE `guid` IN (30026262, 30026590);
DELETE FROM `game_event_gameobject` WHERE `guid` IN (30026262, 30026590);
DELETE FROM `pool_members` WHERE `type` = 1 AND `spawnId` IN (30026262, 30026590);
DELETE FROM `spawn_group` WHERE `spawnType` = 1 AND `spawnId` IN (30026262, 30026590);
DELETE FROM `gameobject_template_locale` WHERE `entry` IN (696375, 696376) AND `locale` = 'ruRU';
INSERT INTO `gameobject_template_locale` (`entry`,`locale`,`name`) VALUES
(696375,'ruRU','Дверь офицера'),
(696376,'ruRU','Воинские звания Альянса');
