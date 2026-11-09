-- Sword of the Fallen (286977, two-handed sword) came from the 70124 sniff with SheatheType 0, so it vanished when sheathed
-- (Eyelash, 2026-11-02). Every other two-handed sword in that sniff has 1 (on the back). Its hotfix row (1004830) already exists;
-- the server mixes the record's content into the hotfix unique id, so clients fetch the changed record. Safe to run again.
UPDATE `item` SET `SheatheType` = 1 WHERE `ID` = 286977;
