-- Classic 1.60 (WoW Forever): Earthseer Farsen (264936) was missing (Discord: "his quests are in the game but he isn't, can't spawn him").
--
-- The sniff import (2026_10_01_03) brought the quest templates of his Dun Morogh chain, the start of the Hall of Thanes attunement,
-- but none of the sniffs saw him: no creature_template, no spawn, and no quest giver or taker rows. The same holds for every
-- quest of the chain, and for advocaite's forever branch and world dump of 2026-10-14. The chain (warcraft.wiki.gg, wowseer.gg,
-- wowhead tooltip of 264936: "Level 13 Humanoid (Normal)", reaction Ironforge, Dun Morogh 64.8 58.4):
--   96408 A Visitor to Dun Morogh  Beldin Steelgrill (1376) -> Farsen    optional breadcrumb
--   96392 Farsen's Watch           Farsen -> Farsen                       objective: Use Farsen's Far Sight (credit 264936)
--   96390 Nip 'Em in the Bud       Farsen -> Farsen                       10 Dark Iron Spies (6123) at Ironband's Compound
--   96391 Underground Map          Dark Iron Map (274268, spy drop) -> Farsen
--   96393 Old Ironforge Incursion  Farsen -> King Magni Bronzebeard (2784) Durgen Dirgehammer's Head (Hall of Thanes)
--
-- Spawn: the map position 64.8 58.4 converted with UiMapAssignment 46736 of UiMap 1426 (70532 client data) is -5794.6 -1389.3,
-- 4 yards from the campfire (gameobject 2061, guid 30010773) on the hill above Gol'Bolar Quarry; z is the terrain height there
-- from the 70532 map tile 0000_42_34, facing the fire.
-- Model: no sniff or public page gives his display. 144321 is a dwarf male display (CreatureDisplayInfoExtra 164780: race 3,
-- sex 0) that no creature uses and that sits in the client data next to the displays of the Hall of Thanes quest givers added
-- with him (Afadra Dunwall 264943 -> 144322 / 164781, Thom Filch 265003 -> 144349). Replace it if a sniff shows another.
-- Far Sight: spell 1293681 "Farsen's Far Sight" exists in the 70532 client with the same effects as Halaan's Gift (1272014:
-- add farsight at a database position + dummy aura), so the gossip works like Halaan Hawk-Eye (2026_10_02_04): while 96392 is
-- in the log, the option makes the player cast it, and the objective is credited. Its view point is Ironband's Compound,
-- where the spies are (wiki 76.7 60.7; a spy spawn point).
-- Dark Iron Map: drops from Dark Iron Spies while Nip 'Em in the Bud is in the log (wiki), and after it is turned in until the
-- map quest is taken, so nobody is locked out of the chain. The rate is not public; 20% is a placeholder until a sniff.
-- Safe to run again.

