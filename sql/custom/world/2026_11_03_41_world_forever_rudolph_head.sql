-- The Tarnished (91317, Forsaken paladin): Rudolph Gelhardt (259431, one spawn, Tarnished Paladin) carries the quest item 270435.
-- The sniff import gave it a guessed 40% chance (drop not seen in the sniffs), so most kills left nothing to loot.
-- A named quest target with a single spawn drops its quest item every time (still only while the quest needs it).
UPDATE `creature_loot_template` SET `Chance` = 100, `Comment` = 'Rudolph Gelhardt - quest item for The Tarnished (named target, always drops)'
WHERE `Entry` = 259431 AND `Item` = 270435 AND `ItemType` = 0;
