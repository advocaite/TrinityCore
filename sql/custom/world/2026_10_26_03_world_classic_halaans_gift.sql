-- The Anchors of Zephras (94414), objective 468541 "View the Anchor Pylon": Halaan Hawk-Eye (257554) lends his sight with
-- "Halaan, please lend me your gift" (menu 41842, option 0), which makes the player cast Halaan's Gift (1272014).
-- 1. Halaan's Gift is a far sight to a DB destination (TARGET_DEST_DB) and had no spell_target_position row, so nothing
--    showed. Destination: above the Thendal Standing Stones looking at the anchor pylon (picked in game, not sniffed).
-- 2. The objective was a criteria tree (221135, gain aura 1272014) that did not complete once the far sight worked, so the
--    gossip option now gives Halaan's kill credit directly; HIDE_CREDIT_MSG (0x10) hides "Halaan Hawk-Eye slain: 1/1" and
--    Halaan says a line instead. Safe to run again.
START TRANSACTION;

DELETE FROM `spell_target_position` WHERE `ID` = 1272014 AND `EffectIndex` = 0;
INSERT INTO `spell_target_position` (`ID`, `EffectIndex`, `OrderIndex`, `MapID`, `PositionX`, `PositionY`, `PositionZ`, `Orientation`, `VerifiedBuild`) VALUES
(1272014, 0, 0, 2991, 4306.801, 1636.801, 1145.852, 5.94, 0);

UPDATE `quest_objectives` SET `Type` = 0, `ObjectID` = 257554, `Flags` = `Flags` | 0x10 WHERE `ID` = 468541 AND `QuestID` = 94414;

DELETE FROM `smart_scripts` WHERE `entryorguid` = 257554 AND `source_type` = 0 AND `id` IN (2, 3);
INSERT INTO `smart_scripts` (`entryorguid`, `source_type`, `id`, `link`, `event_type`, `event_phase_mask`, `event_chance`, `event_flags`, `event_param1`, `event_param2`, `action_type`, `action_param1`, `target_type`, `comment`) VALUES
(257554, 0, 2, 0, 62, 0, 100, 0, 41842, 0, 33, 257554, 7, 'Halaan Hawk-Eye - On Gossip Option 0 Selected - Quest Credit for The Anchors of Zephras'),
(257554, 0, 3, 0, 62, 0, 100, 0, 41842, 0, 1, 0, 7, 'Halaan Hawk-Eye - On Gossip Option 0 Selected - Whisper Text 0 to Invoker');

DELETE FROM `creature_text` WHERE `CreatureID` = 257554 AND `GroupID` = 0;
INSERT INTO `creature_text` (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `Sound`, `SoundPlayType`, `BroadcastTextId`, `TextRange`, `comment`) VALUES
(257554, 0, 0, 'Halaan''s gift carries your sight to the anchor pylon. Look upon it, $n.', 42, 0, 100, 0, 0, 0, 0, 0, 0, 'Halaan Hawk-Eye - gift lent (raid boss whisper to the player)');

COMMIT;
