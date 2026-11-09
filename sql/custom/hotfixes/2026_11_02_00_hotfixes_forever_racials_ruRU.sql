-- ruRU texts for the Forever racial changes of 2026_10_27_05 (Spell.db2 descriptions).
-- That file sent its new Spell.db2 texts in enUS only. A Spell record hotfix without a blob in the client's locale goes out as
-- not valid, so a ruRU client kept its own old text. That text still points at the old 15 min Energized 1259691, whose
-- effects are now 0 (it became Attuned, a marker aura), so Read Leyline showed a gain of 0% (Azeldof, 2026-11-02).
-- Buff and ability names go through $@spellname so they always match what the client shows.
-- The Spell rows get new unique ids so a client that cached the old answer asks again. Safe to run again.
DELETE FROM `hotfix_blob` WHERE `TableHash` = 3776013982 AND `locale` = 'ruRU' AND `RecordId` IN (1259719,20550,20574,1259918,1259705,1259686,1259691,1270893);
INSERT INTO `hotfix_blob` (`TableHash`,`RecordId`,`locale`,`Blob`,`VerifiedBuild`) VALUES
(3776013982,1259719,'ruRU',CONCAT('Расовая пассивная способность',CHAR(0),'Повышает вероятность попадания заклинаниями и атаками на $s1%.',CHAR(0),'',CHAR(0)),70630),
(3776013982,20550,'ruRU',CONCAT('Расовая пассивная способность',CHAR(0),'Увеличивает общий запас здоровья на $s1%.',CHAR(0),'',CHAR(0)),70630),
(3776013982,20574,'ruRU',CONCAT('Расовая пассивная способность',CHAR(0),'Урон, наносимый питомцами охотников и чернокнижников, увеличен на $s1%.',CHAR(0),'',CHAR(0)),70630),
(3776013982,1259918,'ruRU',CONCAT('Расовая пассивная способность',CHAR(0),'Каждые $s2 сек. в движении скорость передвижения увеличивается на $s1%, но не более чем на $s3%. Получение урона или остановка снижают этот эффект.',CHAR(0),'',CHAR(0)),70630),
(3776013982,1259705,'ruRU',CONCAT('Расовая способность',CHAR(0),'Вы пытаетесь получить силу ближайшей лей-линии: скорость восстановления здоровья и маны увеличивается на $1270842s1% на $1270842d. При использовании на лей-линии вы также получаете эффект «$@spellname1259691» на $1259691d, который повышает бонус до 100%. Нельзя использовать, пока действует «Озарение».',CHAR(0),'',CHAR(0)),70630),
(3776013982,1259686,'ruRU',CONCAT('Расовая способность',CHAR(0),'Вы пытаетесь получить благословение средоточия стихий: скорость передвижения, в том числе верхом, увеличивается на $1259688s1% на $1259688d. При использовании в средоточии стихий вы также получаете эффект «$@spellname1270893» на $1270893d, который повышает бонус до 30%.',CHAR(0),'',CHAR(0)),70630),
(3776013982,1259691,'ruRU',CONCAT('',CHAR(0),'',CHAR(0),'«$@spellname1259705» увеличивает скорость восстановления здоровья и маны на 100%. Сохраняется после смерти.',CHAR(0)),70630),
(3776013982,1270893,'ruRU',CONCAT('',CHAR(0),'',CHAR(0),'«$@spellname1259686» увеличивает скорость передвижения на 30%. Сохраняется после смерти.',CHAR(0)),70630);

UPDATE `hotfix_data` SET `UniqueId` = `Id` + 1397800000 - 2000000 WHERE `Id` BETWEEN 2000329 AND 2000336 AND `TableHash` = 3776013982;
