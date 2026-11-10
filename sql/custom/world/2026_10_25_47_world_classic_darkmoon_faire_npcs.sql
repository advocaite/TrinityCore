-- Classic 1.60: the Darkmoon Faire runs again (2026_10_25_36), so its NPCs get their vanilla gossip and flavour from VMaNGOS 1.12.
--
-- Gossip: Selina Dourman 10445 and Maxima Blastenheimer 15303 had the Cataclysm Faire menus (13075 Darkmoon Adventurer's Guide,
-- Whack-a-Gnoll, Ring Toss, Shooting Gallery; 13064 with no text at all). The guide spell 103413 is not in the 70532 client and those
-- attractions are not at the Elwynn and Mulgore Faire. They now use the VMaNGOS menus 6582 and 6575, which are already in the
-- database. The two Cataclysm "Launch me! (Darkmoon Game Token)" options of 6575 did nothing and are removed.
--
-- Flavour: the barkers, vendors, carnies and Flik had their VMaNGOS EventAI talk only there (no creature_text, no AI here).
-- VMaNGOS creature_ai_events 1482801..1487102 -> SmartAI out-of-combat timers (event 1, same ms), talk random line of the group.
-- Melnan Darkstone (Ironforge) and Kruban Darkblade (Orgrimmar) announce the Faire while the buildings go up (events 23/24) and
-- while it is open (events 4/5), as VMaNGOS conditions 955 and 946. Only the carnies VMaNGOS names talk (conditions 510/511/512/516,
-- spawn guids 20000000 + the VMaNGOS guid): the opening-day ones say the litter lines, the building crew the work lines.
-- Yebb Neblegear plays a random animal sound of his pets (VMaNGOS generic_scripts 1482901..1482904), Flik talks and his frog croaks.
-- Sayge (TDB fortune script) and Silas Darkmoon are unchanged. Texts are BroadcastText rows of the client. Safe to run again.

UPDATE `creature_template_gossip` SET `MenuID` = 6582 WHERE `CreatureID` = 10445;
UPDATE `creature_template_gossip` SET `MenuID` = 6575 WHERE `CreatureID` = 15303;
DELETE FROM `gossip_menu_option` WHERE `MenuID` = 6575 AND `OptionID` IN (1,2);

