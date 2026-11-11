-- Blizzard hotfix 6 November 2026: "Searing Dagger activates only from spells with a damaging component."
-- Searing Dagger (273003, Bazzalan in Ragefire Chasm) carries Sear (1291568): on a harmful spell it fires Sear (1291570) for fire
-- damage. Ours had no spell_proc row, so any harmful spell set it off, damage or not (curses, Hunter's Mark, Faerie Fire...). The row
-- keeps the client's proc flags, chance and cooldown and adds the damage check (SpellTypeMask 1 = damage, phase 2 = on hit).
-- Safe to run again.
DELETE FROM `spell_proc` WHERE `SpellId`=1291568;
INSERT INTO `spell_proc` (`SpellId`,`SchoolMask`,`SpellFamilyName`,`SpellFamilyMask0`,`SpellFamilyMask1`,`SpellFamilyMask2`,`SpellFamilyMask3`,`ProcFlags`,`ProcFlags2`,`SpellTypeMask`,`SpellPhaseMask`,`HitMask`,`AttributesMask`,`DisableEffectsMask`,`ProcsPerMinute`,`Chance`,`Cooldown`,`Charges`) VALUES
(1291568,0,0,0,0,0,0,0,0,1,2,0,0,0,0,0,0,0);
