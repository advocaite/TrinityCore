-- Blackwing Lair: the lever by Chromaggus (179148) never opened his side portcullis (179116), so he stayed unattackable
-- (scenario bwl_chromaggus 2026-10-28: "creature 14020 is still not attackable 30 s after game object 179148 was used").
-- The lever now runs our own script classic_go_chromaggus_lever, which opens the door the Chromaggus script waits for.
-- The smart script on lever spawn 30234837 (and its timed list 940000009) is dropped so the door is opened in one place only.
-- Safe to run again.
UPDATE `gameobject_template` SET `AIName` = '', `ScriptName` = 'classic_go_chromaggus_lever' WHERE `entry` = 179148;
DELETE FROM `smart_scripts` WHERE `source_type` = 1 AND `entryorguid` = -30234837;
DELETE FROM `smart_scripts` WHERE `source_type` = 9 AND `entryorguid` = 940000009;
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId` = 22 AND `SourceId` = 1 AND `SourceEntry` = -30234837;
