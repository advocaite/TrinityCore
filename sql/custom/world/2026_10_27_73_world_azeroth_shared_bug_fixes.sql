-- Classic 1.60: four data bugs found by the Azeroth project's players that ours has too
-- Source: Azeroth project, Sakhwoow/WoWForever (GPL-2), their bug-report fixes of 2026-10-23 and 2026-10-25.

-- Deathguard Veterans (Tirisfal Glades): in the official sniffs (70450) only 5 Deathguard Veterans 275103 stand here and no
-- Deathguard Elite 7980 at all. Ours had 4 veterans on top of 4 Deathguard Elites (0-0.1 yd apart) and an Elite on the fifth spot.
DELETE FROM `creature` WHERE `guid` IN (20028707, 20028710, 20028712, 20028713) AND `id` = 7980;
UPDATE `creature` SET `id` = 275103, `spawntimesecs` = 120 WHERE `guid` = 20028709 AND `id` = 7980;

-- Flintfire's Shipment 672508 (quest 98321, 8 needed) opened empty: its personal loot 142903 had no rows.
-- Wowhead Forever: Flintfire's Shipment 280413 in 100% of them.
DELETE FROM `gameobject_loot_template` WHERE `Entry` = 142903;
INSERT INTO `gameobject_loot_template` (`Entry`, `ItemType`, `Item`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`, `Comment`) VALUES
(142903, 0, 280413, 100, 1, 1, 0, 1, 1, 'Flintfire''s Shipment (672508) - Flintfire''s Shipment');

-- Falaath Village 92529 (Zephyris) could not be handed in: the quest is player-cast on accept but had no spell, so Mark of Akir 1254832
-- was never applied (the "lost my mark" gossip recast is already in ours).
UPDATE `quest_template_addon` SET `SourceSpellID` = 1254832 WHERE `ID` = 92529;

-- Peacekeeper Elite 252477 recast Permanent Feign Death 1288546 every 0.9-1.5 s out of combat, so it lay down and moved:
-- cast only when the aura is missing.
UPDATE `smart_scripts` SET `action_param2` = 32 WHERE `entryorguid` = 252477 AND `source_type` = 0 AND `id` = 0 AND `action_type` = 11 AND `action_param1` = 1288546;
