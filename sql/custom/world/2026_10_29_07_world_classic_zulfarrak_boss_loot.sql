-- Classic 1.60: Zul'Farrak boss references carried items that do not exist in the Forever universe (Ukorz 9478/9479,
-- Zum'rah 18083, Antu'sul 9641 — Wowhead tooltip "Entity not found", never observed dropping); the single-use pointers
-- are inlined with each boss's real set so one item drops per kill.
-- rollback: re-insert the reference pointers (7267,1,30202),(7271,1,30203),(8127,1,30215) each with
--   Chance=100,QuestRequired=0,GroupId=0,MinCount=1,MaxCount=1,Comment='VMaNGOS 1.12', then DELETE the rows inserted below.
DELETE FROM `creature_loot_template` WHERE `Entry` IN (7267,7271,8127) AND `ItemType` = 1 AND `Item` IN (30202,30203,30215);
DELETE FROM `creature_loot_template` WHERE `Entry` IN (7267,7271,8127) AND `ItemType` = 0 AND `GroupId` = 1
  AND `Item` IN (9476,9477,11086,18082,9379,9639,9640);
INSERT INTO `creature_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES
(7267,0,9476,0,0,1,1,1,1,'Chief Ukorz Sandscalp - Big Bad Pauldrons'),
(7267,0,9477,0,0,1,1,1,1,'Chief Ukorz Sandscalp - The Chief\'s Enforcer'),
(7267,0,11086,0,0,1,1,1,1,'Chief Ukorz Sandscalp - Jang\'thraze the Protector'),
(7271,0,18082,0,0,1,1,1,1,'Witch Doctor Zum\'rah - Vexing Cane'),
(8127,0,9379,0,0,1,1,1,1,'Antu\'sul - Sang\'thraze the Deflector'),
(8127,0,9639,0,0,1,1,1,1,'Antu\'sul - The Hand of Antu\'sul'),
(8127,0,9640,0,0,1,1,1,1,'Antu\'sul - Vice Grips');