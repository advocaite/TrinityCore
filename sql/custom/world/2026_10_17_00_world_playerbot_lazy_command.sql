-- WoW Forever Bots: help for the player command .playerbot lazy (cs_playerbot_v2.cpp, the add-on's Lazy lock key)
DELETE FROM `command` WHERE `name` = 'playerbot lazy';
INSERT INTO `command` (`name`, `help`) VALUES
('playerbot lazy', 'Syntax: .playerbot lazy\nWarlock only. Casts the next spell of the one-button rotation on your target: armor and pet, one curse, Corruption, Immolate and Siphon Life kept up, Conflagrate, Shadow Bolt, Life Tap, Death Coil. Bind the "Lazy lock" key in the WoW Forever Bots add-on and press it again for every cast.');
