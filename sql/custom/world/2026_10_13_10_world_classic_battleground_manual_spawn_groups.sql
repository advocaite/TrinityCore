-- Classic 1.60: battleground spawn groups that start spawned become manual spawn groups (flag 4). As non-manual groups
-- without conditions, Map::UpdateSpawnGroupConditions switched them on again after SpawnGroupDespawn / SetSpawnGroupInactive.
-- The classic battleground scripts spawn them in OnInit.

-- Warsong Gulch flag stands and ghost gates, Arathi Basin neutral banners and ghost gates
UPDATE `spawn_group_template` SET `groupFlags` = `groupFlags` | 4 WHERE `groupId` IN (489001,489002,489003,529000,529010,529020,529030,529040,529060);

-- Alterac Valley event groups of the initial state (battleground_classic_alterac_valley constructor)
UPDATE `spawn_group_template` SET `groupFlags` = `groupFlags` | 4 WHERE `groupId` BETWEEN 300000 AND 302999;
