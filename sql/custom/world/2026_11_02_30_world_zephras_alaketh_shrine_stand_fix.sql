-- Shrine of Akir (Zephras Isle, map 2991): Al'Aketh cultists stood frozen and never moved, even when pulled (reported 2026-11-02).
--
-- 1. The sniff import 2026_10_01_03 gave Al'Aketh Skypriest (252764) and Al'Aketh Honor Guard (252767) the template aura 445187,
--    a copy of Permanent Feign Death (29266, same as 1288546), taken from sniffed corpses. Every living Skypriest and Honor Guard
--    kept it, so they could be hit but could not move or chase. Same import bug as the Valanaar Peacekeepers (2026_10_05_04,
--    2026_10_28_02). The newer official sniffs (builds 70245-70425; Azeroth project, Sakhwoow/WoWForever, GPL-2) list no
--    permanent aura for either NPC. Real corpse NPCs keep theirs (Al'Aketh Attacker 253782/253787, Dead Cultist 253372).
UPDATE `creature_template_addon` SET `auras` = '' WHERE `entry` IN (252764,252767) AND `auras` = '445187';

-- 2. Seven spots have two cultists standing on exactly the same point (the sniff saw a different cultist there at different
--    times; 43000270 came in with 2026_10_27_70, which only skipped spots next to an NPC of the same name). Both fought at once
--    and looked like one stuck model. Each spot becomes a one-of-two pool, so one cultist stands there.
DELETE FROM `pool_members` WHERE `type` = 0 AND `spawnId` IN
(21101026,21101057,21101081,21101140,21101083,21101141,21101085,21101142,21101087,21101130,21101093,21101127,21101126,43000270);
DELETE FROM `pool_template` WHERE `entry` BETWEEN 1400301 AND 1400307;
INSERT INTO `pool_template` (`entry`,`max_limit`,`description`) VALUES
(1400301,1,'Zephyris - Al''Aketh Guardian or Spiritcaller (3050.7 790.3)'),
(1400302,1,'Zephyris - Al''Aketh Skypriest or Honor Guard (3056.8 756.1)'),
(1400303,1,'Zephyris - Al''Aketh Skypriest or Honor Guard (2933.2 656.2)'),
(1400304,1,'Zephyris - Al''Aketh Skypriest or Honor Guard (2962.8 632.7)'),
(1400305,1,'Zephyris - Al''Aketh Skypriest or Honor Guard (3090.7 609.2)'),
(1400306,1,'Zephyris - Al''Aketh Skypriest or Honor Guard (2995.9 751.9)'),
(1400307,1,'Zephyris - Al''Aketh Honor Guard or Skypriest (3063.1 799.0)');
INSERT INTO `pool_members` (`type`,`spawnId`,`poolSpawnId`,`chance`,`description`) VALUES
(0,21101026,1400301,0,'Al''Aketh Guardian'),
(0,21101057,1400301,0,'Al''Aketh Spiritcaller'),
(0,21101081,1400302,0,'Al''Aketh Skypriest'),
(0,21101140,1400302,0,'Al''Aketh Honor Guard'),
(0,21101083,1400303,0,'Al''Aketh Skypriest'),
(0,21101141,1400303,0,'Al''Aketh Honor Guard'),
(0,21101085,1400304,0,'Al''Aketh Skypriest'),
(0,21101142,1400304,0,'Al''Aketh Honor Guard'),
(0,21101087,1400305,0,'Al''Aketh Skypriest'),
(0,21101130,1400305,0,'Al''Aketh Honor Guard'),
(0,21101093,1400306,0,'Al''Aketh Skypriest'),
(0,21101127,1400306,0,'Al''Aketh Honor Guard'),
(0,21101126,1400307,0,'Al''Aketh Honor Guard'),
(0,43000270,1400307,0,'Al''Aketh Skypriest');
