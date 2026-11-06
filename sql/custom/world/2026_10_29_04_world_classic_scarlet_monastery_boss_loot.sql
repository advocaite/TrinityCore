-- Classic 1.60: Scarlet Monastery bosses were missing their Forever blues (Vishas 274290, Thalnos 274291); the single-use
-- references are inlined together with the new item so each boss drops exactly one of its own set per kill, and Doan gets
-- his Library Walkers at the Wowhead rate (9 of 19,420 kills).
-- rollback: re-insert the reference pointers (3983,1,30137),(4543,1,30149) each with
--   Chance=100,QuestRequired=0,GroupId=0,MinCount=1,MaxCount=1,Comment='VMaNGOS 1.12', then DELETE the rows inserted below.
DELETE FROM `creature_loot_template` WHERE `Entry` IN (3983,4543) AND `ItemType` = 1 AND `Item` IN (30137,30149);
DELETE FROM `creature_loot_template` WHERE `Entry` IN (3983,4543,6487) AND `ItemType` = 0 AND `Item` IN (7682,7683,274290,7684,7685,274291,274294);
INSERT INTO `creature_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES
(3983,0,7682,0,0,1,1,1,1,'Interrogator Vishas - Torturing Poker'),
(3983,0,7683,0,0,1,1,1,1,'Interrogator Vishas - Bloody Brass Knuckles'),
(3983,0,274290,0,0,1,1,1,1,'Interrogator Vishas - Painwalker Buckler'),
(4543,0,7684,0,0,1,1,1,1,'Bloodmage Thalnos - Bloodmage Mantle'),
(4543,0,7685,0,0,1,1,1,1,'Bloodmage Thalnos - Orb of the Forgotten Seer'),
(4543,0,274291,0,0,1,1,1,1,'Bloodmage Thalnos - Polished Skullcap'),
(6487,0,274294,0.0463,0,1,0,1,1,'Arcanist Doan - Library Walkers (wowhead: 9 of 19,420 kills)');