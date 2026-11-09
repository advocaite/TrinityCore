-- Quest pickup objects (chests) that came into our world with the old 1.12 "consumable" flag in Data3. This client's chest layout
-- no longer reads Data3 (Unused); the server takes "used up when looted" from Chest Flags (Data20, bit 0x1), which was 0, so a
-- looted object went straight back to ready and could be looted again and again (Discord player report, Blackrock Blockade,
-- fixed for its 5 objects in 2026_11_04_95). Every official-sniff chest with consumable behaviour has Data20 bit 0x1.
-- This sets it on the other 31 objects found on the realm DB with Data3 = 1 and no Consumable chest flag: they now despawn when
-- looted and come back on their spawn timer. Only rows still in that state are touched. Safe to run again.
UPDATE `gameobject_template` SET `Data20` = `Data20` | 1
WHERE `type` = 3 AND `Data3` = 1 AND (`Data20` & 1) = 0 AND `entry` IN (404941,561159,562103,562105,562106,616860,640104,656180,656181,656182,660647,660930,665282,670771,670773,670774,672821,673093,673094,673494,673496,673497,673515,693669,693670,693674,694785,694855,695222,695223,695277);
