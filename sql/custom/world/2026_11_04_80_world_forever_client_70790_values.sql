-- WoW Forever client 1.60.1.70790 (Battle.net, 4 November 2026 18:49 UK) raised Chain Lightning, the shaman shocks and druid
-- Pounce back to vanilla level or higher. Batch 3 (2026_11_03_40) kept vanilla (VMaNGOS) value rows where
-- Forever was 30%+ lower; on 70790 these 22 effects are now at or above the vanilla value, so the vanilla row only held
-- players below live Forever. Dropping them gives the 70790 numbers (the server reads them from the 70790 game data).
-- Only rows where 70790 is equal or stronger are dropped: no spell gets weaker. Spells: Chain Lightning, Earth Shock, Flame Shock, Frost Shock, Pounce Bleed.
-- Undo: sql/reserve/forever-swap/04_undo_70790.sql. Safe to run again.
DELETE FROM `classic_spell_value` WHERE (`spell_id`,`effect_index`) IN ((421,0),(930,0),(2860,0),(8046,0),(8052,1),(8053,0),(8053,1),(8058,1),(9007,0),(9824,0),(9826,0),(10412,0),(10413,0),(10414,0),(10447,0),(10447,1),(10448,0),(10448,1),(10472,1),(10473,1),(29228,0),(29228,1));
