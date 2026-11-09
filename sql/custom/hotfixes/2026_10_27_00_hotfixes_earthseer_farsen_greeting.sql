-- Classic 1.60 (WoW Forever): Earthseer Farsen's greeting, as the official server shows it (Discord screenshot from Neylem,
-- 2026-10-26). No sniff has the line, so its BroadcastText ID is not known: 400000 is our own, above every ID in the 70532
-- hotfix data (highest 328288). Replace it with the real ID once a sniff shows it. Own hotfix_data id. Safe to run again.
DELETE FROM `broadcast_text` WHERE `ID` = 400000;
INSERT INTO `broadcast_text` (`Text`,`Text1`,`ID`,`LanguageID`,`ConditionID`,`EmotesID`,`Flags`,`ChatBubbleDurationMs`,`VoiceOverPriorityID`,`SoundKitID1`,`SoundKitID2`,`EmoteID1`,`EmoteID2`,`EmoteID3`,`EmoteDelay1`,`EmoteDelay2`,`EmoteDelay3`,`VerifiedBuild`) VALUES
('On a clear day, you can even see Blackrock Mountain from here. It''s both beautiful and terrifying at the same time.','',400000,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0);
DELETE FROM `hotfix_data` WHERE `Id` = 2000200;
INSERT INTO `hotfix_data` (`Id`,`UniqueId`,`TableHash`,`RecordId`,`Status`,`VerifiedBuild`) VALUES
(2000200,1397720200,35137211,400000,1,0);
