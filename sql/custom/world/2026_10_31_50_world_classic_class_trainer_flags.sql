-- Classic 1.60: 12 class trainers (Zephras Isle and Forever) have a class trainer list but no trainer flags in the base data
-- (npcflag 16 Trainer, 32 Class trainer), so the client shows no train option, and their trainer_class is 0, so they never offer
-- the talent reset of their class (Creature::CanResetTalents). Set both flags and the class id
-- (warrior 1, paladin 2, hunter 3, rogue 4, shaman 7, mage 8, druid 11).
-- The flags were already set by 2026_10_27_00 and 2026_10_27_01; this file gives them a source of their own and adds the class id.
-- From Trixx34 (Trixx34/Trinity-Forever-Modules, commit 2a5bcf39, GPL-2.0). Safe to run again.
UPDATE `creature_template` SET `npcflag` = `npcflag` | 48, `trainer_class` = 1 WHERE `entry` IN (252377, 254088);  -- Seena Skybreaker, Corsan Earthrazer (Warrior)
UPDATE `creature_template` SET `npcflag` = `npcflag` | 48, `trainer_class` = 4 WHERE `entry` IN (252379, 254087);  -- Eltheen Nightbreeze, Miriaan Mistblade (Rogue)
UPDATE `creature_template` SET `npcflag` = `npcflag` | 48, `trainer_class` = 8 WHERE `entry` IN (252373, 254086);  -- Anathamaas Aetherwind, Shenaan Spellwind (Mage)
UPDATE `creature_template` SET `npcflag` = `npcflag` | 48, `trainer_class` = 3 WHERE `entry` IN (248415, 254084);  -- Tordrin Sternblade, Elayaa Easewind (Hunter)
UPDATE `creature_template` SET `npcflag` = `npcflag` | 48, `trainer_class` = 7 WHERE `entry` IN (252382, 254082);  -- Sessaria Skystride, Aarnor Galestrike (Shaman)
UPDATE `creature_template` SET `npcflag` = `npcflag` | 48, `trainer_class` = 2 WHERE `entry` = 246389;               -- Hilda the Breaker (Paladin)
UPDATE `creature_template` SET `npcflag` = `npcflag` | 48, `trainer_class` = 11 WHERE `entry` = 252359;              -- Lotheluum Starbreeze (Druid)
