-- Fire Nova Totem IV (7844) was left out of 2026_11_06_75: its esES, esMX, itIT, koKR, ptBR and zhTW names still had the zzOLD prefix. Safe to run again.
UPDATE `creature_template_locale` SET `Name` = TRIM(SUBSTRING(`Name`, 6)) WHERE `entry` = 7844 AND `Name` LIKE 'zzOLD%';
