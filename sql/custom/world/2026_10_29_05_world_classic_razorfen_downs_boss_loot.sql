-- Classic 1.60: Ragglesnout's reference 30206 points at three items (10758/10767/10768) that exist neither in the client
-- item dump (world.armory_item, hotfixes.item_sparse) nor in Wowhead Forever (tooltip "Entity not found"); the dead pointer
-- is removed so the rare matches the world drops Wowhead actually observes. All other Razorfen Downs bosses already drop
-- their full sets via references 30207/30209/30210/30220 and need no change.
-- rollback: re-insert (7354,1,30206,100,0,1,0,1,1,'VMaNGOS 1.12') into creature_loot_template and .reload creature_loot_template.
DELETE FROM `creature_loot_template` WHERE `Entry` = 7354 AND `ItemType` = 1 AND `Item` = 30206;