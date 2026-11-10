-- NPCs that a quest has you speak to but that had no gossip flag, so a click opened nothing (Discord report: Nazgrel can't be
-- talked to during Exploring the Horde, 93739). Each has a sniffed gossip menu (70124) or a gossip-hello script for its quest,
-- and the gossip opens only with UNIT_NPC_FLAG_GOSSIP:
-- Nazgrel 3230 (menu 44858, "Thrall instructed me to speak with you."; credit on gossip hello, 10-26_21)
-- Vol'jin 10540 (menu 44861; "Speak with Vol'jin" talk-to objective; npcflag was 0, so not even the quest window opened)
-- Lady Sylvanas Windrunner 10181 (menu 44863; credit in npc_lady_sylvanas_windrunner on gossip hello)
-- Ott 3539 (menu 91003539; Ott's Masterwork credit on gossip hello; as a vendor without the flag a click opened only his goods)
UPDATE `creature_template` SET `npcflag` = `npcflag` | 1 WHERE `entry` IN (3230, 10540, 10181, 3539);
