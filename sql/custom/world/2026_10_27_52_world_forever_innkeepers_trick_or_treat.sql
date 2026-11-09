-- Classic 1.60 (WoW Forever): Forever's new innkeepers do Trick or Treat during Hallow's End, like the 45 vanilla ones.
--
-- 1. Coriella Calmbreeze (254089) and Donaal Downbreeze (255940) share the sniffed Zephras Isle menu 41301. It gets the vanilla
--    option (broadcast text 10693), shown while event 12 runs and the player has no Tricked or Treated (24755), as on every
--    vanilla innkeeper menu.
DELETE FROM `gossip_menu_option` WHERE `MenuID` = 41301 AND `OptionID` = 2;
INSERT INTO `gossip_menu_option` (`MenuID`, `GossipOptionID`, `OptionID`, `OptionNpc`, `OptionText`, `OptionBroadcastTextID`, `Language`, `Flags`, `ActionMenuID`, `ActionPoiID`, `GossipNpcOptionID`, `BoxCoded`, `BoxMoney`, `BoxText`, `BoxBroadcastTextID`, `SpellID`, `OverrideIconID`, `VerifiedBuild`) VALUES
(41301, 990041301, 2, 0, 'Trick or Treat!', 10693, 0, 0, 0, 0, NULL, 0, 0, '', 0, NULL, NULL, 0);
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId` = 15 AND `SourceGroup` = 41301 AND `SourceEntry` = 2;
INSERT INTO `conditions` (`SourceTypeOrReferenceId`, `SourceGroup`, `SourceEntry`, `SourceId`, `ElseGroup`, `ConditionTypeOrReference`, `ConditionTarget`, `ConditionValue1`, `ConditionValue2`, `ConditionValue3`, `ConditionStringValue1`, `NegativeCondition`, `ErrorType`, `ErrorTextId`, `ScriptName`, `Comment`) VALUES
(15, 41301, 2, 0, 0, 12, 0, 12, 0, 0, '', 0, 0, 0, '', 'Innkeeper Trick or Treat - only during Hallow''s End'),
(15, 41301, 2, 0, 0, 1, 0, 24755, 0, 0, '', 1, 0, 0, '', 'Innkeeper Trick or Treat - not while Tricked or Treated');

-- 2. The eight innkeepers from the other Forever server world dump came without a gossip menu, so a right click opened an empty window: no
--    Trick or Treat, but also no "Make this inn my home" or shop. They get the generic vanilla innkeeper menu 342 (six vanilla
--    innkeepers use it; Trick or Treat is already gated there). Berdun Cliffbrew (240482) and Adarien (252691) sell nothing, so
--    the two shop options of 342 are hidden on them.
DELETE FROM `creature_template_gossip` WHERE `CreatureID` IN (240482, 243815, 244512, 252691, 255894, 256675, 257595, 258380);
INSERT INTO `creature_template_gossip` (`CreatureID`, `MenuID`, `VerifiedBuild`) VALUES
(240482, 342, 0), (243815, 342, 0), (244512, 342, 0), (252691, 342, 0),
(255894, 342, 0), (256675, 342, 0), (257595, 342, 0), (258380, 342, 0);
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId` = 15 AND `SourceGroup` = 342 AND `SourceEntry` IN (3, 4) AND `ConditionTypeOrReference` = 51 AND `ConditionValue2` IN (240482, 252691);
INSERT INTO `conditions` (`SourceTypeOrReferenceId`, `SourceGroup`, `SourceEntry`, `SourceId`, `ElseGroup`, `ConditionTypeOrReference`, `ConditionTarget`, `ConditionValue1`, `ConditionValue2`, `ConditionValue3`, `ConditionStringValue1`, `NegativeCondition`, `ErrorType`, `ErrorTextId`, `ScriptName`, `Comment`) VALUES
(15, 342, 3, 0, 0, 51, 1, 5, 240482, 0, '', 1, 0, 0, '', 'Innkeeper menu - no shop on Berdun Cliffbrew'),
(15, 342, 4, 0, 0, 51, 1, 5, 240482, 0, '', 1, 0, 0, '', 'Innkeeper menu - no shop on Berdun Cliffbrew'),
(15, 342, 3, 0, 0, 51, 1, 5, 252691, 0, '', 1, 0, 0, '', 'Innkeeper menu - no shop on Adarien'),
(15, 342, 4, 0, 0, 51, 1, 5, 252691, 0, '', 1, 0, 0, '', 'Innkeeper menu - no shop on Adarien');

