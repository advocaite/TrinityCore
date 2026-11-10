-- Nibbled-On Book (247834, starts 91741, the Northshire book chain) drops from Kobold Vermin (6) in one kill of four.
-- 2026_10_01_04 used the sniff's 2 of 32 loots (6.25%). The sniffer could not see the book again once the quest was taken
-- (LootItem::ItemAllowedForPlayer), so that count reads low, the same as the Wowhead counts in 2026_10_27_97/_98. At 6.25% half of
-- the players finish Kobold Camp Cleanup (10 Vermin) without it, and bots grinding the camp take kills too. At 25% about one
-- player in 18 still needs more than those ten kills. Report: sicko88 (RageZone, 2026-10-27), had to .additem it.
DELETE FROM `creature_loot_template` WHERE `Entry`=6 AND `ItemType`=0 AND `Item`=247834;
INSERT INTO `creature_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES
(6,0,247834,25,0,1,0,1,1,'Kobold Vermin: Nibbled-On Book, starts 91741');
