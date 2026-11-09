-- Health of WoW Forever NPCs as Blizzard's realm has it (the HpMulti the live 70862 client caches hold, Cache 2026-11-08).
-- Our rows had 1 for all of them, so the elites among them died like normal mobs (Incinerator Gar'im, Discord 2026-11-09:
-- WANTED: Incinerator Gar'im was not elite; live 3.0). Only rows still at 1 are changed; ones we tuned by hand stay.
-- 113 NPCs.
UPDATE `creature_template_difficulty` SET `HealthModifier`=0.6 WHERE `DifficultyID`=0 AND `HealthModifier`=1 AND `Entry` IN (255899);
UPDATE `creature_template_difficulty` SET `HealthModifier`=0.8 WHERE `DifficultyID`=0 AND `HealthModifier`=1 AND `Entry` IN (258331,271486);
UPDATE `creature_template_difficulty` SET `HealthModifier`=0.9 WHERE `DifficultyID`=0 AND `HealthModifier`=1 AND `Entry` IN (268624);
UPDATE `creature_template_difficulty` SET `HealthModifier`=1.02 WHERE `DifficultyID`=0 AND `HealthModifier`=1 AND `Entry` IN (253092,255769);
UPDATE `creature_template_difficulty` SET `HealthModifier`=1.05 WHERE `DifficultyID`=0 AND `HealthModifier`=1 AND `Entry` IN (256389,256390,256391,256736,256742,259859,260873,262399,271005,271006,272437,272646);
UPDATE `creature_template_difficulty` SET `HealthModifier`=1.1 WHERE `DifficultyID`=0 AND `HealthModifier`=1 AND `Entry` IN (248242,248248,248265,248277,256749,256801,257926,259352,267525,270310,272435);
UPDATE `creature_template_difficulty` SET `HealthModifier`=1.15 WHERE `DifficultyID`=0 AND `HealthModifier`=1 AND `Entry` IN (253272,259046,259269,259860,260070,262031,263089,263343,263348,263613,268663,269175,271167);
UPDATE `creature_template_difficulty` SET `HealthModifier`=1.2 WHERE `DifficultyID`=0 AND `HealthModifier`=1 AND `Entry` IN (258522,262473,262477,268558,270637,272228);
UPDATE `creature_template_difficulty` SET `HealthModifier`=1.25 WHERE `DifficultyID`=0 AND `HealthModifier`=1 AND `Entry` IN (257591,258023,258686,259051,259861,260146,260860,263611,263614,270570,276189);
UPDATE `creature_template_difficulty` SET `HealthModifier`=1.3 WHERE `DifficultyID`=0 AND `HealthModifier`=1 AND `Entry` IN (256438,259158,270269,272313);
UPDATE `creature_template_difficulty` SET `HealthModifier`=1.35 WHERE `DifficultyID`=0 AND `HealthModifier`=1 AND `Entry` IN (258785);
UPDATE `creature_template_difficulty` SET `HealthModifier`=1.4 WHERE `DifficultyID`=0 AND `HealthModifier`=1 AND `Entry` IN (260080);
UPDATE `creature_template_difficulty` SET `HealthModifier`=1.5 WHERE `DifficultyID`=0 AND `HealthModifier`=1 AND `Entry` IN (271012,271334,271587,276009);
UPDATE `creature_template_difficulty` SET `HealthModifier`=1.75 WHERE `DifficultyID`=0 AND `HealthModifier`=1 AND `Entry` IN (257558);
UPDATE `creature_template_difficulty` SET `HealthModifier`=2.0 WHERE `DifficultyID`=0 AND `HealthModifier`=1 AND `Entry` IN (256840,257080,260045,260047,260048,260049,260050,260051,260052,260054,260055,260056,260060,260061,260062,260066,260067,260068,260075,260079,267007,269689,269692,269693,271746);
UPDATE `creature_template_difficulty` SET `HealthModifier`=3.0 WHERE `DifficultyID`=0 AND `HealthModifier`=1 AND `Entry` IN (214519,257087,258878,258880,259353,270589,270892,271346,271348,271351);
UPDATE `creature_template_difficulty` SET `HealthModifier`=4.0 WHERE `DifficultyID`=0 AND `HealthModifier`=1 AND `Entry` IN (257079);
UPDATE `creature_template_difficulty` SET `HealthModifier`=5.0 WHERE `DifficultyID`=0 AND `HealthModifier`=1 AND `Entry` IN (269699,270571,271462);
UPDATE `creature_template_difficulty` SET `HealthModifier`=5.15 WHERE `DifficultyID`=0 AND `HealthModifier`=1 AND `Entry` IN (271742);
UPDATE `creature_template_difficulty` SET `HealthModifier`=5.5 WHERE `DifficultyID`=0 AND `HealthModifier`=1 AND `Entry` IN (269695);
UPDATE `creature_template_difficulty` SET `HealthModifier`=6.0 WHERE `DifficultyID`=0 AND `HealthModifier`=1 AND `Entry` IN (274757);
UPDATE `creature_template_difficulty` SET `HealthModifier`=50.0 WHERE `DifficultyID`=0 AND `HealthModifier`=1 AND `Entry` IN (263643);
