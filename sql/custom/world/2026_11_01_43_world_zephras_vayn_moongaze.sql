-- Zephras Isle, Shadowgale Forest: Vayn Moongaze (254151)
-- 1) "Show me what I've earned the right to purchase, Vayn." (gossip 44202, a vendor option) closed the window: the sniffed npcflag
--    is 3 (gossip, quest giver) without the vendor flag, so the server refused the vendor list. His 25 reputation items are in
--    npc_vendor already. The only Zephras NPC with a vendor option and no vendor flag.
UPDATE `creature_template` SET `npcflag` = `npcflag` | 128 WHERE `entry` = 254151;
-- 2) His camp (faction 3585, enemy of the Al'Aketh) stands 10-40 yards from Al'Aketh Footsoldiers and Stormchasers (3578), so they
--    fought all the time and nobody could talk to Vayn while he was in combat. The camp no longer fights other NPCs (players can
--    still pull the cultists as before).
UPDATE `creature_template` SET `unit_flags` = `unit_flags` | 0x200 WHERE `entry` IN (254151,258239,258136,258282,258130,258133,258135,258138);
