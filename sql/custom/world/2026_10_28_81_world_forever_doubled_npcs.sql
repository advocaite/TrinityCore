-- NPCs standing twice on one spot: the vanilla spawn and the Forever twin of the same NPC (another entry, same name and model).
-- Found by a scan of the full world (forever-tdb-2026-10-14 + every sql/custom file): same name, same map, under 3 yards apart,
-- outside pools and events. Discord report 2026-10-28: Mokvar in Grommash Hold.
--
-- Mokvar (Orgrimmar) and Deliana (Ironforge): the vanilla 16012 / 16013 keep the dungeon set quests, their quest-end scripts,
-- gossip and locales; the Forever twins 230317 (sniff 2026_10_03_00) and 230319 (Azeroth import 2026_10_27_70) stood inside them.
-- The twins' spawns go. Forever's Mokvar is also a vendor (2026_10_26_41 stocked 230317), so 16012 takes over that stock.
DELETE FROM `creature` WHERE `guid`=21200548 AND `id`=230317;
DELETE FROM `creature` WHERE `guid`=43000027 AND `id`=230319;
DELETE FROM `creature_addon` WHERE `guid` IN (21200548, 43000027);
DELETE FROM `npc_vendor` WHERE `entry`=16012;
INSERT INTO `npc_vendor` (`entry`,`slot`,`item`,`maxcount`,`incrtime`,`ExtendedCost`,`type`,`BonusListIDs`,`PlayerConditionID`,`IgnoreFiltering`,`VerifiedBuild`)
SELECT 16012,`slot`,`item`,`maxcount`,`incrtime`,`ExtendedCost`,`type`,`BonusListIDs`,`PlayerConditionID`,`IgnoreFiltering`,`VerifiedBuild` FROM `npc_vendor` WHERE `entry`=230317;
UPDATE `creature_template` SET `npcflag`=`npcflag`|128 WHERE `entry`=16012;

-- Mount display creatures at the Undercity, Orgrimmar and Sen'jin stables: the vanilla ones and the sniffed Forever ones
-- (2026_10_03_00, 0.7-2.8 yards away) both stood there. The sniffed ones stay.
DELETE FROM `creature` WHERE `guid` IN (20031906, 20031907, 20031909, 20031910, 20031940, 20006616, 20006619, 20095101, 20095102, 20095104, 20095105, 20095106)
    AND `id` IN (12342, 12341, 12343, 11156, 14558, 12351, 5195, 14540, 14541, 14543, 14544, 14545);
DELETE FROM `creature_addon` WHERE `guid` IN (20031906, 20031907, 20031909, 20031910, 20031940, 20006616, 20006619, 20095101, 20095102, 20095104, 20095105, 20095106);

-- Grinkle (Orgrimmar): the sniff caught him while the Stranglethorn Fishing Extravaganza announcement ran and saved him as a
-- static spawn on top of the event's own one (event 14), so he stood twice there every week and once all the rest.
DELETE FROM `creature` WHERE `guid`=21200540 AND `id`=15116;
DELETE FROM `creature_addon` WHERE `guid`=21200540;
