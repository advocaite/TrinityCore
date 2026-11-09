-- Class quests without a class mask (world 2026_10_27_94) let other classes finish them and keep the spells they teach: a
-- Windshaper Skyborne hunter got Bear Form from Strength and Mercy. Take those spells (and a shapeshift aura or action button
-- that uses them) from every character that is not of the quest's class. The spells are the ones the reward spells learn in
-- the 70630 SpellEffect.db2:
--   druid:  19179 Bear Form -> 5487 Bear Form, 6795 Growl, 6807 Maul; 1309050 Cat Form -> 768 Cat Form, 5215 Prowl, 1082 Claw,
--           1079 Rip; 1446 Aquatic Form -> 1066; 8947 Cure Poison -> 8946
--   shaman: 8073 Stoneskin Totem -> 8071
-- Re-runnable.
DELETE `cs` FROM `character_spell` `cs` JOIN `characters` `c` ON `c`.`guid` = `cs`.`guid`
WHERE (`c`.`class` <> 11 AND `cs`.`spell` IN (5487,6795,6807,768,5215,1082,1079,1066,8946))
   OR (`c`.`class` <> 7 AND `cs`.`spell` = 8071);

DELETE `ae` FROM `character_aura_effect` `ae` JOIN `characters` `c` ON `c`.`guid` = `ae`.`guid`
WHERE `c`.`class` <> 11 AND `ae`.`spell` IN (5487,768,1066,5215);
DELETE `a` FROM `character_aura` `a` JOIN `characters` `c` ON `c`.`guid` = `a`.`guid`
WHERE `c`.`class` <> 11 AND `a`.`spell` IN (5487,768,1066,5215);

DELETE `ca` FROM `character_action` `ca` JOIN `characters` `c` ON `c`.`guid` = `ca`.`guid`
WHERE `ca`.`type` = 0
  AND ((`c`.`class` <> 11 AND `ca`.`action` IN (5487,6795,6807,768,5215,1082,1079,1066,8946))
    OR (`c`.`class` <> 7 AND `ca`.`action` = 8071));
