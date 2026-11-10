-- Forever quests whose objective items had no source anywhere in the world DB.
-- Every row is INFERRED: no sniff loot data exists for these items. Each row names
-- the quest text and the sniff quest_poi area (or spawn area) it is taken from.

DELETE FROM `creature_loot_template` WHERE `Item` IN (248001,248003,248004,267414,277277,275719,280911,281031,281035,281036,281037,286209);
INSERT INTO `creature_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES
-- 91775 Book Return: Lost Book x6 "handled by kobolds"; sniff objective POI = Fargodeep Mine
(475,0,248001,35,1,1,0,1,1,'INFERRED 91775 Lost Book: quest text kobolds, sniff POI Fargodeep'),
(40,0,248001,35,1,1,0,1,1,'INFERRED 91775 Lost Book: quest text kobolds, sniff POI Fargodeep'),
-- 91777 Rare Books: "The kobolds must have taken them to the Jasperlode Mine", the master caster
(476,0,248003,25,1,1,0,1,1,'INFERRED 91777: quest text Jasperlode kobold master, Kobold Geomancer is the Jasperlode caster'),
(476,0,248004,25,1,1,0,1,1,'INFERRED 91777: quest text Jasperlode kobold master, Kobold Geomancer is the Jasperlode caster'),
-- 95212 Never Saddle on Quality: pelts from the leopards east of Amberstill Ranch
(1201,0,267414,50,1,1,0,1,1,'INFERRED 95212: quest text leopards east of Amberstill, Snow Leopard spawns there'),
-- 95774 Her Name Is Olgra: "Slay quilboars"; Razormane Raider 34487 has no spawns, these are the Razormane near Mankrik
(3265,0,277277,25,1,1,0,1,1,'INFERRED 95774: quest text slay Razormane quilboars'),
(3266,0,277277,25,1,1,0,1,1,'INFERRED 95774: quest text slay Razormane quilboars'),
(3267,0,277277,25,1,1,0,1,1,'INFERRED 95774: quest text slay Razormane quilboars'),
(3268,0,277277,25,1,1,0,1,1,'INFERRED 95774: quest text slay Razormane quilboars'),
(3269,0,277277,25,1,1,0,1,1,'INFERRED 95774: quest text slay Razormane quilboars'),
-- 96874 This Is Spinal Axe: spines from the beasts of Thunder Ridge
(3130,0,275719,50,1,1,0,1,1,'INFERRED 96874: quest text beasts of Thunder Ridge'),
(3131,0,275719,50,1,1,0,1,1,'INFERRED 96874: quest text beasts of Thunder Ridge'),
-- 98407 Show of Force: "Collect Spiked Collars from Redridge Thrashers"
(712,0,280911,60,1,1,0,1,1,'INFERRED 98407: quest text names Redridge Thrashers'),
-- 98424 Fizsprocket's Notes: start item 281031 is "folded in Fizsprocket's inside breast pocket"
(3051,0,281031,100,0,1,0,1,1,'INFERRED 98424 start item: quest text Fizsprocket pocket'),
-- the rest of the pages are "inside The Venture Co. Mine"; no page objects exist, supervisors run the mine
(2979,0,281035,15,1,1,0,1,1,'INFERRED 98424: quest text pages inside Venture Co. Mine'),
(2979,0,281036,15,1,1,0,1,1,'INFERRED 98424: quest text pages inside Venture Co. Mine'),
(2979,0,281037,15,1,1,0,1,1,'INFERRED 98424: quest text pages inside Venture Co. Mine'),
-- 99143 Bottles and Baubles: Shiny Junk x6; sniff objective POI is the Elwynn murloc shore
(285,0,286209,40,1,1,0,1,1,'INFERRED 99143: sniff POI murloc shore'),
(735,0,286209,40,1,1,0,1,1,'INFERRED 99143: sniff POI murloc shore');
