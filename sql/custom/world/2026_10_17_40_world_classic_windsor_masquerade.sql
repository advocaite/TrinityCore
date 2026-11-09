-- Classic 1.60: The Great Masquerade (6403). Reginald Windsor (12580, summoned by Squire Rowe) now runs the Stormwind walk
-- from the gates to the keep (classic_npc_reginald_windsor in classic_stormwind_rendezvous.cpp): the city guards salute and
-- kneel, General Marcus Jonathan kneels, Lady Katrana Prestor is revealed as Onyxia and her royal guards fight Bolvar, and the
-- quest completes for the escorting group. Until now the summoned Windsor rode to his first waypoint and stood there.
UPDATE `creature_template` SET `ScriptName` = 'classic_npc_reginald_windsor', `AIName` = '' WHERE `entry` = 12580;
DELETE FROM `smart_scripts` WHERE `entryorguid` = 12580 AND `source_type` = 0;
