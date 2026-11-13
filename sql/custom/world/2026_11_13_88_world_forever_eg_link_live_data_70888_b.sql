-- WoW Forever: world rows from live players' EG Link data (build 70888) that our world lacked or had different.
-- Data from Efficient Games (https://efficient.games), licensed under CC BY 4.0
-- (https://creativecommons.org/licenses/by/4.0/). Source: github.com/JIVESCORP/eg-link-output-wowf commit 164bd1e2, build 1.60.1.70888,
-- collected 2026-11-10..2026-11-12 from 5 players on the live realm. Changed: picked and converted to our tables; only rows we lacked
-- or had different from live. Where our own live captures (2026_11_10_60/61 and the 11-10 to 11-13 sniff files) cover a record, ours is kept.
-- Safe to run again.

-- 1. Object templates our world lacks (template and quest loot list only, no spawns). Fill-only: rows we have stay.
INSERT IGNORE INTO `gameobject_template` (`entry`,`type`,`displayId`,`name`,`IconName`,`castBarCaption`,`unk1`,`size`,`Data0`,`Data1`,`Data2`,`Data3`,`Data4`,`Data5`,`Data6`,`Data7`,`Data8`,`Data9`,`Data10`,`Data11`,`Data12`,`Data13`,`Data14`,`Data15`,`Data16`,`Data17`,`Data18`,`Data19`,`Data20`,`Data21`,`Data22`,`Data23`,`Data24`,`Data25`,`Data26`,`Data27`,`Data28`,`Data29`,`Data30`,`Data31`,`Data32`,`Data33`,`Data34`,`ContentTuningId`,`RequiredLevel`,`VerifiedBuild`) VALUES
(413699,10,84162,'Large Nest','','','',0.8,3686,0,0,0,0,0,0,0,0,0,428606,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,70888),
(417354,5,86802,'Ruined Tent','','','',1.0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(417355,5,86803,'Ruined Tent','','','',1.0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(417356,5,4451,'Human Skeleton','','','',1.0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(417357,5,86804,'Human Skeleton','','','',1.0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(417361,5,86808,'Looted Supplies','','','',1.0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(417362,5,3791,'Looted Supplies','','','',1.0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(423703,2,1107,'Broken Warhammer','','','',0.8,0,29896,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(423953,0,676,'Rusted Cage','','','',1.0,0,3732,327680,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(423955,5,1128,'Book','','','',1.0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(424265,3,12,'Grave','','','',1.0,43,122167,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4096,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(441948,10,2048,'Shrine of the Warden','','','',0.4,0,0,0,3000,0,0,3,9260,0,2,446448,0,0,0,0,0,0,0,0,0,0,0,162081,0,0,0,1,0,0,0,0,0,0,0,0,0,0,70888),
(516194,5,6329,'BlastedLandsBonePile03','','','',1.25,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(577958,5,4714,'Shen''dralar Pylon','','','',2.25,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(577959,2,4714,'Shen''dralar Pylon','','','',2.25,0,29082,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(581734,3,758,'Valley Bone','','','',2.0,4088,136143,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4097,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,70888),
(581737,3,6327,'Valley Bone','','','',2.0,4088,136143,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4097,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,70888),
(581738,3,114341,'Valley Bones','','','',3.0,43,136144,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4097,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,70888),
(581739,3,114342,'Valley Bones','','','',3.0,43,136144,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4097,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,70888),
(582187,3,114448,'Petrified Tree','','','',1.5,4088,136239,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5121,0,0,0,0,0,0,0,1,0,0,0,0,0,1,0,0,70888),
(584112,8,90799,'Orgrul''s Skeletons','','','',1.0,2241,10,0,0,92873,0,0,0,0,0,0,0,0,0,14,14,14,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(584550,5,114523,'Incomplete Summon','','','',2.0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(587299,3,219,'Tear of Theradras','','','',1.2,1635,136879,15,1,0,0,0,0,0,0,0,0,0,0,0,0,0,147491,0,0,4097,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(607122,3,115777,'Pylon Shard','','','',2.0,0,0,1,0,1,1,0,0,0,0,0,0,0,0,0,0,0,147104,0,0,5120,0,0,0,0,0,0,0,1,0,136286,0,0,0,0,0,0,70888),
(607123,3,115777,'Pylon Shard','','','',2.0,0,0,1,0,1,1,0,0,0,0,0,0,0,0,0,0,0,147105,0,0,5120,0,0,0,0,0,0,0,1,0,136287,0,0,0,0,0,0,70888),
(612074,5,116304,'Woodwyrd','','','',1.0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(613843,10,116946,'Wall of Fire','','','',0.3,4263,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,70888),
(614491,5,117260,'Fire','','','',3.0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(618568,10,676,'Twilight Cage','','','',1.2,4326,0,0,300000,0,0,0,0,0,0,1277397,0,0,0,0,0,0,0,0,0,0,0,923,0,0,0,1,0,0,0,0,0,0,0,0,0,0,70888),
(618608,0,676,'Twilight Cage','','','',1.2,1,86,3000,0,0,0,0,150956,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(620422,3,267,'Riverweed','','','',0.5,57,138719,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4097,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(620625,10,676,'Twilight Cage','','','',1.2,4326,0,0,300000,0,0,0,0,0,0,1277397,0,0,0,0,0,0,0,0,0,0,0,924,0,0,0,1,0,0,0,0,0,0,0,0,0,0,70888),
(621981,5,120306,'Tree Stump','','','',0.75,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(621982,5,120309,'Fallen Log','','','',1.0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(622001,3,120320,'Discarded Doll','','','',0.75,4335,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4096,0,0,0,0,0,0,0,0,0,138917,0,0,0,0,0,0,70888),
(625552,10,1787,'Cage','','','',1.0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,70888),
(625582,2,1,'Fuzzle''s Treasure','','','',1.0,0,29488,0,0,0,0,0,1,0,0,151407,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(629215,8,201,'Brazier','','','',1.0,4,5,2066,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(629217,8,201,'Brazier','','','',1.0,4,5,2066,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(629218,8,201,'Brazier','','','',1.0,4,5,2066,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(629219,8,201,'Brazier','','','',1.0,4,5,2066,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(629220,8,201,'Brazier','','','',1.0,4,5,2066,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(629221,8,201,'Brazier','','','',1.0,4,5,2066,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(629222,8,201,'Brazier','','','',1.0,4,5,2066,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(629223,8,201,'Brazier','','','',1.0,4,5,2066,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(629224,8,201,'Brazier','','','',1.0,4,5,2066,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(629225,8,201,'Brazier','','','',1.0,4,5,2066,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(629226,8,201,'Brazier','','','',1.0,4,5,2066,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(629227,8,201,'Brazier','','','',1.0,4,5,2066,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(629228,8,201,'Brazier','','','',1.0,4,5,2066,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(629229,8,201,'Brazier','','','',1.0,4,5,2066,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(629230,8,201,'Brazier','','','',1.0,4,5,2066,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(629231,8,201,'Brazier','','','',1.0,4,5,2066,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(629232,8,201,'Brazier','','','',1.0,4,5,2066,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(629233,8,201,'Brazier','','','',1.0,4,5,2066,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(629243,8,201,'Brazier','','','',1.0,4,5,2066,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(629245,8,703,'Bellow','','','',1.0,0,10,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(629246,8,273,'Anvil','','','',1.0,1,10,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(629247,8,273,'Anvil','','','',1.0,1,10,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(629694,8,129080,'Ritual Site','','','',1.0,2325,10,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(645164,2,17,'WANTED!','','','',1.0,0,29788,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(648994,3,253,'Wildflower','','','',1.0,4395,140538,60000,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4097,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(649266,0,411,'Guard Tower Portcullis','','','',1.51,0,61,30000,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(653499,5,6328,'Pile of Bones','','','',1.0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(654285,5,88145,'Rock Garden','','','',0.75,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(654618,3,86328,'Rog''mar Supply Crate','','','',0.6,43,141342,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4097,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(658115,2,285,'Lost Shipping Crate','','','',0.75,0,30122,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(658116,2,285,'Lost Shipping Crate','','','',0.75,0,30123,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(659817,8,273,'Anvil','','','',1.0,1,10,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(659819,8,192,'Campfire','','','',1.0,4,8,2061,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(659838,8,306,'Forge','','','',1.0,3,10,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(659839,8,867,'Bonfire','','','',1.0,4,10,2061,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(659853,8,192,'Campfire','','','',1.0,4,8,2061,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(667572,9,128398,'Gnawed on Forearm','','','',1.25,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(667676,3,128431,'Pristine Mattock','','','',1.0,43,142450,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4097,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(668003,10,128471,'Etched Stone','','','',1.01,4492,97663,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,160066,0,0,0,1,0,0,0,0,0,0,0,0,0,0,70888),
(668004,10,93072,'Etched Stone','','','',1.01,4492,97663,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,160069,0,0,0,1,0,0,0,0,0,0,0,0,0,0,70888),
(668295,3,105,'Tremayne Family Blacksmithing Techniques','','','',1.0,57,142510,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4097,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(670717,8,200,'Bonfire','','','',1.21,4,10,2066,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(677339,10,127537,'Letter from Dragha','','','',0.6,0,0,0,3000,0,0,0,10966,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,70888),
(694010,8,208,'Stone Anvil','','','',1.0,1,10,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(694011,8,408,'Cooking Fire','','','',1.0,4,10,2061,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(694017,8,732,'Bonfire','','','',1.0,4,10,2066,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(694019,8,200,'Bonfire','','','',1.0,4,10,2066,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(694020,8,732,'Bonfire','','','',1.0,4,10,2066,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(694054,3,164,'Recipe: Raw Plated Armorfish','','','',1.0,4494,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4097,0,0,0,0,0,0,0,0,0,144443,0,0,0,0,0,0,70888),
(694725,5,129510,'Empty Vial','','','',0.5,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(694729,5,132602,'Empty Vial','','','',1.0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(700626,23,5494,'Meeting Stone','','','',1.0,1,60,16842,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(702373,0,124832,'Door','','','',1.0,0,1620,30000,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(702837,7,138,'Chair','','','',1.0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(706187,3,63,'Tear of Tilloa','','','',1.0,43,1699,30,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4096,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888),
(707249,2,15,'Wanted','','','',0.5,0,30952,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888);
INSERT IGNORE INTO `gameobject_questitem` (`GameObjectEntry`,`Idx`,`ItemId`,`VerifiedBuild`) VALUES
(581734,0,254663,70888),
(581737,0,254663,70888),
(581738,0,254663,70888),
(581739,0,254663,70888),
(582187,0,254841,70888),
(587299,0,255878,70888),
(607122,0,255656,70888),
(607123,0,255657,70888),
(620422,0,265806,70888),
(622001,0,266251,70888),
(648994,0,271823,70888),
(654618,0,274975,70888),
(667676,0,278404,70888),
(668295,0,278868,70888),
(706187,0,2779,70888);

-- 2. Quest givers and turn-ins players used on live that our quests lack (the quests and NPCs/objects exist).
-- 79094 The Lessons of Ta'zo
INSERT IGNORE INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (211022,79094,70888);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry`=211022;
-- 79097 Baxtan: On Destructive Magics
INSERT IGNORE INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (211022,79097,70888);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry`=211022;

-- 3. Quests we built with placeholder text (VerifiedBuild -1): the live text, levels, rewards and objectives.
--    Givers and turn-ins stay ours. Reward text is not in the data, so the placeholder reward text stays.
-- 99192 A Donation of Silk: live record has no text yet, left as it is
-- 99197 A Donation of Silk: live record has no text yet, left as it is

-- 4. Quest objectives whose amount is different on live (same target).

-- 5. Quests whose story or objective line is different on live (not only a placeholder like $1oa or a line break).
-- 5250 Starfall
UPDATE `quest_template` SET `QuestDescription`='In Winterspring, to the northeast of Felwood, there are many ancient elven ruins. It''s a harsh land, but it holds many secrets and places to explore.\n\nMy studies here keep me so busy that I have not had the opportunity to travel to Winterspring for some time. But there''s no reason why you shouldn''t travel there and study the ruins.\n\nWhile you''re there, look for Wynd Nightchaser. He spends most of his time in Starfall and he knows a lot about the area.\n\nTell him that you are a student of mine...' WHERE `ID`=5250;
-- 5321 The Sleeper Has Awakened: live text left out, its objectives need 11218 which ours lacks
-- 5640 Desperate Prayer
UPDATE `quest_template` SET `QuestDescription`='Praise be to Elune, and to your Light, $n.\n\nAbout this time, many of our order learn abilities reflecting their faith in Elune. If knowledge serves me, I believe the same holds true for your order also.\n\nWhen your business in Darnassus is complete, I would seek out one of your High Priests for further training.', `LogDescription`='Speak to High Priest Rohan in Stormwind.' WHERE `ID`=5640;
-- 7507 Nostro's Compendium
UPDATE `quest_template` SET `LogDescription`='Return Nostro''s Compendium of Dragon Slaying to the Athenaeum.' WHERE `ID`=7507;
-- 7790 River Ends: left out. Forever reused this id; ours is still the old "REUSE" placeholder, so it needs the whole quest built.
-- 7871 Vanquish the Invaders!
UPDATE `quest_template` SET `QuestDescription`='Beyond this tunnel you will find a field of strife and turmoil, young $c. The Horde continue to decimate our sacred forest, cutting down the evergreen foliage to power their machines of war.\n\nEnter Warsong Gulch and defend Silverwing Hold. Push back the invading Horde forces!\n\nDo this, and earn a talisman of merit.  Bring me such a talisman, $n, and I shall reward you.' WHERE `ID`=7871;
-- 7876 Quell the Silverwing Usurpers
UPDATE `quest_template` SET `QuestDescription`='The wilds of the Ashenvale forest will succumb to the might of the Horde, $r. Nothing the Silverwing say or do can stop our sovereign imperative. Kalimdor belongs to the Horde. How dare they attempt to prevent us from harvesting what is rightfully ours!\n\nLet the pride swell in your chest as you cut down their weak attempts at slowing our progress. Destroy the Silverwing Sentinels, and earn a Warsong mark of honor.  Return to me with such a mark, $n, and you will be rewarded.\n' WHERE `ID`=7876;
-- 8152 Rapid Escape: live text left out, its objectives need 280504 which ours lacks
-- 8156 Arathi Basin Resources!
UPDATE `quest_template` SET `QuestDescription`='War must be fought with soldiers, any soldier will tell you.  They''ll also say that a battle fought with poor gear or on an empty stomach is lost before it starts!\n\nThat is why Arathi Basin is important.  There are key areas in the basin that hold essential resources.  Metals, weapons, food, lumber - all are needed, and all can be gained there.\n\nThat is what I want from you, $n.  Enter Arathi Basin, win the battle by holding more bases than the enemy, and return to me with a crate of resources.', `LogDescription`='Win the battle for Arathi Basin, get an Arathi Resource Crate, and return it to Sergeant Maclear at Refuge Pointe.' WHERE `ID`=8156;
-- 8291 Vanquish the Invaders!
UPDATE `quest_template` SET `QuestDescription`='Beyond this tunnel you will find a field of strife and turmoil, young $c. The Horde continue to decimate our sacred forest, cutting down the evergreen foliage to power their machines of war.\n\nEnter Warsong Gulch and defend Silverwing Hold. Push back the invading Horde forces!\n\nDo this, and earn a talisman of merit.  Bring me such a talisman, $n, and I shall reward you.' WHERE `ID`=8291;
-- 8294 Quell the Silverwing Usurpers
UPDATE `quest_template` SET `QuestDescription`='The wilds of the Ashenvale forest will succumb to the might of the Horde, $r. Nothing the Silverwing say or do can stop our sovereign imperative. Kalimdor belongs to the Horde. How dare they attempt to prevent us from harvesting what is rightfully ours!\n\nLet the pride swell in your chest as you cut down their weak attempts at slowing our progress. Destroy the Silverwing Sentinels, and earn a Warsong mark of honor.  Return to me with such a mark, $n, and you will be rewarded.' WHERE `ID`=8294;
-- 8297 Arathi Basin Resources!
UPDATE `quest_template` SET `QuestDescription`='War must be fought with soldiers, any soldier will tell you.  They''ll also say that a battle fought with poor gear or on an empty stomach is lost before it starts!\n\nThat is why Arathi Basin is important.  There are key areas in the basin that hold essential resources.  Metals, weapons, food, lumber - all are needed, and all can be gained there.\n\nThat is what I want from you, $n.  Enter Arathi Basin, win the battle by holding more bases than the enemy, and return to me with a crate of resources.', `LogDescription`='Win the battle for Arathi Basin, get an Arathi Resource Crate, and return it to Sergeant Maclear at Refuge Pointe.' WHERE `ID`=8297;
-- 8299 Cut Arathor Supply Lines
UPDATE `quest_template` SET `QuestDescription`='The League of Arathor is sparing no expense in their move to retake Arathi.  They, and all the races of the Alliance, now stream to this distant region, intent on expanding their hold from Refuge Pointe.  We must stop them, and the best means to stop them is to take their supplies.\n\nYou can help us, $n.  Enter Arathi Basin, known for its rich mines, fertile land and skilled craftsman.  Capture and control every base you can, win the battle, and return to me with the resources you gain.', `LogDescription`='Win the battle for Arathi Basin, get an Arathi Resource Crate, and return it to Deathstalker Mortis at Hammerfall.' WHERE `ID`=8299;
-- 8367 For Great Honor: live text left out, its objectives need 274895 which ours lacks
-- 8371 Concerted Efforts: live text left out, its objectives need 274895 which ours lacks
-- 8801 C'Thun's Legacy
UPDATE `quest_template` SET `LogDescription`='Take the Eye of C''Thun to Caelestrasz in the Temple of Ahn''Qiraj.' WHERE `ID`=8801;
-- 98189 Return the Statuette: live text left out, its objectives need 279589 which ours lacks
-- 98190 A Friend of the Family: live text left out, its objectives need 279595 which ours lacks
-- 98191 A Friend of the Family: live text left out, its objectives need 279598 which ours lacks

-- 6. Items live vendors sell that ours lack (vendor exists here; items bought with other items/currencies are left out).
--    Limited stock: the most players saw, restock time 2 h like our other limited rows (ESTIMATED).
-- 1149 Uthok (1 visit(s) on live)
INSERT IGNORE INTO `npc_vendor` (`entry`,`slot`,`item`,`maxcount`,`incrtime`,`ExtendedCost`,`type`,`VerifiedBuild`) VALUES (1149,22,11291,0,0,0,1,70888) /* Star Wood */;
-- 3085 Gloria Femmel (1 visit(s) on live)
INSERT IGNORE INTO `npc_vendor` (`entry`,`slot`,`item`,`maxcount`,`incrtime`,`ExtendedCost`,`type`,`VerifiedBuild`) VALUES (3085,7,4470,0,0,0,1,70888) /* Simple Wood */,(3085,8,4471,0,0,0,1,70888) /* Flint and Tinder */,(3085,9,4536,0,0,0,1,70888) /* Shiny Red Apple */,(3085,10,10284,0,0,0,1,70888) /* Simple Flour */,(3085,11,11291,0,0,0,1,70888) /* Star Wood */;
-- 5483 Erika Tate (3 visit(s) on live)
INSERT IGNORE INTO `npc_vendor` (`entry`,`slot`,`item`,`maxcount`,`incrtime`,`ExtendedCost`,`type`,`VerifiedBuild`) VALUES (5483,7,4470,0,0,0,1,70888) /* Simple Wood */,(5483,8,4471,0,0,0,1,70888) /* Flint and Tinder */,(5483,9,4536,0,0,0,1,70888) /* Shiny Red Apple */,(5483,10,10284,0,0,0,1,70888) /* Simple Flour */,(5483,11,11291,0,0,0,1,70888) /* Star Wood */;
-- 12027 Tukk (1 visit(s) on live)
INSERT IGNORE INTO `npc_vendor` (`entry`,`slot`,`item`,`maxcount`,`incrtime`,`ExtendedCost`,`type`,`VerifiedBuild`) VALUES (12027,28,11291,0,0,0,1,70888) /* Star Wood */;
-- 12031 Mai'Lahii (1 visit(s) on live)
INSERT IGNORE INTO `npc_vendor` (`entry`,`slot`,`item`,`maxcount`,`incrtime`,`ExtendedCost`,`type`,`VerifiedBuild`) VALUES (12031,123,273636,0,0,0,1,70888) /* Chef's Knife */;
-- 12033 Wulan (1 visit(s) on live)
INSERT IGNORE INTO `npc_vendor` (`entry`,`slot`,`item`,`maxcount`,`incrtime`,`ExtendedCost`,`type`,`VerifiedBuild`) VALUES (12033,10,4470,0,0,0,1,70888) /* Simple Wood */,(12033,11,4471,0,0,0,1,70888) /* Flint and Tinder */,(12033,12,4536,0,0,0,1,70888) /* Shiny Red Apple */,(12033,13,10284,0,0,0,1,70888) /* Simple Flour */,(12033,14,11291,0,0,0,1,70888) /* Star Wood */;
-- 12245 Vendor-Tron 1000 (2 visit(s) on live)
INSERT IGNORE INTO `npc_vendor` (`entry`,`slot`,`item`,`maxcount`,`incrtime`,`ExtendedCost`,`type`,`VerifiedBuild`) VALUES (12245,37,5642,0,0,0,1,70888) /* Recipe: Free Action Potion */,(12245,38,5643,0,0,0,1,70888) /* Recipe: Great Rage Potion */,(12245,39,5973,0,0,0,1,70888) /* Pattern: Barbaric Leggings */,(12245,40,6068,0,0,0,1,70888) /* Recipe: Shadow Oil */,(12245,41,7451,0,0,0,1,70888) /* Pattern: Green Whelp Bracers */,(12245,42,7613,0,0,0,1,70888) /* Pattern: Green Leather Armor */;

-- 7. Trainer spells live teaches at a different level (apprentice professions at 1, druid Primal Bite).
UPDATE `trainer_spell` SET `ReqLevel`=1 WHERE `TrainerId`=1001383 AND `SpellId`=2018; -- Apprentice Blacksmith, was 5
UPDATE `trainer_spell` SET `ReqLevel`=1 WHERE `TrainerId`=1002834 AND `SpellId`=7620; -- Apprentice Fishing, was 0
UPDATE `trainer_spell` SET `ReqLevel`=10 WHERE `TrainerId`=1002834 AND `SpellId`=7731; -- Journeyman Fishing, was 0
UPDATE `trainer_spell` SET `ReqLevel`=1 WHERE `TrainerId`=1003007 AND `SpellId`=2108; -- Apprentice Leatherworking, was 5
UPDATE `trainer_spell` SET `ReqLevel`=1 WHERE `TrainerId`=1003355 AND `SpellId`=2018; -- Apprentice Blacksmith, was 5
