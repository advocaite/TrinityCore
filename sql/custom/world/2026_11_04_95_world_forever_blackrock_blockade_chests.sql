-- Blackrock Blockade (98387, Render's Valley, Redridge): the Meat Haunch, Water Barrel, Grain Sack, Weapon Rack and Stolen Weapon
-- objects (673376, 673384, 673385, 673390, 673399) could be looted again and again (Discord player report 2026-11-04).
-- Their template rows carry the old 1.12 layout's "consumable" in data3, which this client's chest layout no longer uses
-- (data3 is Unused). The server reads "consumable" from Chest Flags (data20, bit 0x1), which was 0, so a looted object went
-- straight back to ready. Set the Consumable chest flag: a looted object now despawns and comes back on its spawn timer (300 s).
-- Safe to run again.
UPDATE `gameobject_template` SET `Data20` = `Data20` | 1
WHERE `entry` IN (673376, 673384, 673385, 673390, 673399) AND `type` = 3;
