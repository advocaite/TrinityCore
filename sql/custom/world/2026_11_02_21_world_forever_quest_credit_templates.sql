-- Two quest credits that never counted: the SmartAI line that gives the credit names a creature
-- entry that is not in creature_template, so the server drops that script line when it loads.
--   271912 "Tarantula Egg destroyed"  - Alther's Mill (98386), Tarantula Egg 673101 on use
--   252078 "Learn about the cultists' plans" - Wardrobe 254128 listen credit
-- Add invisible credit-only templates (not spawned anywhere). INSERT IGNORE: an existing row is left alone.
INSERT IGNORE INTO `creature_template` (`entry`,`KillCredit1`,`KillCredit2`,`name`,`faction`,`npcflag`,`unit_class`,`unit_flags`,`type`,`AIName`,`flags_extra`,`ScriptName`,`VerifiedBuild`) VALUES
(271912,0,0,'Tarantula Egg destroyed',35,0,1,33554432,10,'',128,'',0),
(252078,0,0,'Learn about the cultists'' plans',35,0,1,33554432,10,'',128,'',0);
