-- Drops that start a quest but only dropped for players who already had a quest for them (Discord, Beastmade 2026-10-29:
-- Ukorsbane never drops Ukor's Lost Pack). These loot rows are marked QuestRequired, so the item drops only while a quest
-- in the log needs it, but the item itself starts the quest (ItemSparse StartQuestID, client 70658), so before that nobody
-- can loot it. Without QuestRequired the core still keeps the drop to the right players: a quest-starting item drops for
-- someone who hasn't taken its quest, and after that only while the quest needs it. Every QuestRequired row of an item with a
-- StartQuestID, in all loot tables (all are creature loot):
--   Ukorsbane 266851 Ukor's Lost Pack 275722 (96876), Halikor 266852 Halikor's Hoof 275723 (96877), Rockjaw Ambusher 1116 and
--   Backbreaker 1118 Stolen Blasting Powder 268548 (95213), Croaky 248278 247826 (91740), Elmpaw 248299 247862 (91746),
--   Fallen Mage 255066 and Kirin Tor Necromancer 269134 An Old History Book 2794 (337), Baron Marinous 270294 279275 (98028),
--   Whispering Horror 260396 268812 (95328), Wind Hollow 251676 257215 (93173), Hive'Zora Abomination 15449 21749 (8770).
-- Safe to run again.
UPDATE `creature_loot_template` SET `QuestRequired`=0 WHERE `QuestRequired`=1 AND (`Entry`,`Item`) IN
((266851,275722),(266852,275723),(1116,268548),(1118,268548),(248278,247826),(248299,247862),(255066,2794),(269134,2794),
(270294,279275),(260396,268812),(251676,257215),(15449,21749));
