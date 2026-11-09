-- Fang of Githyiss (277190, starts 97236) drops from Githyiss the Vile (1994) on every kill for a player who can take the quest.
-- _42/_61 used Wowhead Forever's 235/4760 (4.9%). Wowhead counts every kill, also those by players who already have or finished
-- the quest, and the server never shows them a quest-starting item (LootItem::ItemAllowedForPlayer); Head of Rend Blackhand
-- reads 12/5866 there the same way. Report: Beastmade (Discord 2026-10-27), killed him twice, no fang.
DELETE FROM `creature_loot_template` WHERE `Entry`=1994 AND `ItemType`=0 AND `Item`=277190;
INSERT INTO `creature_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES
(1994,0,277190,100,0,1,0,1,1,'Githyiss the Vile: Fang of Githyiss, quest starter, every kill');