UPDATE `creature_template` SET `AIName` = 'SmartAI' WHERE `entry` IN (14828,14842,14843,14844,14845,14846,14847,14849,14860,14871,14829) AND `ScriptName` = '';
DELETE FROM `creature_text` WHERE `CreatureID` IN (14828,14842,14843,14844,14845,14846,14847,14849,14860,14871);
INSERT INTO `creature_text` (`CreatureID`,`GroupID`,`ID`,`Text`,`Type`,`Language`,`Probability`,`Emote`,`Duration`,`Sound`,`SoundPlayType`,`BroadcastTextId`,`TextRange`,`comment`) VALUES
(14828,0,0,'Tickets! Redeem your Darkmoon Prize Tickets here!  Wondrous and exotic prizes are waiting for you!',12,0,25,0,0,0,0,10149,0,'Gelvas Grimegate'),
(14828,0,1,'Mysterious prizes await the adventurous.  Step right up, step right up!',12,0,25,0,0,0,0,10150,0,'Gelvas Grimegate'),
(14828,0,2,'Five tickets or five hundred, it doesn\'t matter friend, everybody is a winner!',12,0,25,0,0,0,0,10151,0,'Gelvas Grimegate'),
(14828,0,3,'Prizes!  Get your prizes right here!  Get over here and turn in your Darkmoon Prize Tickets for valuable prizes!',12,0,25,0,0,0,0,10152,0,'Gelvas Grimegate'),
(14842,0,0,'The Darkmoon Faire is coming to town!  Don\'t miss it!',12,0,25,5,0,0,0,10097,0,'Melnan Darkstone'),
(14842,0,1,'The Darkmoon Faire is coming!  There is fun for children of ALL ages!  Come one, come all!',12,0,25,5,0,0,0,10098,0,'Melnan Darkstone'),
(14842,0,2,'Step right up and hear about the most amazing spectacle in all of Azeroth - the Darkmoon Faire!  Huzzah!',12,0,25,94,0,0,0,10099,0,'Melnan Darkstone'),
(14842,0,3,'Be sure to catch all the amazing sights and wonders of the Darkmoon Faire when it comes to town!',12,0,25,5,0,0,0,10100,0,'Melnan Darkstone'),
(14842,1,0,'It\'s here!  The Darkmoon Faire is now open for business!  Be sure to see the most exciting show on all of Azeroth!',12,0,25,5,0,0,0,10101,0,'Melnan Darkstone'),
(14842,1,1,'The Darkmoon Faire has come to town!  Be sure to experience the most thrilling show you\'ll ever see!',12,0,25,5,0,0,0,10102,0,'Melnan Darkstone'),
(14842,1,2,'The Darkmoon Faire will only be live for a short time before the curtains are drawn closed.  Be sure to visit it today!',12,0,25,5,0,0,0,10103,0,'Melnan Darkstone'),
(14842,1,3,'Boys and girls - children of all ages - the Darkmoon Faire has arrived!  Huzzah!',12,0,25,5,0,0,0,10104,0,'Melnan Darkstone'),
(14843,0,0,'The Darkmoon Faire is coming to town!  Don\'t miss it!',12,0,25,5,0,0,0,10093,0,'Kruban Darkblade'),
(14843,0,1,'Be sure to catch all the amazing sights and wonders of the Darkmoon Faire when it comes to town!',12,0,25,5,0,0,0,10094,0,'Kruban Darkblade'),
(14843,0,2,'The Darkmoon Faire is coming!  There is fun for children of ALL ages!  Come one, come all!',12,0,25,5,0,0,0,10095,0,'Kruban Darkblade'),
(14843,0,3,'Step right up and hear about the most amazing spectacle in all of Azeroth - the Darkmoon Faire!  Huzzah!',12,0,25,94,0,0,0,10096,0,'Kruban Darkblade'),
(14843,1,0,'The Darkmoon Faire has come to town!  Be sure to experience the most thrilling show you\'ll ever see!',12,0,25,5,0,0,0,10106,0,'Kruban Darkblade'),
(14843,1,1,'The Darkmoon Faire will only be live for a short time before the curtains are drawn closed.  Be sure to visit it today!',12,0,25,5,0,0,0,10107,0,'Kruban Darkblade'),
(14843,1,2,'Boys and girls - children of all ages - the Darkmoon Faire has arrived!  Huzzah!',12,0,25,5,0,0,0,10108,0,'Kruban Darkblade'),
(14843,1,3,'It\'s here!  The Darkmoon Faire is now open for business!  Be sure to see the most exciting show on all of Azeroth!',12,0,25,5,0,0,0,10105,0,'Kruban Darkblade'),
(14844,0,0,'Cheap beer!  Get your cheap beer right here!',12,0,25,0,0,0,0,10129,0,'Sylannia'),
(14844,0,1,'Huzzah for the tipper!  Step right up and get your delicious, cold drinks right here!',12,0,25,0,0,0,0,10130,0,'Sylannia'),
(14844,0,2,'Just in... another batch of Darkmoon Special Reserve.  Come get yours before they\'re all gone!',12,0,25,0,0,0,0,10131,0,'Sylannia'),
(14844,0,3,'You there!  Yes you!  You look thirsty.  Get over here, I have just what you need!',12,0,25,0,0,0,0,10132,0,'Sylannia'),
(14845,0,0,'Step right up!  Get your red hot wings right here!',12,0,25,0,0,0,0,10133,0,'Stamp Thunderhorn'),
(14845,0,1,'You\'ve got money and we\'ve got food.  What are you waiting for?!',12,0,25,0,0,0,0,10135,0,'Stamp Thunderhorn'),
(14845,0,2,'Darkmoon Dogs fresh off the grill.  Come and get \'em while they\'re hot!',12,0,25,0,0,0,0,10134,0,'Stamp Thunderhorn'),
(14845,0,3,'For a limited time only, crunchy frog available exclusively here at the Darkmoon Faire.',12,0,25,0,0,0,0,10136,0,'Stamp Thunderhorn'),
(14846,0,0,'Every item that I have for sale I acquired myself.  You have my personal guarantee of authenticity.',12,0,25,0,0,0,0,10141,0,'Lhara'),
(14846,0,1,'Darkmoon Faire Cards?  See the Professor here for those.',12,0,25,0,0,0,0,10142,0,'Lhara'),
(14846,0,2,'Come and take a look at my wares while you still can.  We\'re here all week, and then we\'re off to some far off land.',12,0,25,0,0,0,0,10143,0,'Lhara'),
(14846,0,3,'Have you any idea what I go through to bring these incomparable valuables to sell?  Come see them all for yourself!',12,0,25,0,0,0,0,10144,0,'Lhara'),
(14847,0,0,'Have a completed Darkmoon deck? Turn it in here!',12,0,25,0,0,0,0,10137,0,'Professor Thaddeus Paleo'),
(14847,0,1,'Come speak with me if you have any Darkmoon Faire Cards.',12,0,25,0,0,0,0,10138,0,'Professor Thaddeus Paleo'),
(14847,0,2,'Darkmoon decks! Turn them in here!',12,0,25,0,0,0,0,10139,0,'Professor Thaddeus Paleo'),
(14847,0,3,'Have you any Darkmoon Faire cards?  Come speak with me to learn more about them if you dare!',12,0,25,0,0,0,0,10140,0,'Professor Thaddeus Paleo'),
(14849,0,0,'Sure are a lot of litter bugs around here.',12,0,25,1,0,0,0,10118,0,'Darkmoon Carnie'),
(14849,0,1,'Having a good time?',12,0,25,6,0,0,0,10119,0,'Darkmoon Carnie'),
(14849,0,2,'Don\'t forget to buy refreshments and souvenirs!',12,0,25,1,0,0,0,10120,0,'Darkmoon Carnie'),
(14849,0,3,'The Darkmoon Faire is the greatest event on all of Azeroth!',12,0,25,71,0,0,0,10121,0,'Darkmoon Carnie'),
(14849,1,0,'Can\'t you see I\'ve got work to do here?',12,0,25,6,0,0,0,10058,0,'Darkmoon Carnie'),
(14849,1,1,'Faire\'s a coming!',12,0,25,71,0,0,0,10059,0,'Darkmoon Carnie'),
(14849,1,2,'That\'s right friend!  The Darkmoon Faire is going to be right here.  Just as soon as I unload those huge wagons, put up all the tents, erect the zoo and pour the drinks.  Make sure you come back!',12,0,25,1,0,0,0,10060,0,'Darkmoon Carnie'),
(14849,1,3,'Won\'t be long now until the Darkmoon Faire opens.  Come back later and check to see if we\'re done.',12,0,25,1,0,0,0,10061,0,'Darkmoon Carnie'),
(14860,0,0,'Frogs and leather balls for sale!',12,0,100,0,0,0,0,10148,0,'Flik'),
(14871,0,0,'Jubjub?  Where are you, Jubjub?  Oh no!  Where did you go this time!',12,0,100,0,0,0,0,10171,0,'Morja');

