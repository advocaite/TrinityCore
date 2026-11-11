-- .aura self / .unaura self: the aura commands on the player who types them, whatever is targeted. Safe to run again.
DELETE FROM `command` WHERE `name` IN ('aura self', 'unaura self');
INSERT INTO `command` (`name`, `help`) VALUES
('aura self', 'Syntax: .aura self #spellid\r\n\r\nAdd the aura of spell #spellid to yourself, whatever you have targeted.'),
('unaura self', 'Syntax: .unaura self #spellid\r\n\r\nRemove the aura of spell #spellid from yourself, whatever you have targeted. Use "all" in place of #spellid to remove all your auras.');
