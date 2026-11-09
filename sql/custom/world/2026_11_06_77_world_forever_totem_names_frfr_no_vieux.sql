-- French names of old totem ranks carry the data prefix [VIEUX] (the frFR form of zzOLD), so the totem showed it over its head. Remove it from these totem NPCs. Safe to run again.
UPDATE `creature_template_locale` SET `Name` = TRIM(SUBSTRING(`Name`, 8)) WHERE `entry` IN (5879,6110,6111,7844,7845,15482,30652,30653,32775,32776) AND `locale` = 'frFR' AND `Name` LIKE '[VIEUX]%';
