-- Undo of 02_world_drop_vanilla_holds.sql: puts the vanilla value rows back (as they were in 0.1.26). Safe to run again.
DELETE FROM `classic_spell_value` WHERE `spell_id` IN (13165,14318,14319,14320,14321,3166,11069);
/*M!999999\- enable the sandbox mode */ 
INSERT INTO `classic_spell_value` VALUES (3166,0,6,29,5,1,0,3,0,0,'Lesser Intellect');
INSERT INTO `classic_spell_value` VALUES (11069,0,6,107,-101,1,0,10,0,0,'Improved Fireball');
INSERT INTO `classic_spell_value` VALUES (13165,0,6,124,19,1,0,0,0,0,'Aspect of the Hawk');
INSERT INTO `classic_spell_value` VALUES (14318,0,6,124,34,1,0,0,0,0,'Aspect of the Hawk');
INSERT INTO `classic_spell_value` VALUES (14319,0,6,124,49,1,0,0,0,0,'Aspect of the Hawk');
INSERT INTO `classic_spell_value` VALUES (14320,0,6,124,69,1,0,0,0,0,'Aspect of the Hawk');
INSERT INTO `classic_spell_value` VALUES (14321,0,6,124,89,1,0,0,0,0,'Aspect of the Hawk');
