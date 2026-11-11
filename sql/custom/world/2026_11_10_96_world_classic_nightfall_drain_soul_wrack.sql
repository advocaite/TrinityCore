-- Classic 1.60 Nightfall (18094): the client tooltip names Corruption, Drain Soul, Drain Life and Wrack. The proc row only had Corruption
-- (warlock family flag [0] 0x2) and Drain Life ([0] 0x8); add Drain Soul ([0] 0x4000) and Wrack 1316697 ([1] 0x40000). Safe to run again.
UPDATE `spell_proc` SET `SpellFamilyMask0` = 0x400A, `SpellFamilyMask1` = 0x40000 WHERE `SpellId` = 18094;
