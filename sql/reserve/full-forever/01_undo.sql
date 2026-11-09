-- Undo of 01_hotfixes_live_spell_values.sql: drops the live rows, the 70685 client values come back. Safe to run again.
DELETE FROM `spell_effect` WHERE `ID` IN (690400,691884,691886,692212,692607,697417,698474,1323534,1323538);
DELETE FROM `spell_effect` WHERE `ID` IN (681806);
DELETE FROM `hotfix_data` WHERE `Id` BETWEEN 2001500 AND 2001559;
