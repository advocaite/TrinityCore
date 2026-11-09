-- WoW Forever quests, rebuilt only from what we hold ourselves: our saved Wowhead WoW Forever quest pages and the 70685
-- client data (ItemSparse, SpellName, Faction, QuestXP). Replaces these quests' rows from 2026_10_26_31/_32/_37/_61/_64/_65.
-- Only quests whose giver, turn-in, kill targets and objective items already exist in our world are kept (20). Quests that
-- need an NPC or object we can't build (no display id / template from our own data), a Forever item with no source in our
-- data, or a previous quest we don't have are left out.
-- INFERRED: races = every race of the page's side; class quests (QuestSort of a class) limited to that class; previous quest
-- from the page's series list (A Friend of the Family: Return the Statuette 98189); paragraph breaks restored at sentence ends.
-- CHECK in game: To Darnassus (98397) teaches Cat Form 1309050, picked from several client spells of the same name; Remember That I Love You (95161) has no Tarnished Locket reward (item not in the client);
-- Blessings of the Great Cat Spirit (98731) and Taming the Beast (94007) have no previous quest set (series list unclear).

-- 65601 Love Hurts (level 20, required 20) -- Wowhead Forever page
DELETE FROM `quest_template` WHERE `ID`=65601;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyMultiplier`,`RewardSpell`,`StartItem`,`Flags`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`) VALUES (65601,2,-61,2,1,1,0,0,8,8589934770,'Love Hurts','Speak with Magar in Orgrimmar.','A sayaad male is known as an incubus. A creature of profound evil and singular mind.$B$BThe demon must be lured into our world with an item of enticement - an object that represents great love and devotion.$B$BIf you trust a blind man to lead you, $n, then listen. East from the Cleft of Shadow, along the main drag of Orgrimmar, lives Magar. He longs for the return of his mate, Krilge.$B$BSeek him out. He''ll ask you to search for something precious. That is the key which you must bring to Gan''rul.','','');
DELETE FROM `quest_objectives` WHERE `QuestID`=65601;
DELETE FROM `quest_template_addon` WHERE `ID`=65601;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (65601,0,256,0);
DELETE FROM `quest_request_items` WHERE `ID`=65601;
DELETE FROM `quest_offer_reward` WHERE `ID`=65601;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (65601,'Who told you about Krilge? Have you come with any news? You better not be toying with me...',0);
DELETE FROM `quest_classic_money` WHERE `ID`=65601;
DELETE FROM `quest_classic_level` WHERE `ID`=65601;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (65601,20,20,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=65601;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (65601,2,20);
DELETE FROM `creature_queststarter` WHERE `quest`=65601;
DELETE FROM `gameobject_queststarter` WHERE `quest`=65601;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (5909,65601,0);
DELETE FROM `creature_questender` WHERE `quest`=65601;
DELETE FROM `gameobject_questender` WHERE `quest`=65601;
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (3363,65601,0);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry` IN (3363,5909);

-- 91858 Diplomatic Incident (level 22, required 18) -- Wowhead Forever page
DELETE FROM `quest_template` WHERE `ID`=91858;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyMultiplier`,`RewardSpell`,`StartItem`,`Flags`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`RewardFactionID1`,`RewardFactionOverride1`) VALUES (91858,2,-141,2,1,1,0,0,0,8589934770,'Diplomatic Incident','Travel to The Sepulcher in Silverpine Forest and speak with Trevan Rol about the missing Earthen Ring travelers.','A most unfortunate turn of events in Silverpine has presented a unique opportunity for us to prove our worth in the eyes of the horde.$B$BA pair of dignitaries from the Earthen Ring went missing in Silverpine Forest en route to Tarren Mill. We don''t know much else but rumors say that this is becoming quite the diplomatic incident. If we can recover these missing persons, it could help gain us the legitimacy we so dearly need.$B$BWe have a man at The Sepulcher, Trevan Rol. Travel there and speak with him.','','',68,5000);
DELETE FROM `quest_objectives` WHERE `QuestID`=91858;
DELETE FROM `quest_template_addon` WHERE `ID`=91858;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (91858,0,2,0);
DELETE FROM `quest_request_items` WHERE `ID`=91858;
DELETE FROM `quest_offer_reward` WHERE `ID`=91858;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (91858,'Danitha sent you? Well in that case I already know why you are here. You just walked into a damned fine mess, $c.',0);
DELETE FROM `quest_classic_money` WHERE `ID`=91858;
INSERT INTO `quest_classic_money` (`ID`,`RewardMoney`,`RewardMoneyMaxLevel`) VALUES (91858,350,0);
DELETE FROM `quest_classic_level` WHERE `ID`=91858;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (91858,22,18,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=91858;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (91858,2,22);
DELETE FROM `creature_queststarter` WHERE `quest`=91858;
DELETE FROM `gameobject_queststarter` WHERE `quest`=91858;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (246378,91858,0);
DELETE FROM `creature_questender` WHERE `quest`=91858;
DELETE FROM `gameobject_questender` WHERE `quest`=91858;
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (248840,91858,0);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry` IN (246378,248840);

-- 91859 A Curious Pair (level 22, required 18) -- Wowhead Forever page
DELETE FROM `quest_template` WHERE `ID`=91859;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyMultiplier`,`RewardSpell`,`StartItem`,`Flags`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`RewardFactionID1`,`RewardFactionOverride1`) VALUES (91859,2,-141,2,1,1,0,0,0,8589934770,'A Curious Pair','Speak with Deathguard Baldren about the mysterious Earthen Ring travelers who recently passed through The Sepulcher.','Cutting straight to it, these "dignitaries" are no ordinary diplomats. One was a Tauren. Ordinary enough for the Earthen Ring, but the other--and you''ll scarcely believe this--was an elf. A *high* elf. They were cloaked, but I''ve seen quel''dorei before and I''d bet my good pinky finger that''s what I saw.$B$BWhy was an elf accompanying a Tauren through horde territory? Well, your guess is as good as mine. That''s as much as I know, so I''d suggest speaking with Deathguard Baldren near the entrance to town.','','',68,5000);
DELETE FROM `quest_objectives` WHERE `QuestID`=91859;
DELETE FROM `quest_template_addon` WHERE `ID`=91859;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (91859,0,2,91858);
DELETE FROM `quest_request_items` WHERE `ID`=91859;
DELETE FROM `quest_offer_reward` WHERE `ID`=91859;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (91859,'I might not care for you paladins, but I will say that your timing is impeccable. It''s better to be lucky than good, I suppose?',0);
DELETE FROM `quest_classic_money` WHERE `ID`=91859;
INSERT INTO `quest_classic_money` (`ID`,`RewardMoney`,`RewardMoneyMaxLevel`) VALUES (91859,350,0);
DELETE FROM `quest_classic_level` WHERE `ID`=91859;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (91859,22,18,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=91859;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (91859,2,22);
DELETE FROM `creature_queststarter` WHERE `quest`=91859;
DELETE FROM `gameobject_queststarter` WHERE `quest`=91859;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (248840,91859,0);
DELETE FROM `creature_questender` WHERE `quest`=91859;
DELETE FROM `gameobject_questender` WHERE `quest`=91859;
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (259611,91859,0);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry` IN (248840,259611);

-- 93791 Speak with Belann (level 10, required 10) -- Wowhead Forever page
DELETE FROM `quest_template` WHERE `ID`=93791;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyMultiplier`,`RewardSpell`,`StartItem`,`Flags`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`RewardFactionID1`,`RewardFactionOverride1`,`RewardFactionID2`,`RewardFactionOverride2`) VALUES (93791,2,-161,3,1,1,0,0,8,4294967373,'Speak with Belann','Speak with Belann Windwood in Valanaar.','$n, it is clear your skill with magic is growing and I believe it is time for you to partake in one of the High Order''s oldest traditions for young and aspiring mages. It is time for you to craft a spell focus of your own.$B$BSeek out Belann Windwood in Valanaar. He has been crafting staves and wands for the High Order for over a century, so you will be in good hands.','','',2778,5000,2779,5000);
DELETE FROM `quest_objectives` WHERE `QuestID`=93791;
DELETE FROM `quest_template_addon` WHERE `ID`=93791;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (93791,0,128,0);
DELETE FROM `quest_request_items` WHERE `ID`=93791;
DELETE FROM `quest_offer_reward` WHERE `ID`=93791;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (93791,'Ah, you must be $n? It is heartening to see young mages eager to learn our ways and history. Come, I will explain the next steps to crafting your spell focus.',0);
DELETE FROM `quest_classic_money` WHERE `ID`=93791;
DELETE FROM `quest_classic_level` WHERE `ID`=93791;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (93791,10,10,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=93791;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (93791,2,10);
DELETE FROM `creature_queststarter` WHERE `quest`=93791;
DELETE FROM `gameobject_queststarter` WHERE `quest`=93791;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (252373,93791,0);
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (256507,93791,0);
DELETE FROM `creature_questender` WHERE `quest`=93791;
DELETE FROM `gameobject_questender` WHERE `quest`=93791;
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (256507,93791,0);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry` IN (252373,256507);

-- 94007 Taming the Beast (level 10, required 10) -- Wowhead Forever page
DELETE FROM `quest_template` WHERE `ID`=94007;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyMultiplier`,`RewardSpell`,`StartItem`,`Flags`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`) VALUES (94007,2,-261,2,1,1,0,0,0,0,'Taming the Beast','Report to','Now that you''ve gained a level of comfort in the wilds of the highlands, I believe it is time for you to seek your very own animal companion.$B$BGo to the port of Valanaar to the southeast, and speak with Quel''ana Quickgale there. He will instruct you in the art of beast taming.','','');
DELETE FROM `quest_objectives` WHERE `QuestID`=94007;
DELETE FROM `quest_template_addon` WHERE `ID`=94007;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (94007,0,4,0);
DELETE FROM `quest_request_items` WHERE `ID`=94007;
DELETE FROM `quest_offer_reward` WHERE `ID`=94007;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (94007,'I am most glad that you came. Come, let us begin.',0);
DELETE FROM `quest_classic_money` WHERE `ID`=94007;
DELETE FROM `quest_classic_level` WHERE `ID`=94007;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (94007,10,10,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=94007;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (94007,2,10);
DELETE FROM `creature_queststarter` WHERE `quest`=94007;
DELETE FROM `gameobject_queststarter` WHERE `quest`=94007;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (254084,94007,0);
DELETE FROM `creature_questender` WHERE `quest`=94007;
DELETE FROM `gameobject_questender` WHERE `quest`=94007;
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (252389,94007,0);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry` IN (252389,254084);

-- 94914 Moonglade (level 10, required 10) -- Wowhead Forever page
DELETE FROM `quest_template` WHERE `ID`=94914;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyMultiplier`,`RewardSpell`,`StartItem`,`Flags`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`RewardFactionID1`,`RewardFactionOverride1`) VALUES (94914,2,-263,2,1,1,0,0,0,4294967373,'Moonglade','Use the spell "Teleport: Moonglade" to travel to Moonglade. When you arrive, speak with Dendrite Starblaze in the vil','All druids are welcomed to the sacred lands of Moonglade. Keeper Remulos--Moonglade''s guardian--keeps watch over all druids who hibernate there. I''ll teach you the spell that lets you transport yourself there. Cast it whenever you seek Moonglade''s knowledge or solace.$B$BAs for your first lesson here on Azeroth, travel to Moonglade and speak with Dendrite Starblaze in Nighthaven about the path you''re now on. Good luck, young druid.','','',609,2500);
DELETE FROM `quest_objectives` WHERE `QuestID`=94914;
DELETE FROM `quest_template_addon` WHERE `ID`=94914;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (94914,0,1024,0);
DELETE FROM `quest_request_items` WHERE `ID`=94914;
DELETE FROM `quest_offer_reward` WHERE `ID`=94914;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (94914,'Ah I see that you have already mastered the form of the Bear. Good, good. You are well on your way, young $c.',0);
DELETE FROM `quest_classic_money` WHERE `ID`=94914;
DELETE FROM `quest_classic_level` WHERE `ID`=94914;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (94914,10,10,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=94914;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (94914,2,10);
DELETE FROM `creature_queststarter` WHERE `quest`=94914;
DELETE FROM `gameobject_queststarter` WHERE `quest`=94914;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (5504,94914,0);
DELETE FROM `creature_questender` WHERE `quest`=94914;
DELETE FROM `gameobject_questender` WHERE `quest`=94914;
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (11802,94914,0);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry` IN (5504,11802);

-- 95161 Remember That I Love You (level 22, required 15) -- Wowhead Forever page
DELETE FROM `quest_template` WHERE `ID`=95161;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyMultiplier`,`RewardSpell`,`StartItem`,`Flags`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`) VALUES (95161,2,16591,1,1,1,0,251522,0,4294967373,'Remember That I Love You','Deliver the Blood-Stained Letter to Avette Fellwood in Duskwood.','If you''re looking for Jeremy Heartweaver, your best bet is to talk to his newly adopted mother, Avette Fellwood out in Duskwood. Sweet woman. Likely needs another pair of hands for her work, but it beats staying in the orphanage.$B$BIf you see little Jeremy, do be sure to tell him we''re all thinking of him, won''t you?','','');
DELETE FROM `quest_objectives` WHERE `QuestID`=95161;
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`Flags`,`Visible`,`Description`,`VerifiedBuild`) VALUES (9516100,95161,1,0,0,251522,1,0,1,'Blood-Stained Letter',0);
DELETE FROM `quest_template_addon` WHERE `ID`=95161;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (95161,0,0,0);
DELETE FROM `quest_request_items` WHERE `ID`=95161;
INSERT INTO `quest_request_items` (`ID`,`CompletionText`,`VerifiedBuild`) VALUES (95161,'Yes? What do you need?',0);
DELETE FROM `quest_offer_reward` WHERE `ID`=95161;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (95161,'Oh no...$B$BIt''s nice of you to bring this, but I don''t think Jeremy is ready to see it. Matron Nightingale said my little Jeremy hasn''t spoken since the attack on Lordaeron, and I don''t intend to force him now. Showing him this will only cause him more pain.$B$BDon''t look at me like that. I won''t hide his past from him, but he needs time to heal. I''ll give this to him when he''s older. Thank you for stopping by, but I think it''s time you should go.',0);
DELETE FROM `quest_classic_money` WHERE `ID`=95161;
DELETE FROM `quest_classic_level` WHERE `ID`=95161;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (95161,22,15,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=95161;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (95161,2,22);
DELETE FROM `creature_queststarter` WHERE `quest`=95161;
DELETE FROM `gameobject_queststarter` WHERE `quest`=95161;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (14450,95161,0);
DELETE FROM `creature_questender` WHERE `quest`=95161;
DELETE FROM `gameobject_questender` WHERE `quest`=95161;
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (228,95161,0);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry` IN (228,14450);

-- 98046 Crown of the Earth (level 11, required 1) -- Wowhead Forever page
DELETE FROM `quest_template` WHERE `ID`=98046;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyMultiplier`,`RewardSpell`,`StartItem`,`Flags`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`RewardFactionID1`,`RewardFactionOverride1`) VALUES (98046,2,141,1,1,1,0,279291,0,4294967373,'Crown of the Earth','Bring the Drained Vessel to Lariia in the Temple of the Moon in Darnassus.','<Arch Druid Fandral Staghelm''s gaze narrows.>$B$BYou''re still here?$B$BTake the phial and get out of my sight, $c. It is clear my answers do not lie in the waters, just as they did not before. $B$BPerhaps one of the priestesses in the Temple of the Moon to the south will dispose of it.$B$B<Fandral looks away, no longer speaking to you.>$B$BI must look elsewhere if this is to work...','','',69,15000);
DELETE FROM `quest_objectives` WHERE `QuestID`=98046;
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`Flags`,`Visible`,`Description`,`VerifiedBuild`) VALUES (9804600,98046,1,0,0,279291,1,0,1,'Drained Vessel',0);
DELETE FROM `quest_template_addon` WHERE `ID`=98046;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (98046,0,0,0);
DELETE FROM `quest_request_items` WHERE `ID`=98046;
INSERT INTO `quest_request_items` (`ID`,`CompletionText`,`VerifiedBuild`) VALUES (98046,'How may I aid you?',0);
DELETE FROM `quest_offer_reward` WHERE `ID`=98046;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (98046,'Do not mind the Arch Druid, $n. He bears the weight of much in Malfurion''s absence.$B$BWait! Some drops yet remain in the phial! Perhaps that is why his incantation failed?',0);
DELETE FROM `quest_classic_money` WHERE `ID`=98046;
DELETE FROM `quest_classic_level` WHERE `ID`=98046;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (98046,11,1,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=98046;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (98046,2,11);
DELETE FROM `creature_queststarter` WHERE `quest`=98046;
DELETE FROM `gameobject_queststarter` WHERE `quest`=98046;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (3516,98046,0);
DELETE FROM `creature_questender` WHERE `quest`=98046;
DELETE FROM `gameobject_questender` WHERE `quest`=98046;
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (4092,98046,0);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry` IN (3516,4092);

-- 98065 Crown of the Earth (level 11, required 1) -- Wowhead Forever page
DELETE FROM `quest_template` WHERE `ID`=98065;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyMultiplier`,`RewardSpell`,`StartItem`,`Flags`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`RewardFactionID1`,`RewardFactionOverride1`) VALUES (98065,2,141,1,1,1,0,279331,0,4294967373,'Crown of the Earth','Bring the Moonwell Remnants to Tyrande Whisperwind in the Temple of the Moon in Darnassus.','You must bring this to Tyrande at once! I will transfer it to a more ceremonial container for you.$B$BDo not be afraid. The High Priestess of Elune is firm, but kind. She protects all who enter Darnassus, and carries our goddess'' light to all.$B$BShe will be glad to receive this sacred water. You will find her directly above us.','','',69,15000);
DELETE FROM `quest_objectives` WHERE `QuestID`=98065;
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`Flags`,`Visible`,`Description`,`VerifiedBuild`) VALUES (9806500,98065,1,0,0,279331,1,0,1,'Moonwell Remnants',0);
DELETE FROM `quest_template_addon` WHERE `ID`=98065;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (98065,0,0,0);
DELETE FROM `quest_request_items` WHERE `ID`=98065;
INSERT INTO `quest_request_items` (`ID`,`CompletionText`,`VerifiedBuild`) VALUES (98065,'Please state your business. I have a city to run, and a people to lead.',0);
DELETE FROM `quest_offer_reward` WHERE `ID`=98065;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (98065,'Staghelm has done much in my beloved''s absence, but he is not Malfurion. And he does not run the affairs of Darnassus--that burden is mine alone.$B$B<Tyrande''s face settles.>I apologize. As you have likely seen, we have many challenges to overcome.$B$BFandral is Arch Druid for now, and I ask you respect his station. But his doings pertain to druidism, and mine not only to Elune but indeed to lead our people as she wills.$B$BEvery drop of this water is blessed by our goddess. I thank you for saving it.',0);
DELETE FROM `quest_classic_money` WHERE `ID`=98065;
DELETE FROM `quest_classic_level` WHERE `ID`=98065;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (98065,11,1,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=98065;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (98065,2,11);
DELETE FROM `creature_queststarter` WHERE `quest`=98065;
DELETE FROM `gameobject_queststarter` WHERE `quest`=98065;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (4092,98065,0);
DELETE FROM `creature_questender` WHERE `quest`=98065;
DELETE FROM `gameobject_questender` WHERE `quest`=98065;
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (7999,98065,0);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry` IN (4092,7999);

-- 98189 Return the Statuette (level 25, required 20) -- Wowhead Forever page
DELETE FROM `quest_template` WHERE `ID`=98189;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyMultiplier`,`RewardSpell`,`StartItem`,`Flags`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`) VALUES (98189,2,11,1,1,1,0,0,0,4294967373,'Return the Statuette','Bring the Menethil Statuette to Captain Stoutfist in Mene','My goodness, it''s--it''s beautiful!$B$BThe Blind Princess was to deliver this to Menethil Harbor before tragedy struck. We were so close!$B$BI believe it was to be signed for by Captain Stoutfist in Menethil Keep just behind us. Why don''t you do the honors of finishing the job?','','');
DELETE FROM `quest_objectives` WHERE `QuestID`=98189;
DELETE FROM `quest_template_addon` WHERE `ID`=98189;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (98189,0,0,285);
DELETE FROM `quest_request_items` WHERE `ID`=98189;
INSERT INTO `quest_request_items` (`ID`,`CompletionText`,`VerifiedBuild`) VALUES (98189,'What''s that you''ve got there? Let me take a look.',0);
DELETE FROM `quest_offer_reward` WHERE `ID`=98189;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (98189,'You managed to get it back from those monsters! There aren''t too many artifacts of the late king left now.',0);
DELETE FROM `quest_classic_money` WHERE `ID`=98189;
DELETE FROM `quest_classic_level` WHERE `ID`=98189;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (98189,25,20,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=98189;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (98189,2,25);
DELETE FROM `creature_queststarter` WHERE `quest`=98189;
DELETE FROM `gameobject_queststarter` WHERE `quest`=98189;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (1242,98189,0);
DELETE FROM `creature_questender` WHERE `quest`=98189;
DELETE FROM `gameobject_questender` WHERE `quest`=98189;
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (2104,98189,0);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry` IN (1242,2104);

-- 98190 A Friend of the Family (level 25, required 20) -- Wowhead Forever page
DELETE FROM `quest_template` WHERE `ID`=98190;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyMultiplier`,`RewardSpell`,`StartItem`,`Flags`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`) VALUES (98190,2,1519,1,1,1,0,0,0,4294967373,'A Friend of the Family','Bring the Menethil Statuette to Highlord Bolvar Fordrag','You''ll want to take this to Stormwind right away. Proud as I am that this keep and harbor bears King Terenas Menethil II''s name, we are a calm harbor in a very nasty storm of enemies. It''s just not safe here.$B$BThe Dragonmaw in particular would sacrifice much to loot something like this... and I shudder to think of what they''d do with it.$B$BDon''t bother going up the chain of command on this one, <lad/lass>. Go straight to Highlord Fordragon with this. He''ll know where it will be best kept.','','');
DELETE FROM `quest_objectives` WHERE `QuestID`=98190;
DELETE FROM `quest_template_addon` WHERE `ID`=98190;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (98190,0,0,98189);
DELETE FROM `quest_request_items` WHERE `ID`=98190;
INSERT INTO `quest_request_items` (`ID`,`CompletionText`,`VerifiedBuild`) VALUES (98190,'Let us speak, $c. I can feel you carry something of the Light.',0);
DELETE FROM `quest_offer_reward` WHERE `ID`=98190;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (98190,'You have my gratitude for recovering this. I am relieved it did not fall into the wrong hands.',0);
DELETE FROM `quest_classic_money` WHERE `ID`=98190;
DELETE FROM `quest_classic_level` WHERE `ID`=98190;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (98190,25,20,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=98190;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (98190,2,25);
DELETE FROM `creature_queststarter` WHERE `quest`=98190;
DELETE FROM `gameobject_queststarter` WHERE `quest`=98190;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (2104,98190,0);
DELETE FROM `creature_questender` WHERE `quest`=98190;
DELETE FROM `gameobject_questender` WHERE `quest`=98190;
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (1748,98190,0);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry` IN (1748,2104);

-- 98191 A Friend of the Family (level 25, required 20) -- Wowhead Forever page
DELETE FROM `quest_template` WHERE `ID`=98191;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyMultiplier`,`RewardSpell`,`StartItem`,`Flags`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`RewardChoiceItemID1`,`RewardChoiceItemQuantity1`,`RewardChoiceItemID2`,`RewardChoiceItemQuantity2`) VALUES (98191,2,1519,1,1,1,0,0,0,4294967373,'A Friend of the Family','Bring the Menethil Statuette to Lord Grayson Shadowbrea','King Terenas was a good man. A devout man. Beloved by his people, and truly the reason we have this Alliance at all.$B$BThe irony is he was humble--he hated these sorts of memorials, and so precious few exist. And of those, few survived Lordaeron''s fall.$B$BIt seems the Light has coalesced in this statuette. It would be best kept in the Cathedral of the Light in Cathedral Square, in the center of the city. Bring it to Lord Grayson Shadowbreaker for safekeeping.','','',281296,1,281297,1);
DELETE FROM `quest_objectives` WHERE `QuestID`=98191;
DELETE FROM `quest_template_addon` WHERE `ID`=98191;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (98191,0,0,98189);
DELETE FROM `quest_request_items` WHERE `ID`=98191;
INSERT INTO `quest_request_items` (`ID`,`CompletionText`,`VerifiedBuild`) VALUES (98191,'Light be with you, $c. You are clearly not here for small talk--what is it?',0);
DELETE FROM `quest_offer_reward` WHERE `ID`=98191;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (98191,'By the Light... it looks just like his boy...$B$B<Lord Grayson Shadowbreaker pauses for a moment.>This statuette has indeed been blessed. Its connection to Terenas, its survival of what that monster did to him... this carries great power for those who believe in its strength.$B$BIt is best kept here for now, under guard of the paladin order. We will meditate on it further to learn more of its gifts, and perhaps it may aid us in dark times ahead.$B$BThank you, $c. Light bless you.',0);
DELETE FROM `quest_classic_money` WHERE `ID`=98191;
DELETE FROM `quest_classic_level` WHERE `ID`=98191;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (98191,25,20,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=98191;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (98191,2,25);
DELETE FROM `creature_queststarter` WHERE `quest`=98191;
DELETE FROM `gameobject_queststarter` WHERE `quest`=98191;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (1748,98191,0);
DELETE FROM `creature_questender` WHERE `quest`=98191;
DELETE FROM `gameobject_questender` WHERE `quest`=98191;
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (928,98191,0);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry` IN (928,1748);

-- 98298 Arugal's Folly (level 16, required 9) -- Wowhead Forever page
DELETE FROM `quest_template` WHERE `ID`=98298;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyMultiplier`,`RewardSpell`,`StartItem`,`Flags`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`RewardItem1`,`RewardAmount1`,`RewardFactionID1`,`RewardFactionOverride1`) VALUES (98298,2,130,5,1,1,0,0,8,8589934770,'Arugal''s Folly','Bring 6 Worgen Bits to Dalar Dawnweaver in The Sepulcher.','Ah, it''s all become so embarrassingly clear! Arugal hardly had any magic at all, let alone enough to create the worgen. ''Arugal''s Curse.'' Pah! Barely potent enough to last a few seconds.$B$BThese shackles prove he can bind the beasts, not create them. The worgen clearly spread their curse a different way. I would know how.$B$BGo to the Greymane Wall to the south. The worgen seem to be spreading to the humans nearby. Tear the beasts apart and bring me whatever you find. This requires a more direct approach.','','',281315,1,68,10000);
DELETE FROM `quest_objectives` WHERE `QuestID`=98298;
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`Flags`,`Visible`,`Description`,`VerifiedBuild`) VALUES (9829800,98298,1,0,0,280375,6,0,1,'Worgen Bits',0);
DELETE FROM `quest_template_addon` WHERE `ID`=98298;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (98298,0,0,421);
DELETE FROM `quest_request_items` WHERE `ID`=98298;
INSERT INTO `quest_request_items` (`ID`,`CompletionText`,`VerifiedBuild`) VALUES (98298,'Fangs, claws, gizzards. It makes no difference to me. The easiest way to study something is to see the guts yourself.',0);
DELETE FROM `quest_offer_reward` WHERE `ID`=98298;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (98298,'It appears I get the last laugh after all, haha! Arugal truly was a fool. And I, Dalar Dawnweaver, have exposed him as the fraud he is.$B$BThese pieces are all the proof I need that the curse has nothing to do with that blithering dolt.$B$BRegrettably, that means his remedy is also worth nothing. It seems the search for a solution to the worgen curse continues.',0);
DELETE FROM `quest_classic_money` WHERE `ID`=98298;
INSERT INTO `quest_classic_money` (`ID`,`RewardMoney`,`RewardMoneyMaxLevel`) VALUES (98298,800,0);
DELETE FROM `quest_classic_level` WHERE `ID`=98298;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (98298,16,9,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=98298;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (98298,2,16);
DELETE FROM `creature_queststarter` WHERE `quest`=98298;
DELETE FROM `gameobject_queststarter` WHERE `quest`=98298;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (1938,98298,0);
DELETE FROM `creature_questender` WHERE `quest`=98298;
DELETE FROM `gameobject_questender` WHERE `quest`=98298;
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (1938,98298,0);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry` IN (1938);

-- 98391 The Sisterhood of Elune (level 10, required 5) -- Wowhead Forever page
DELETE FROM `quest_template` WHERE `ID`=98391;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyMultiplier`,`RewardSpell`,`StartItem`,`Flags`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`RewardFactionID1`,`RewardFactionOverride1`) VALUES (98391,2,1657,1,1,1,0,0,8,4294967373,'The Sisterhood of Elune','Speak to Sister Aquinne in Darnassus.','Blessings to you, $c. The Sisterhood of Elune has stood as a pillar of kaldorei society for thousands of years.$B$BBefore the Great Sundering, before the War of the Ancients, through the Long Vigil, we have guided and healed our people through the darkness. Such is our charge, and such is the demand of the peerless Mother Moon.$B$BWe now require your aid in a matter of great importance. Sister Aquinne awaits your arrival in Darnassus.','','',69,1000);
DELETE FROM `quest_objectives` WHERE `QuestID`=98391;
DELETE FROM `quest_template_addon` WHERE `ID`=98391;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (98391,0,0,0);
DELETE FROM `quest_request_items` WHERE `ID`=98391;
DELETE FROM `quest_offer_reward` WHERE `ID`=98391;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (98391,'I am glad you arrived so quickly. The Sisterhood requires your aid.',0);
DELETE FROM `quest_classic_money` WHERE `ID`=98391;
DELETE FROM `quest_classic_level` WHERE `ID`=98391;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (98391,10,5,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=98391;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (98391,2,10);
DELETE FROM `creature_queststarter` WHERE `quest`=98391;
DELETE FROM `gameobject_queststarter` WHERE `quest`=98391;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (3600,98391,0);
DELETE FROM `creature_questender` WHERE `quest`=98391;
DELETE FROM `gameobject_questender` WHERE `quest`=98391;
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (7316,98391,0);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry` IN (3600,7316);

-- 98392 Darkness in the Glade (level 12, required 6) -- Wowhead Forever page
DELETE FROM `quest_template` WHERE `ID`=98392;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyMultiplier`,`RewardSpell`,`StartItem`,`Flags`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`) VALUES (98392,2,141,5,1,1,0,0,8,4294967373,'Darkness in the Glade','Collect Hatescreech''s Amulet, Windmistress Gaedress'' Amulet, and Witchmother Arysa''s Amulet for Sentinel Arynia Cloudsbreak in the Or','These belts... look closely at the leather, $n. Do you see the taint running through the fibers?$B$BI have a suspicion as to its origin, but we will need further proof to be sure.$B$BVenture back out into the harpies'' nests to the west and take out their leaders. If their necklaces carry the same darkness as these belts, we''ll have our answer.','','');
DELETE FROM `quest_objectives` WHERE `QuestID`=98392;
DELETE FROM `quest_template_addon` WHERE `ID`=98392;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (98392,0,0,937);
DELETE FROM `quest_request_items` WHERE `ID`=98392;
INSERT INTO `quest_request_items` (`ID`,`CompletionText`,`VerifiedBuild`) VALUES (98392,'Have you wrested the amulets from the harpies'' leaders?',0);
DELETE FROM `quest_offer_reward` WHERE `ID`=98392;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (98392,'<Arynia investigates the amulets.>Do you see the foul green swirls within the gems? This is indeed fel moss corruption at work.$B$BI have heard similar reports from Shadowglen and elsewhere in the boughs of this substance turning our very world against us.$B$BI will inform my superiors of this at once.',0);
DELETE FROM `quest_classic_money` WHERE `ID`=98392;
DELETE FROM `quest_classic_level` WHERE `ID`=98392;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (98392,12,6,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=98392;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (98392,2,12);
DELETE FROM `creature_queststarter` WHERE `quest`=98392;
DELETE FROM `gameobject_queststarter` WHERE `quest`=98392;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (3519,98392,0);
DELETE FROM `creature_questender` WHERE `quest`=98392;
DELETE FROM `gameobject_questender` WHERE `quest`=98392;
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (3519,98392,0);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry` IN (3519);

-- 98393 The Great Cat Spirit (level 20, required 20) -- Wowhead Forever page
DELETE FROM `quest_template` WHERE `ID`=98393;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyMultiplier`,`RewardSpell`,`StartItem`,`Flags`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`RewardFactionID1`,`RewardFactionOverride1`) VALUES (98393,2,-263,2,1,1,0,0,0,4294967373,'The Great Cat Spirit','Speak with Dendrite Starblaze in the village of Nighthaven, Moonglade.','It would seem that you are ready for the next step in your journey, young druid. Dendrite Starblaze has sent for you, and wishes for you to seek him out in Moonglade once again.$B$BYou have already learned to call upon the magic to take you to Moonglade, so you should be able to embark immediately if you choose. I would suggest you leave soon, if you wish to continue your druidic training.','','',609,2500);
DELETE FROM `quest_objectives` WHERE `QuestID`=98393;
DELETE FROM `quest_template_addon` WHERE `ID`=98393;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (98393,0,1024,0);
DELETE FROM `quest_request_items` WHERE `ID`=98393;
DELETE FROM `quest_offer_reward` WHERE `ID`=98393;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (98393,'And so it continues, Father Cenarius.$B$B<Dendrite makes a quick, invisible gesture in the air above him. A slight aura of power falls over him.>Greetings again, $n. It is time for you to learn the next perspective of nature: the cat. I will put you on the path to understanding this aspect of druidic life, but much like the bear, it is you that must embrace this learning and propagate it.',0);
DELETE FROM `quest_classic_money` WHERE `ID`=98393;
DELETE FROM `quest_classic_level` WHERE `ID`=98393;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (98393,20,20,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=98393;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (98393,2,20);
DELETE FROM `creature_queststarter` WHERE `quest`=98393;
DELETE FROM `gameobject_queststarter` WHERE `quest`=98393;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (4217,98393,0);
DELETE FROM `creature_questender` WHERE `quest`=98393;
DELETE FROM `gameobject_questender` WHERE `quest`=98393;
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (11802,98393,0);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry` IN (4217,11802);

-- 98394 The Great Cat Spirit (level 20, required 20) -- Wowhead Forever page
DELETE FROM `quest_template` WHERE `ID`=98394;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyMultiplier`,`RewardSpell`,`StartItem`,`Flags`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`RewardFactionID1`,`RewardFactionOverride1`) VALUES (98394,2,-263,3,1,1,0,0,0,4294967373,'The Great Cat Spirit','Seek out the Great Cat Spirit in southern part Moonglade and learn what it has to share with you about the nature o','In the southern part of Moonglade, on a hill overlooking the bridge, resides a wise and cunning being we call the Great Cat Spirit.$B$BMuch like the Great Bear Spirit, this spirit has served as a tutor and mentor to druids that desire to take the feline shape. All druids eventually seek its wisdom, and if you desire to take the form of a cat, you will as well.$B$BGo meet with the Great Cat Spirit and learn what it will teach you.','','',609,5000);
DELETE FROM `quest_objectives` WHERE `QuestID`=98394;
DELETE FROM `quest_template_addon` WHERE `ID`=98394;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (98394,0,1024,0);
DELETE FROM `quest_request_items` WHERE `ID`=98394;
DELETE FROM `quest_offer_reward` WHERE `ID`=98394;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (98394,'I see you have come to learn to draw upon my spirit. Good. Let us see if you are ready to take my shape.',0);
DELETE FROM `quest_classic_money` WHERE `ID`=98394;
DELETE FROM `quest_classic_level` WHERE `ID`=98394;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (98394,20,20,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=98394;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (98394,2,20);
DELETE FROM `creature_queststarter` WHERE `quest`=98394;
DELETE FROM `gameobject_queststarter` WHERE `quest`=98394;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (11802,98394,0);
DELETE FROM `creature_questender` WHERE `quest`=98394;
DELETE FROM `gameobject_questender` WHERE `quest`=98394;
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (11957,98394,0);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry` IN (11802,11957);

-- 98396 The Great Cat Spirit (level 20, required 20) -- Wowhead Forever page
DELETE FROM `quest_template` WHERE `ID`=98396;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyMultiplier`,`RewardSpell`,`StartItem`,`Flags`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`RewardFactionID1`,`RewardFactionOverride1`) VALUES (98396,2,-263,3,1,1,0,0,0,4294967373,'The Great Cat Spirit','Recover the Relic of the Fang, Relic of the Claw, and Relic of the Silent Shadow from the Stormrage Barrow Den, and','To draw upon my spirit, you will need more than a few words. To understand and respect my form you must prove that you can embody cunning, stealth, and if you should be cornered, ferocity.$B$BIn the Barrow Dens just east of here, are three relics of my favor you must acquire. They are guarded by powerful enraged spirits that will attack you on sight. A statue at the entrance will bless you with temporary stealth to get past them.$B$BReturn these relics to me. We will see if you are ready to take my shape.','','',609,5000);
DELETE FROM `quest_objectives` WHERE `QuestID`=98396;
DELETE FROM `quest_template_addon` WHERE `ID`=98396;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (98396,0,1024,0);
DELETE FROM `quest_request_items` WHERE `ID`=98396;
INSERT INTO `quest_request_items` (`ID`,`CompletionText`,`VerifiedBuild`) VALUES (98396,'Ah, you have returned. I do hope you have brought the relics with you?',0);
DELETE FROM `quest_offer_reward` WHERE `ID`=98396;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (98396,'Excellent. You have shown you understand the true nature of the cat, and are able to embody the cunning, stealth, and ferocity needed to take my shape.$B$BI now deem you worthy of my blessing, young $c.',0);
DELETE FROM `quest_classic_money` WHERE `ID`=98396;
DELETE FROM `quest_classic_level` WHERE `ID`=98396;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (98396,20,20,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=98396;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (98396,2,20);
DELETE FROM `creature_queststarter` WHERE `quest`=98396;
DELETE FROM `gameobject_queststarter` WHERE `quest`=98396;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (11957,98396,0);
DELETE FROM `creature_questender` WHERE `quest`=98396;
DELETE FROM `gameobject_questender` WHERE `quest`=98396;
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (11957,98396,0);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry` IN (11957);

-- 98397 To Darnassus (level 20, required 20) -- Wowhead Forever page
DELETE FROM `quest_template` WHERE `ID`=98397;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyMultiplier`,`RewardSpell`,`StartItem`,`Flags`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`) VALUES (98397,2,-263,1,1,1,1309050,0,0,4294967373,'To Darnassus','Return to Darnassus and speak with Mathrengyl Bearwalker in the Cenarion Enclave.','The time has come for us to part ways once again, young one, as the application of your lessons takes you back to the one who sent you to me. It would seem your instructor, Mathrengyl Bearwalker, has found a worthy pupil in you. I believe I see one as well. Heed his lessons and you shall soon be able to draw upon the Great Cat Spirit to take its shape.$B$BGo back to whence you came. When the time is right, we will meet again.','','');
DELETE FROM `quest_objectives` WHERE `QuestID`=98397;
DELETE FROM `quest_template_addon` WHERE `ID`=98397;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (98397,0,1024,98731);
DELETE FROM `quest_request_items` WHERE `ID`=98397;
DELETE FROM `quest_offer_reward` WHERE `ID`=98397;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (98397,'Welcome back, $n. I can sense you have met with the Great Cat Spirit and have successfully completed the trial. All druids must pass a trial given by this spirit, though the trials vary by generation. I am proud you have passed and have heeded the wisdom of the Great Cat Spirit. It is an ancient spirit that is as deadly as it is playful, though I can sense you understand the spirit''s nature already.$B$BI will now instruct you on how to take the shape of the cat.',0);
DELETE FROM `quest_classic_money` WHERE `ID`=98397;
DELETE FROM `quest_classic_level` WHERE `ID`=98397;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (98397,20,20,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=98397;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (98397,2,20);
DELETE FROM `creature_queststarter` WHERE `quest`=98397;
DELETE FROM `gameobject_queststarter` WHERE `quest`=98397;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (11802,98397,0);
DELETE FROM `creature_questender` WHERE `quest`=98397;
DELETE FROM `gameobject_questender` WHERE `quest`=98397;
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (4217,98397,0);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry` IN (4217,11802);

-- 98731 Blessings of the Great Cat Spirit (level 20, required 20) -- Wowhead Forever page
DELETE FROM `quest_template` WHERE `ID`=98731;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyMultiplier`,`RewardSpell`,`StartItem`,`Flags`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`RewardFactionID1`,`RewardFactionOverride1`) VALUES (98731,2,-263,3,1,1,0,0,0,4294967373,'Blessings of the Great Cat Spirit','Completion<Dendrite makes another invisible gesture in the air above him as you approach. Another aura','Now that you have received my blessing, return to Dendrite. He will instruct you further. I do hope we meet again someday.$B$BMay the shadows forever hide you and your foes easily fall prey to your claws, young one.','','',609,5000);
DELETE FROM `quest_objectives` WHERE `QuestID`=98731;
DELETE FROM `quest_template_addon` WHERE `ID`=98731;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (98731,0,1024,0);
DELETE FROM `quest_request_items` WHERE `ID`=98731;
DELETE FROM `quest_offer_reward` WHERE `ID`=98731;
DELETE FROM `quest_classic_money` WHERE `ID`=98731;
DELETE FROM `quest_classic_level` WHERE `ID`=98731;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (98731,20,20,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=98731;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (98731,2,20);
DELETE FROM `creature_queststarter` WHERE `quest`=98731;
DELETE FROM `gameobject_queststarter` WHERE `quest`=98731;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (11957,98731,0);
DELETE FROM `creature_questender` WHERE `quest`=98731;
DELETE FROM `gameobject_questender` WHERE `quest`=98731;
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (11802,98731,0);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry` IN (11802,11957);
