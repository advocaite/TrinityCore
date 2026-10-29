-- Classic 1.60: the six Atal'ai warders' shared reference 30160 (10783-10788) points at items that do not exist in the
-- Forever universe (Wowhead tooltip "Entity not found", zero drops observed across thousands of kills); the dead pointer
-- rows are removed. All other Sunken Temple set items (Eranikus 10833, Jammal'an 10807, Ogom 10804, Atal'alarion 10799,
-- dragons 12243/12465/12466, Spawn of Hakkar 10801/10802) are Wowhead-real but missing from this client's item data —
-- their rows stay, inert pending an item import.
-- rollback: re-insert (5712,1,30160,40,0,1,0,1,1,'VMaNGOS 1.12') ... likewise for 5713-5717, then .reload creature_loot_template.
DELETE FROM `creature_loot_template` WHERE `Entry` IN (5712,5713,5714,5715,5716,5717) AND `ItemType` = 1 AND `Item` = 30160;