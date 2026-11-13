-- Seal of Fury dealt its Holy damage twice on every melee hit (Discord 2026-11-12, dumpins: "Your Seal of Fury hit Vampiric
-- Duskbat 6 Holy" twice per swing). The Forever seal has two dummy effects that name the damage spell as their trigger spell:
-- effect 0 (the proc, handled by classic_spell_pal_seal_of_fury) and effect 2, whose base points hold the seal's Judgement of Fury
-- (read by the seal table in SpellInfo.cpp). A dummy aura with a trigger spell procs that spell by default, so effect 2 cast the
-- damage a second time (and on rank 2 its trigger is Judgement of Fury 20411 itself). Effect 2 never procs now.
-- Safe to run again.
UPDATE `spell_proc` SET `DisableEffectsMask` = `DisableEffectsMask` | 0x4 WHERE `SpellId` IN (1311649,1311656,20163,20419,20421,20422,20423);
