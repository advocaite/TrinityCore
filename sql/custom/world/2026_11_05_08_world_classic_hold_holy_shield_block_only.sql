-- Classic 1.60: Holy Shield (20925, 20927, 20928) held back as in 0.1.29 until the owner decides. 2026_10_25_92 made its Holy damage
-- happen on blocked attacks only, as the tooltip says; that lowers the damage it deals, so the rows go again and the core takes the client
-- proc data (normal and critical hits use a charge and deal the damage). Lava Burst is held back in the C++ (always a critical strike with
-- Flame Shock). To take the tooltip version, drop this file and run 2026_10_25_92 again. Safe to run again.
DELETE FROM `spell_proc` WHERE `SpellId` IN (20925, 20927, 20928);
