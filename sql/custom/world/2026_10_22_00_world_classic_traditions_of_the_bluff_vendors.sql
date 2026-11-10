-- Classic 1.60 / Forever quest 97485 "Traditions of the Bluff" (Eylah Sunhorn 269068, Thunder Bluff) could not be
-- completed: the quest log tells the player to buy the four incense ingredients from the mid/upper rise suppliers
--   Bundle of Herbs              (278079) - Nida Winterhoof    3014  Herbalism Supplier
--   Bundle of Dried Cedar Twigs  (278080) - Nata Dawnstrider   3012  Enchanting Supplies
--   Sinew Thread                 (278081) - Mahu              3005  Leatherworking & Tailoring Supplies
--   Ceremonial Flint and Tinder  (278083) - Naal Mistrunner    3027  Cooking Supplier
-- but world.npc_vendor only carried one of the four rows (Naal/278083); the other three vendors had no such item, so
-- no player could ever obtain the reagents of Craft Incense (spell 1303520), craft Objective 476015's item
-- "Incense created" (278078 Prepared Incense) and reach the "Spread incense" objective (476016, kill credit 269255
-- from the incense use spell). Reported as "The items are not sold at the merchants".
--
-- All four items resolve on the client build the server runs (VerifiedBuild 70338: `.lookup item` lists 278079,
-- 278080, 278081, 278083 next to their 281041-281044 duplicates, and 278078 next to 281040), so the missing
-- npc_vendor rows - not the item data - were the defect. Rows copy Naal's existing 278083 row (maxcount 0,
-- no extended cost, no player condition: the ingredients are plain 1-copper vendor goods, not quest-gated).
-- INSERT ... ON DUPLICATE KEY UPDATE: safe to run again (primary key is entry+item+ExtendedCost+type).

INSERT INTO `npc_vendor`
    (`entry`, `slot`, `item`, `maxcount`, `incrtime`, `ExtendedCost`, `type`, `BonusListIDs`, `PlayerConditionID`, `IgnoreFiltering`, `VerifiedBuild`)
VALUES
    (3014, 101, 278079, 0, 0, 0, 1, NULL, 0, 0, 70124),   -- Nida Winterhoof  - Bundle of Herbs
    (3012, 417, 278080, 0, 0, 0, 1, NULL, 0, 0, 70124),   -- Nata Dawnstrider - Bundle of Dried Cedar Twigs
    (3005, 159, 278081, 0, 0, 0, 1, NULL, 0, 0, 70124)    -- Mahu             - Sinew Thread
ON DUPLICATE KEY UPDATE
    `slot` = VALUES(`slot`),
    `maxcount` = VALUES(`maxcount`),
    `ExtendedCost` = VALUES(`ExtendedCost`),
    `type` = VALUES(`type`);
