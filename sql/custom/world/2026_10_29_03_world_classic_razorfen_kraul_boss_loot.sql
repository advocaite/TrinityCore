-- Classic 1.60: Razorfen Kraul bosses were missing their Forever blues (Ramtusk 274161, Agathelos 274160,
-- Aggem 274159+274158, Roogug's whole set 274155/274152/274149); the single-use references are inlined together
-- with the new items so each boss drops exactly one of its own set per kill.
-- rollback: re-insert the reference pointers (4420,1,30143),(4422,1,30145) each with
--   Chance=100,QuestRequired=0,GroupId=0,MinCount=1,MaxCount=1,Comment='VMaNGOS 1.12', re-insert
--   (4424,0,6681,100,0,1,0,1,1,'VMaNGOS 1.12'), then DELETE the rows inserted below.
DELETE FROM `creature_loot_template` WHERE `Entry` IN (4420,4422) AND `ItemType` = 1 AND `Item` IN (30143,30145);
DELETE FROM `creature_loot_template` WHERE `Entry` = 4424 AND `ItemType` = 0 AND `Item` = 6681 AND `GroupId` = 0;
DELETE FROM `creature_loot_template` WHERE `Entry` IN (4420,4422,4424,6168) AND `ItemType` = 0 AND `GroupId` = 1
  AND `Item` IN (6686,6687,274161,6690,6691,274160,6681,274159,274158,274155,274152,274149);
INSERT INTO `creature_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES
(4420,0,6686,0,0,1,1,1,1,'Overlord Ramtusk - Tusken Helm'),
(4420,0,6687,0,0,1,1,1,1,'Overlord Ramtusk - Corpsemaker'),
(4420,0,274161,0,0,1,1,1,1,'Overlord Ramtusk - Quillord Mail Leggings'),
(4422,0,6690,0,0,1,1,1,1,'Agathelos the Raging - Ferine Leggings'),
(4422,0,6691,0,0,1,1,1,1,'Agathelos the Raging - Swinetusk Shank'),
(4422,0,274160,0,0,1,1,1,1,'Agathelos the Raging - Quilrager Throwing Axe'),
(4424,0,6681,0,0,1,1,1,1,'Aggem Thorncurse - Thornspike'),
(4424,0,274159,0,0,1,1,1,1,'Aggem Thorncurse - Thorncursed Grips'),
(4424,0,274158,0,0,1,1,1,1,'Aggem Thorncurse - Death Prophet Spine'),
(6168,0,274155,0,0,1,1,1,1,'Roogug - Geomancer Headdress'),
(6168,0,274152,0,0,1,1,1,1,'Roogug - Roogug\'s Severed Head'),
(6168,0,274149,0,0,1,1,1,1,'Roogug - Thornweaver Drape');