DELETE FROM `smart_scripts` WHERE `source_type` = 0 AND `entryorguid` IN (14828,14842,14843,14844,14845,14846,14847,14860,14871,14829,-20056680,-20056613,-20000684,-20000720,-20046801,-20042598,-20008969,-20301271,-20000724,-20000566);
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`Difficulties`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`event_param5`,`event_param_string`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`action_param7`,`action_param_string`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_param4`,`target_param_string`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(14828,0,0,0,'',1,0,100,0,45000,84000,45000,84000,0,'',1,0,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'Gelvas Grimegate - Out of Combat - Talk (VMaNGOS EventAI 1482801)'),
(14844,0,0,0,'',1,0,100,0,45000,84000,45000,84000,0,'',1,0,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'Sylannia - Out of Combat - Talk (VMaNGOS EventAI 1484401)'),
(14845,0,0,0,'',1,0,100,0,45000,84000,45000,84000,0,'',1,0,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'Stamp Thunderhorn - Out of Combat - Talk (VMaNGOS EventAI 1484501)'),
(14846,0,0,0,'',1,0,100,0,45000,84000,45000,84000,0,'',1,0,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'Lhara - Out of Combat - Talk (VMaNGOS EventAI 1484601)'),
(14847,0,0,0,'',1,0,100,0,45000,84000,45000,84000,0,'',1,0,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'Professor Thaddeus Paleo - Out of Combat - Talk (VMaNGOS EventAI 1484701)'),
(14860,0,0,1,'',1,0,100,0,30000,45000,30000,45000,0,'',1,0,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'Flik - Out of Combat - Talk (VMaNGOS EventAI 1486001)'),
(14860,0,1,0,'',61,0,100,0,0,0,0,0,0,'',4,8353,0,1,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'Flik - Linked - Flik\'s Frog sound (VMaNGOS generic_scripts 1486001)'),
(14871,0,0,0,'',1,0,100,0,30000,84000,30000,84000,0,'',1,0,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'Morja - Out of Combat - Talk (VMaNGOS EventAI 1487102)'),
(14829,0,0,0,'',1,0,100,0,3000,40000,3000,40000,0,'',115,8354,8352,8356,8355,0,1,0,'',1,0,0,0,0,'',0,0,0,0,'Yebb Neblegear - Out of Combat - Random pet sound (VMaNGOS EventAI 1482901)'),
(14842,0,0,0,'',1,0,100,0,45000,120000,45000,120000,0,'',1,0,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'Melnan Darkstone - Out of Combat - Talk, Faire coming (VMaNGOS EventAI 1484201)'),
(14842,0,1,0,'',1,0,100,0,45000,120000,45000,120000,0,'',1,1,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'Melnan Darkstone - Out of Combat - Talk, Faire open (VMaNGOS EventAI 1484202)'),
(14843,0,0,0,'',1,0,100,0,45000,120000,45000,120000,0,'',1,0,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'Kruban Darkblade - Out of Combat - Talk, Faire coming (VMaNGOS EventAI 1484301)'),
(14843,0,1,0,'',1,0,100,0,45000,120000,45000,120000,0,'',1,1,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'Kruban Darkblade - Out of Combat - Talk, Faire open (VMaNGOS EventAI 1484302)'),
(-20056680,0,0,0,'',1,0,100,0,45000,84000,45000,84000,0,'',1,0,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'Darkmoon Faire Carnie - Out of Combat - Talk (VMaNGOS EventAI 1484901)'),
(-20056613,0,0,0,'',1,0,100,0,45000,84000,45000,84000,0,'',1,0,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'Darkmoon Faire Carnie - Out of Combat - Talk (VMaNGOS EventAI 1484901)'),
(-20000684,0,0,0,'',1,0,100,0,45000,84000,45000,84000,0,'',1,0,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'Darkmoon Faire Carnie - Out of Combat - Talk (VMaNGOS EventAI 1484901)'),
(-20000720,0,0,0,'',1,0,100,0,45000,84000,45000,84000,0,'',1,0,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'Darkmoon Faire Carnie - Out of Combat - Talk (VMaNGOS EventAI 1484901)'),
(-20046801,0,0,0,'',1,0,100,0,45000,72000,45000,72000,0,'',1,1,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'Darkmoon Faire Carnie - Out of Combat - Talk (VMaNGOS EventAI 1484902)'),
(-20042598,0,0,0,'',1,0,100,0,45000,72000,45000,72000,0,'',1,1,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'Darkmoon Faire Carnie - Out of Combat - Talk (VMaNGOS EventAI 1484902)'),
(-20008969,0,0,0,'',1,0,100,0,45000,72000,45000,72000,0,'',1,1,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'Darkmoon Faire Carnie - Out of Combat - Talk (VMaNGOS EventAI 1484902)'),
(-20301271,0,0,0,'',1,0,100,0,45000,72000,45000,72000,0,'',1,1,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'Darkmoon Faire Carnie - Out of Combat - Talk (VMaNGOS EventAI 1484902)'),
(-20000724,0,0,0,'',1,0,100,0,45000,72000,45000,72000,0,'',1,1,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'Darkmoon Faire Carnie - Out of Combat - Talk (VMaNGOS EventAI 1484902)'),
(-20000566,0,0,0,'',1,0,100,0,45000,72000,45000,72000,0,'',1,1,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'Darkmoon Faire Carnie - Out of Combat - Talk (VMaNGOS EventAI 1484902)');

DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId` = 22 AND `SourceId` = 0 AND `SourceEntry` IN (14842,14843);
INSERT INTO `conditions` (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,`ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,`ConditionStringValue1`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`) VALUES
(22,1,14842,0,0,12,0,23,0,0,'',0,0,0,'','Melnan Darkstone talks only while game event 23 runs (VMaNGOS condition 955)'),
(22,1,14842,0,1,12,0,24,0,0,'',0,0,0,'','Melnan Darkstone talks only while game event 24 runs (VMaNGOS condition 955)'),
(22,2,14842,0,0,12,0,4,0,0,'',0,0,0,'','Melnan Darkstone talks only while game event 4 runs (VMaNGOS condition 946)'),
(22,2,14842,0,1,12,0,5,0,0,'',0,0,0,'','Melnan Darkstone talks only while game event 5 runs (VMaNGOS condition 946)'),
(22,1,14843,0,0,12,0,23,0,0,'',0,0,0,'','Kruban Darkblade talks only while game event 23 runs (VMaNGOS condition 955)'),
(22,1,14843,0,1,12,0,24,0,0,'',0,0,0,'','Kruban Darkblade talks only while game event 24 runs (VMaNGOS condition 955)'),
(22,2,14843,0,0,12,0,4,0,0,'',0,0,0,'','Kruban Darkblade talks only while game event 4 runs (VMaNGOS condition 946)'),
(22,2,14843,0,1,12,0,5,0,0,'',0,0,0,'','Kruban Darkblade talks only while game event 5 runs (VMaNGOS condition 946)');
