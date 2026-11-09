-- WoW Forever client 1.60.1.70790 (Battle.net, 4 November 2026 18:49 UK) changed 10 spell records that 2026_11_03_41 sends as
-- hotfixes with their 70707/70755 values. A hotfix row wins over the game data, so without this a 70790 player would keep the
-- older numbers. These rows now carry the 70790 values (the hotfix_data entries of 2026_11_03_41 stay, so every client gets them):
--   Ferocious Bite (1328618): effect 0 base 14 -> 17 and per combo point 24 -> 30, effect 1 base 60 -> 70.
--   Chain Lightning (421, 930, 2860, 10605): cast time index 5 -> 19.
--   Sap (2070, 6770, 11297): Attributes6 32 -> 0.
--   Venom (1310703, replaced by talent 1329328 in 70790): its 70790 misc record.
-- Safe to run again.
UPDATE `spell_effect` SET `EffectPointsPerResource` = 30, `EffectBasePoints` = 17 WHERE `ID` = 1367722;
UPDATE `spell_effect` SET `EffectBasePoints` = 70 WHERE `ID` = 1367723;
UPDATE `spell_misc` SET `CastingTimeIndex` = 19 WHERE `ID` IN (314274, 314381, 316208, 322672);
UPDATE `spell_misc` SET `Attributes6` = 0 WHERE `ID` IN (316193, 319142, 324065);
UPDATE `spell_misc` SET `Attributes1` = 262160, `Attributes2` = 0, `Attributes3` = 0, `Attributes4` = 1073741824, `Attributes5` = 0, `DurationIndex` = 32, `RangeIndex` = 1 WHERE `ID` = 857291;
