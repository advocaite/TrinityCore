-- Skyborne "listen to" quest steps that nothing gave credit for.
-- These objectives are kill credits on friendly NPCs (Forever uses a scripted talk), and nothing in our data gave them,
-- so the step stayed at 0/1 however long the player clicked through the NPC's dialogue (Discord: The Broken Construct).
--  93737 The Broken Construct  "Listen to what Riaani Nightwind has to say": her only option did nothing.
--  92595 The Windshapers       "Listen to Illaya":           credit at the end of her story (<Remain Silent>).
--  92596 The High Order        "Listen to Rathiril Sunlance": credit at the end of his story (<Remain Silent>).
--  93159 The Strange Hermit    "Learn more about the Strange Hermit": credit on "What are all of these contraptions?".
--  92679 Blood Tithe           "Listen to Alvarion Windfield's Story": no story option in the sniffs; credit on accept or on talking to him.
--  92708/92709 A Grand Adventure "Listen to Ayessa / Elaadrin": credit 273972 had no creature_template, so nothing could give it;
--                              no story option in the sniffs; credit on accept or on talking to them.
-- Talking credit also covers players who already have the quest.

-- 273972 "Credit" (kill credit for A Grand Adventure), same shape as the other Zephras credit dummies (256634)
DELETE FROM `creature_template` WHERE `entry`=273972;
INSERT INTO `creature_template` (`entry`,`name`,`faction`,`speed_walk`,`speed_run`,`scale`,`BaseAttackTime`,`RangeAttackTime`,`BaseVariance`,`RangeVariance`,`unit_class`,`unit_flags`,`unit_flags2`,`type`,`AIName`,`ExperienceModifier`,`RegenHealth`,`ScriptName`,`VerifiedBuild`) VALUES
(273972,'Credit',35,1,0.992063,1,2000,2000,1,1,1,33555200,2048,10,'',1,1,'',70124);
DELETE FROM `creature_template_difficulty` WHERE `Entry`=273972;
INSERT INTO `creature_template_difficulty` (`Entry`,`DifficultyID`,`HealthModifier`,`ManaModifier`,`ArmorModifier`,`DamageModifier`,`CreatureDifficultyID`,`TypeFlags`,`VerifiedBuild`) VALUES
(273972,0,1,1,1,1,0,1024,70124);
DELETE FROM `creature_template_model` WHERE `CreatureID`=273972;
INSERT INTO `creature_template_model` (`CreatureID`,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`) VALUES
(273972,0,1126,1,0,70124),
(273972,1,15435,1,1,70124);

UPDATE `creature_template` SET `AIName`='SmartAI' WHERE `entry` IN (251684,251902,251903,251968,252448,252475,256083) AND `ScriptName`='';
DELETE FROM `smart_scripts` WHERE `entryorguid` IN (251684,251902,251903,251968,252448,252475,256083) AND `source_type`=0 AND `id` IN (50,51);
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(256083,0,50,51,62,0,100,0,42848,0,0,0,33,256083,0,0,0,0,0,7,0,0,0,0,0,0,0,'Riaani Nightwind - On Gossip Select - Quest Credit Listen to Riaani (93737)'),
(256083,0,51,0,61,0,100,0,0,0,0,0,72,0,0,0,0,0,0,7,0,0,0,0,0,0,0,'Riaani Nightwind - Linked - Close Gossip'),
(251902,0,50,0,62,0,100,0,40580,0,0,0,33,251902,0,0,0,0,0,7,0,0,0,0,0,0,0,'Illaya Amberwind - On Gossip Select <Remain Silent> - Quest Credit Listen to Illaya (92595)'),
(251903,0,50,0,62,0,100,0,40720,0,0,0,33,251903,0,0,0,0,0,7,0,0,0,0,0,0,0,'Rathiril Sunlance - On Gossip Select <Remain Silent> - Quest Credit Listen to Rathiril (92596)'),
(251684,0,50,0,62,0,100,0,40520,0,0,0,33,251684,0,0,0,0,0,7,0,0,0,0,0,0,0,'Strange Hermit - On Gossip Select contraptions - Quest Credit Learn more (93159)'),
(252448,0,50,0,19,0,100,0,92679,0,0,0,33,252448,0,0,0,0,0,7,0,0,0,0,0,0,0,'Alvarion Windfield - On Quest Accept - Quest Credit Listen to Alvarion (92679)'),
(252448,0,51,0,64,0,100,0,0,0,0,0,33,252448,0,0,0,0,0,7,0,0,0,0,0,0,0,'Alvarion Windfield - On Gossip Hello - Quest Credit Listen to Alvarion (92679)'),
(251968,0,50,0,19,0,100,0,92708,0,0,0,33,273972,0,0,0,0,0,7,0,0,0,0,0,0,0,'Ayessa Dawnsinger - On Quest Accept - Quest Credit Listen to Ayessa (92708)'),
(251968,0,51,0,64,0,100,0,0,0,0,0,33,273972,0,0,0,0,0,7,0,0,0,0,0,0,0,'Ayessa Dawnsinger - On Gossip Hello - Quest Credit Listen to Ayessa (92708)'),
(252475,0,50,0,19,0,100,0,92709,0,0,0,33,273972,0,0,0,0,0,7,0,0,0,0,0,0,0,'Elaadrin Evengale - On Quest Accept - Quest Credit Listen to Elaadrin (92709)'),
(252475,0,51,0,64,0,100,0,0,0,0,0,33,273972,0,0,0,0,0,7,0,0,0,0,0,0,0,'Elaadrin Evengale - On Gossip Hello - Quest Credit Listen to Elaadrin (92709)');