-- creature
DELETE FROM `creature_template` WHERE `entry` = 264936;
INSERT INTO `creature_template` (`entry`,`KillCredit1`,`KillCredit2`,`name`,`femaleName`,`subname`,`TitleAlt`,`IconName`,`RequiredExpansion`,`VignetteID`,`faction`,`npcflag`,`speed_walk`,`speed_run`,`scale`,`Classification`,`dmgschool`,`BaseAttackTime`,`RangeAttackTime`,`BaseVariance`,`RangeVariance`,`unit_class`,`unit_flags`,`unit_flags2`,`unit_flags3`,`family`,`trainer_class`,`type`,`VehicleId`,`AIName`,`MovementType`,`ExperienceModifier`,`RacialLeader`,`movementId`,`WidgetSetID`,`WidgetSetUnitConditionID`,`RegenHealth`,`CreatureImmunitiesId`,`flags_extra`,`ScriptName`,`VerifiedBuild`) VALUES
(264936,0,0,'Earthseer Farsen',NULL,NULL,NULL,NULL,0,0,55,3,1.0,1.142857,1.0,0,0,2000,2000,1,1,1,0,2048,0,0,0,7,0,'SmartAI',0,1,0,1693,0,0,1,0,0,'',0);
DELETE FROM `creature_template_difficulty` WHERE `Entry` = 264936;
INSERT INTO `creature_template_difficulty` (`Entry`,`DifficultyID`,`LevelScalingDeltaMin`,`LevelScalingDeltaMax`,`ContentTuningID`,`HealthScalingExpansion`,`HealthModifier`,`ManaModifier`,`ArmorModifier`,`DamageModifier`,`CreatureDifficultyID`,`TypeFlags`,`TypeFlags2`,`TypeFlags3`,`LootID`,`PickPocketLootID`,`SkinLootID`,`GoldMin`,`GoldMax`,`VerifiedBuild`) VALUES
(264936,0,0,0,0,0,1.0,1.0,1,1,0,134217728,0,0,0,0,0,0,0,0);
DELETE FROM `creature_template_model` WHERE `CreatureID` = 264936;
INSERT INTO `creature_template_model` (`CreatureID`,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`) VALUES
(264936,0,144321,1.0,1.0,0);
INSERT IGNORE INTO `creature_model_info` (`DisplayID`,`BoundingRadius`,`CombatReach`,`DisplayID_Other_Gender`,`VerifiedBuild`) VALUES
(144321,0.389,1.5,0,0);
DELETE FROM `creature_classic_level` WHERE `entry` = 264936;
INSERT INTO `creature_classic_level` (`entry`,`level_min`,`level_max`) VALUES
(264936,13,13);

-- spawn: Dun Morogh, hill above Gol'Bolar Quarry
DELETE FROM `creature` WHERE `guid` = 26493600 OR `id` = 264936;
INSERT INTO `creature` (`guid`,`id`,`map`,`zoneId`,`areaId`,`spawnDifficulties`,`phaseUseFlags`,`PhaseId`,`PhaseGroup`,`terrainSwapMap`,`position_x`,`position_y`,`position_z`,`orientation`,`modelid`,`equipment_id`,`spawntimesecs`,`wander_distance`,`currentwaypoint`,`curHealthPct`,`MovementType`,`npcflag`,`unit_flags`,`unit_flags2`,`unit_flags3`,`ScriptName`,`VerifiedBuild`) VALUES
(26493600,264936,0,1,0,'0',0,0,0,-1,-5794.6,-1389.3,440.7,4.903,0,0,300,0,0,100,0,NULL,NULL,NULL,NULL,'',0);

-- quest givers and takers
DELETE FROM `creature_queststarter` WHERE `quest` IN (96408,96392,96390,96393);
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES
(1376,96408,0),   -- Beldin Steelgrill: A Visitor to Dun Morogh
(264936,96392,0), -- Farsen's Watch
(264936,96390,0), -- Nip 'Em in the Bud
(264936,96393,0); -- Old Ironforge Incursion
DELETE FROM `creature_questender` WHERE `quest` IN (96408,96392,96390,96391,96393);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES
(264936,96408,0),
(264936,96392,0),
(264936,96390,0),
(264936,96391,0),
(2784,96393,0);   -- King Magni Bronzebeard

-- chain: A Visitor to Dun Morogh is an optional breadcrumb (the import had made it required), the dungeon quest follows the map
INSERT INTO `quest_template_addon` (`ID`,`PrevQuestID`,`BreadcrumbForQuestId`) VALUES
(96408,0,96392),
(96392,0,0),
(96393,96391,0)
ON DUPLICATE KEY UPDATE `PrevQuestID` = VALUES(`PrevQuestID`), `BreadcrumbForQuestId` = VALUES(`BreadcrumbForQuestId`);

-- Far Sight: gossip option while Farsen's Watch is in the log -> player casts Farsen's Far Sight, objective credited
DELETE FROM `creature_template_gossip` WHERE `CreatureID` = 264936;
INSERT INTO `creature_template_gossip` (`CreatureID`,`MenuID`,`VerifiedBuild`) VALUES
(264936,2649360,0);
DELETE FROM `gossip_menu_option` WHERE `MenuID` = 2649360;
INSERT INTO `gossip_menu_option` (`MenuID`,`GossipOptionID`,`OptionID`,`OptionNpc`,`OptionText`,`OptionBroadcastTextID`,`Language`,`Flags`,`ActionMenuID`,`ActionPoiID`,`GossipNpcOptionID`,`BoxCoded`,`BoxMoney`,`BoxText`,`BoxBroadcastTextID`,`SpellID`,`OverrideIconID`,`VerifiedBuild`) VALUES
(2649360,2649360,0,0,'Earthseer, please lend me your far sight.',0,0,0,0,0,NULL,0,0,NULL,0,NULL,NULL,0);
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId` = 15 AND `SourceGroup` = 2649360;
INSERT INTO `conditions` (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,`ConditionTypeOrReference`,`ConditionTarget`,
`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`) VALUES
(15,2649360,0,0,0,9,0,96392,0,0,0,0,0,'','Earthseer Farsen - Show gossip option only while Farsen''s Watch is taken'),
(15,2649360,0,0,0,28,0,96392,0,0,1,0,0,'','Earthseer Farsen - Hide gossip option once Farsen''s Watch is complete');

DELETE FROM `smart_scripts` WHERE `entryorguid` = 264936 AND `source_type` = 0;
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,
`event_param2`,`action_type`,`action_param1`,`action_param2`,`target_type`,`comment`) VALUES
(264936,0,0,1,62,0,100,0,2649360,0,85,1293681,0,7,'Earthseer Farsen - On Gossip Option 0 Selected - Invoker Casts Farsen''s Far Sight'),
(264936,0,1,2,61,0,100,0,0,0,33,264936,0,7,'Earthseer Farsen - Linked - Quest Credit Use Farsen''s Far Sight'),
(264936,0,2,0,61,0,100,0,0,0,72,0,0,7,'Earthseer Farsen - Linked - Close Gossip');

