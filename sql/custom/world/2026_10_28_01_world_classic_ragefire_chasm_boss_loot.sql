-- Classic 1.60: Ragefire Chasm bosses Oggleflint (11517) and Bazzalan (11519) had no gear reference in the VMaNGOS import,
-- unlike Jergosh (30331) and Taragaman (30332); their three Forever items go in one group, so one of them drops per kill.
DELETE FROM `creature_loot_template` WHERE `Entry` IN (11517,11519) AND `Item` IN (272996,272998,272999,273003,273005,273007);
INSERT INTO `creature_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES
(11517,0,272999,0,0,1,1,1,1,'Oggleflint - Barbaric Crossbow'),
(11517,0,272998,0,0,1,1,1,1,'Oggleflint - Bone Knuckles'),
(11517,0,272996,0,0,1,1,1,1,'Oggleflint - Trogg Scepter'),
(11519,0,273003,0,0,1,1,1,1,'Bazzalan - Searing Dagger'),
(11519,0,273005,0,0,1,1,1,1,'Bazzalan - Satyrskin Cloak'),
(11519,0,273007,0,0,1,1,1,1,'Bazzalan - Chasm Walkers');
