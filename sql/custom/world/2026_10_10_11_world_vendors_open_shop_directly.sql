-- Vendors that only sell (or sell and repair) open the shop straight away instead of a dialog with one option.
--
-- In this world (VMaNGOS conversion) many shop-only NPCs carry UNIT_NPC_FLAG_GOSSIP (0x1), some also a stale
-- UNIT_NPC_FLAG_QUESTGIVER (0x2) although they start and end no quest. The 1.60 client opens a gossip window first
-- whenever the gossip bit is set, even when the menu holds a single "I want to browse your goods" entry. Dropping the
-- bits makes a right click open the shop directly (with the repair button), like the real vendors.
--
-- A template qualifies when ALL of these hold:
--   * it has a vendor flag (0xF80) and gossip and/or quest giver, nothing else except repair (0x1000);
--   * no C++ script, also none on one of its spawns (creature.ScriptName: spawns inherit the template flags), no quest
--     it starts or ends, no SmartAI gossip handling (events 62 and 64), also none attached to one of its spawns
--     (negative entryorguid = spawn guid);
--   * its gossip menu has at least one Vendor option (OptionNpc 1) and no option of any other type.
-- A spawn row is changed only when its own npcflag override is also made of those bits, it has no ScriptName and no
-- SmartAI of its own. Vendors with a gossip flag but no menu option at all are NOT touched here (they need a separate
-- decision). Everything else (trainers, innkeepers, bankers, flight masters, quest givers, scripted creatures) keeps
-- its dialog. Run twice, it changes nothing the second time.

DROP TEMPORARY TABLE IF EXISTS `tmp_shop_only_vendors`;
CREATE TEMPORARY TABLE `tmp_shop_only_vendors` AS
SELECT ct.`entry`
FROM `creature_template` ct
WHERE (ct.`npcflag` & 0x3) <> 0
  AND (ct.`npcflag` & 0xF80) <> 0
  AND (ct.`npcflag` & ~(0x1 | 0x2 | 0xF80 | 0x1000)) = 0
  AND (ct.`ScriptName` = '' OR ct.`ScriptName` IS NULL)
  AND NOT EXISTS (SELECT 1 FROM `creature_queststarter` qs WHERE qs.`id` = ct.`entry`)
  AND NOT EXISTS (SELECT 1 FROM `creature_questender` qe WHERE qe.`id` = ct.`entry`)
  AND NOT EXISTS (SELECT 1 FROM `smart_scripts` ss WHERE ss.`source_type` = 0 AND ss.`event_type` IN (62, 64) AND ss.`entryorguid` = ct.`entry`)
  AND NOT EXISTS (SELECT 1 FROM `creature` c JOIN `smart_scripts` ss ON ss.`source_type` = 0 AND ss.`event_type` IN (62, 64) AND ss.`entryorguid` = -CAST(c.`guid` AS SIGNED) WHERE c.`id` = ct.`entry`)
  AND NOT EXISTS (SELECT 1 FROM `creature` c WHERE c.`id` = ct.`entry` AND c.`ScriptName` <> '')
  AND EXISTS (SELECT 1 FROM `creature_template_gossip` g JOIN `gossip_menu_option` o ON o.`MenuID` = g.`MenuID` WHERE g.`CreatureID` = ct.`entry` AND o.`OptionNpc` = 1)
  AND NOT EXISTS (SELECT 1 FROM `creature_template_gossip` g JOIN `gossip_menu_option` o ON o.`MenuID` = g.`MenuID` WHERE g.`CreatureID` = ct.`entry` AND o.`OptionNpc` <> 1);

UPDATE `creature_template` SET `npcflag` = `npcflag` & ~0x3 WHERE `entry` IN (SELECT `entry` FROM `tmp_shop_only_vendors`);

UPDATE `creature` c
SET c.`npcflag` = c.`npcflag` & ~0x3
WHERE c.`npcflag` IS NOT NULL
  AND (c.`npcflag` & 0x3) <> 0
  AND (c.`npcflag` & 0xF80) <> 0
  AND (c.`npcflag` & ~(0x1 | 0x2 | 0xF80 | 0x1000)) = 0
  AND (c.`ScriptName` = '' OR c.`ScriptName` IS NULL)
  AND c.`id` IN (SELECT `entry` FROM `tmp_shop_only_vendors`)
  AND NOT EXISTS (SELECT 1 FROM `smart_scripts` ss WHERE ss.`source_type` = 0 AND ss.`event_type` IN (62, 64) AND ss.`entryorguid` = -CAST(c.`guid` AS SIGNED));

DROP TEMPORARY TABLE `tmp_shop_only_vendors`;