DELETE FROM `spell_target_position` WHERE `ID` = 1293681 AND `EffectIndex` = 0;
INSERT INTO `spell_target_position` (`ID`,`EffectIndex`,`OrderIndex`,`MapID`,`PositionX`,`PositionY`,`PositionZ`,`Orientation`,`VerifiedBuild`) VALUES
(1293681,0,0,0,-5889.97,-2007.89,408.7,0,0); -- Farsen's Far Sight: Ironband's Compound

-- Dark Iron Map from Dark Iron Spies (runs after the VMaNGOS loot file 2026_10_24_10, which replaces loot 6123)
DELETE FROM `creature_loot_template` WHERE `Entry` = 6123 AND `Item` = 274268;
INSERT INTO `creature_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES
(6123,0,274268,20,0,1,0,1,1,'Dark Iron Spy - Dark Iron Map (starts Underground Map)');
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId` = 1 AND `SourceGroup` = 6123 AND `SourceEntry` = 274268;
INSERT INTO `conditions` (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,`ConditionTypeOrReference`,`ConditionTarget`,
`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`) VALUES
(1,6123,274268,0,0,9,0,96390,0,0,0,0,0,'','Dark Iron Map - drops while Nip ''Em in the Bud is taken'),
(1,6123,274268,0,1,8,0,96390,0,0,0,0,0,'','Dark Iron Map - or after Nip ''Em in the Bud is turned in'),
(1,6123,274268,0,1,9,0,96391,0,0,1,0,0,'','Dark Iron Map - and Underground Map is not taken'),
(1,6123,274268,0,1,8,0,96391,0,0,1,0,0,'','Dark Iron Map - and Underground Map is not turned in');
