-- Rough Wolf Pelts (91751, Helene Peltskinner, Elwynn Forest): players reported (Discord, 2026-11-10) that the pelt never drops.
-- On the live realm the Rough Wolf Pelt (247877) is a quest item of two Elwynn wolves: Prowler (118) and Gray Forest Wolf
-- (1922) (creature query in an English player cache, build 70862). The sniff import (2026_10_01_04) only gave it to the
-- Prowler, so the Gray Forest Wolves, the common wolves of the quest areas, dropped nothing.
-- Adds the drop and the quest item to the Gray Forest Wolf (same 40% as the Prowler, only while on the quest) and makes sure
-- both wolves use their own loot table. Safe to run again.

UPDATE `creature_template_difficulty` SET `LootID` = `Entry` WHERE `DifficultyID` = 0 AND `LootID` = 0 AND `Entry` IN (118,1922);

DELETE FROM `creature_loot_template` WHERE (`Entry`,`Item`) IN ((118,247877),(1922,247877));
INSERT INTO `creature_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES
(118,0,247877,40,1,1,0,1,1,'Rough Wolf Pelt (quest 91751)'),
(1922,0,247877,40,1,1,0,1,1,'Rough Wolf Pelt (quest 91751)');

DELETE FROM `creature_questitem` WHERE `CreatureEntry` IN (118,1922) AND `DifficultyID` = 0 AND (`Idx` = 0 OR `ItemId` = 247877);
INSERT INTO `creature_questitem` (`CreatureEntry`,`DifficultyID`,`Idx`,`ItemId`,`VerifiedBuild`) VALUES
(118,0,0,247877,70862),
(1922,0,0,247877,70862);
