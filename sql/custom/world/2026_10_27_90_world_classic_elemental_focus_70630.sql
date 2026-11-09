-- Elemental Focus (16164): client 70630 moved the 10% chance into the effect value (ProcChance 10 -> 100, s1 0 -> 10).
-- Chance 0 here used the client chance, so with 70630 data every Fire, Frost or Nature spell would give Clearcasting.
-- Set the chance itself to Forever's 10%. Same result on 70532 data. Safe to run again.
UPDATE `spell_proc` SET `Chance`=10 WHERE `SpellId`=16164;