-- 3. Picking the option: close the gossip and cast Trick or Treat (24751) at the player, the same two rows as the vanilla innkeepers.
--    None of these ten has a script of its own.
DELETE FROM `smart_scripts` WHERE `source_type` = 0 AND `entryorguid` IN (254089, 255940, 240482, 243815, 244512, 252691, 255894, 256675, 257595, 258380) AND `id` IN (0, 1);
INSERT INTO `smart_scripts` (`entryorguid`, `source_type`, `id`, `link`, `Difficulties`, `event_type`, `event_phase_mask`, `event_chance`, `event_flags`, `event_param1`, `event_param2`, `event_param3`, `event_param4`, `event_param5`, `event_param_string`, `action_type`, `action_param1`, `action_param2`, `action_param3`, `action_param4`, `action_param5`, `action_param6`, `action_param7`, `action_param_string`, `target_type`, `target_param1`, `target_param2`, `target_param3`, `target_param4`, `target_param_string`, `target_x`, `target_y`, `target_z`, `target_o`, `comment`) VALUES
(254089, 0, 0, 1, '', 62, 0, 100, 0, 41301, 2, 0, 0, 0, '', 72, 0, 0, 0, 0, 0, 0, 0, '', 7, 0, 0, 0, 0, '', 0, 0, 0, 0, 'Coriella Calmbreeze - On Gossip Option 2 Selected - Close Gossip'),
(254089, 0, 1, 0, '', 61, 0, 100, 0, 0, 0, 0, 0, 0, '', 11, 24751, 0, 0, 0, 0, 0, 0, '', 7, 0, 0, 0, 0, '', 0, 0, 0, 0, 'Coriella Calmbreeze - Linked - Cast Trick or Treat at the player'),
(255940, 0, 0, 1, '', 62, 0, 100, 0, 41301, 2, 0, 0, 0, '', 72, 0, 0, 0, 0, 0, 0, 0, '', 7, 0, 0, 0, 0, '', 0, 0, 0, 0, 'Donaal Downbreeze - On Gossip Option 2 Selected - Close Gossip'),
(255940, 0, 1, 0, '', 61, 0, 100, 0, 0, 0, 0, 0, 0, '', 11, 24751, 0, 0, 0, 0, 0, 0, '', 7, 0, 0, 0, 0, '', 0, 0, 0, 0, 'Donaal Downbreeze - Linked - Cast Trick or Treat at the player'),
(240482, 0, 0, 1, '', 62, 0, 100, 0, 342, 0, 0, 0, 0, '', 72, 0, 0, 0, 0, 0, 0, 0, '', 7, 0, 0, 0, 0, '', 0, 0, 0, 0, 'Berdun Cliffbrew - On Gossip Option 0 Selected - Close Gossip'),
(240482, 0, 1, 0, '', 61, 0, 100, 0, 0, 0, 0, 0, 0, '', 11, 24751, 0, 0, 0, 0, 0, 0, '', 7, 0, 0, 0, 0, '', 0, 0, 0, 0, 'Berdun Cliffbrew - Linked - Cast Trick or Treat at the player'),
(243815, 0, 0, 1, '', 62, 0, 100, 0, 342, 0, 0, 0, 0, '', 72, 0, 0, 0, 0, 0, 0, 0, '', 7, 0, 0, 0, 0, '', 0, 0, 0, 0, 'Sebelia - On Gossip Option 0 Selected - Close Gossip'),
(243815, 0, 1, 0, '', 61, 0, 100, 0, 0, 0, 0, 0, 0, '', 11, 24751, 0, 0, 0, 0, 0, 0, '', 7, 0, 0, 0, 0, '', 0, 0, 0, 0, 'Sebelia - Linked - Cast Trick or Treat at the player'),
(244512, 0, 0, 1, '', 62, 0, 100, 0, 342, 0, 0, 0, 0, '', 72, 0, 0, 0, 0, 0, 0, 0, '', 7, 0, 0, 0, 0, '', 0, 0, 0, 0, 'Ortak Lomgok - On Gossip Option 0 Selected - Close Gossip'),
(244512, 0, 1, 0, '', 61, 0, 100, 0, 0, 0, 0, 0, 0, '', 11, 24751, 0, 0, 0, 0, 0, 0, '', 7, 0, 0, 0, 0, '', 0, 0, 0, 0, 'Ortak Lomgok - Linked - Cast Trick or Treat at the player'),
(252691, 0, 0, 1, '', 62, 0, 100, 0, 342, 0, 0, 0, 0, '', 72, 0, 0, 0, 0, 0, 0, 0, '', 7, 0, 0, 0, 0, '', 0, 0, 0, 0, 'Adarien - On Gossip Option 0 Selected - Close Gossip'),
(252691, 0, 1, 0, '', 61, 0, 100, 0, 0, 0, 0, 0, 0, '', 11, 24751, 0, 0, 0, 0, 0, 0, '', 7, 0, 0, 0, 0, '', 0, 0, 0, 0, 'Adarien - Linked - Cast Trick or Treat at the player'),
(255894, 0, 0, 1, '', 62, 0, 100, 0, 342, 0, 0, 0, 0, '', 72, 0, 0, 0, 0, 0, 0, 0, '', 7, 0, 0, 0, 0, '', 0, 0, 0, 0, 'Innkeeper Zizplink - On Gossip Option 0 Selected - Close Gossip'),
(255894, 0, 1, 0, '', 61, 0, 100, 0, 0, 0, 0, 0, 0, '', 11, 24751, 0, 0, 0, 0, 0, 0, '', 7, 0, 0, 0, 0, '', 0, 0, 0, 0, 'Innkeeper Zizplink - Linked - Cast Trick or Treat at the player'),
(256675, 0, 0, 1, '', 62, 0, 100, 0, 342, 0, 0, 0, 0, '', 72, 0, 0, 0, 0, 0, 0, 0, '', 7, 0, 0, 0, 0, '', 0, 0, 0, 0, 'Innkeeper Toka - On Gossip Option 0 Selected - Close Gossip'),
(256675, 0, 1, 0, '', 61, 0, 100, 0, 0, 0, 0, 0, 0, '', 11, 24751, 0, 0, 0, 0, 0, 0, '', 7, 0, 0, 0, 0, '', 0, 0, 0, 0, 'Innkeeper Toka - Linked - Cast Trick or Treat at the player'),
(257595, 0, 0, 1, '', 62, 0, 100, 0, 342, 0, 0, 0, 0, '', 72, 0, 0, 0, 0, 0, 0, 0, '', 7, 0, 0, 0, 0, '', 0, 0, 0, 0, 'Barracks Master Harlan - On Gossip Option 0 Selected - Close Gossip'),
(257595, 0, 1, 0, '', 61, 0, 100, 0, 0, 0, 0, 0, 0, '', 11, 24751, 0, 0, 0, 0, 0, 0, '', 7, 0, 0, 0, 0, '', 0, 0, 0, 0, 'Barracks Master Harlan - Linked - Cast Trick or Treat at the player'),
(258380, 0, 0, 1, '', 62, 0, 100, 0, 342, 0, 0, 0, 0, '', 72, 0, 0, 0, 0, 0, 0, 0, '', 7, 0, 0, 0, 0, '', 0, 0, 0, 0, 'Caitir Flinthew - On Gossip Option 0 Selected - Close Gossip'),
(258380, 0, 1, 0, '', 61, 0, 100, 0, 0, 0, 0, 0, 0, '', 11, 24751, 0, 0, 0, 0, 0, 0, '', 7, 0, 0, 0, 0, '', 0, 0, 0, 0, 'Caitir Flinthew - Linked - Cast Trick or Treat at the player');
UPDATE `creature_template` SET `AIName` = 'SmartAI' WHERE `entry` IN (254089, 255940, 240482, 243815, 244512, 252691, 255894, 256675, 257595, 258380) AND `ScriptName` = '';
