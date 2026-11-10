-- Priest Garments quests (Garments of Darkness 5650 and the rest): Guard Roberts, Mountaineer Dolf, Deathguard Kel, Sentinel Shaya
-- and Grunt Kor'ja wait for the priest's Lesser Heal and Power Word: Fortitude (their SAI reacts to both), but a player can only
-- heal or buff a creature that has CREATURE_TYPE_FLAG_CAN_ASSIST (WorldObject::IsValidAssistTarget), and they had TypeFlags 0, so
-- the spells were refused. The other Forever server has the flag on four of them; Dolf is the same NPC for the dwarf quest. Safe to run again.
UPDATE `creature_template_difficulty` SET `TypeFlags` = `TypeFlags` | 0x1000 -- CREATURE_TYPE_FLAG_CAN_ASSIST
WHERE `Entry` IN (12423,12427,12428,12429,12430) AND `DifficultyID` = 0;
