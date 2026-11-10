-- Forever creatures whose faction disagrees with Wowhead Forever (react to Alliance / Horde), and Captain Sander's Locker.
-- Faction ids are the ones VMaNGOS 1.12 gives the matching vanilla mobs around each spawn. Safe to run again.

-- Duskwood: Lost Stalker / Defender / Knight are hostile undead on Wowhead Forever. 35 made the Defender and Knight friendly.
-- 21 is the VMaNGOS faction of the Duskwood undead around them (Skeletal Fiend, Flesh Eater, Plague Spreader).
UPDATE `creature_template` SET `faction` = 21 WHERE `entry` IN (272260, 272284, 272291);

-- Wetlands: Carnage (rare beast) is hostile on Wowhead Forever; 48 is the faction of the Highland raptors it spawns among.
UPDATE `creature_template` SET `faction` = 48 WHERE `entry` = 271336;

-- Plains Coyote / Plains Prowler (level 36-40) are hostile on Wowhead Forever; 38 is the vanilla coyote and wolf faction
-- (Crag Coyote, Coyote, Prairie Wolf).
UPDATE `creature_template` SET `faction` = 38 WHERE `entry` IN (259267, 268831);

-- Bolder'ok ogres: the Brute is hostile to Alliance on Wowhead Forever (35 made it friendly). 45 is the VMaNGOS ogre faction
-- (Boulderfist, Dustbelcher, Mo'grosh). Wowhead has no reaction for the Magus; it stands in the same camp, so it gets the same.
UPDATE `creature_template` SET `faction` = 45 WHERE `entry` IN (255849, 255850);

-- Rog'mar orcs: Wowhead Forever has them hostile to Alliance and friendly to Horde, as the Rog'mar Poacher already is (29).
UPDATE `creature_template` SET `faction` = 29 WHERE `entry` IN (255996, 256749);

-- Captain Sander's Locker (270257) had no contents. Wowhead Forever lists Rusty Cutlass, Midshipman's Worn Boots and
-- Captain Sander's Lucky Shooter as coming from it, without chances: one of the three, equal chance.
DELETE FROM `item_loot_template` WHERE `Entry` = 270257;
INSERT INTO `item_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES
(270257,0,270001,0,0,1,1,1,1,'Captain Sander''s Locker - Rusty Cutlass'),
(270257,0,270002,0,0,1,1,1,1,'Captain Sander''s Locker - Midshipman''s Worn Boots'),
(270257,0,276694,0,0,1,1,1,1,'Captain Sander''s Locker - Captain Sander''s Lucky Shooter');
