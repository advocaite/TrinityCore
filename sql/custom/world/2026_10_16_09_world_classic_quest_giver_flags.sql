-- Classic 1.60: three vanilla quest givers carry a creature_queststarter row but not the quest giver npcflag,
-- so the client never shows their "!" and the quest cannot be taken (1651, 1742 and 3549 start 5665, 3356 and 4184).
UPDATE `creature_template` SET `npcflag` = `npcflag` | 2 WHERE `entry` IN (1651, 1742, 3549) AND (`npcflag` & 2) = 0;
