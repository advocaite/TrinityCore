-- Classic 1.60 client 70532: the party buffs last as long as the 70532 data and their tooltips say (1 hour, Battle Shout 3 minutes).
-- classic_spell_info still carried the vanilla durations, so Blessing of Kings showed "1 hour" but ran out after 5 minutes, the Greater Blessings
-- after 15 and Fortitude, Divine Spirit, Mark of the Wild and Arcane Intellect after 30. Every row below changed only the duration, so it goes and
-- the client value applies. Safe to run again.
DELETE FROM `classic_spell_info` WHERE `spell_id` IN (
-- Blessing of Might, Wisdom, Kings, Salvation, Light
19740, 19834, 19835, 19836, 19837, 19838, 25291,
19742, 19850, 19852, 19853, 19854, 25290,
20217, 1038,
19977, 19978, 19979,
-- Greater Blessing of Might, Wisdom, Kings, Salvation, Light
25782, 25916, 25894, 25918, 25898, 25895, 25890,
-- Mark of the Wild
1126, 5232, 5234, 6756, 8907, 9884, 9885,
-- Power Word: Fortitude, Divine Spirit
1243, 1244, 1245, 2791, 10937, 10938,
14752, 14818, 14819, 27841,
-- Arcane Intellect, and its Forever twin
1459, 1460, 1461, 10156, 10157, 364161,
-- Battle Shout
5242, 6192, 6673, 11549, 11550, 11551, 25289);
