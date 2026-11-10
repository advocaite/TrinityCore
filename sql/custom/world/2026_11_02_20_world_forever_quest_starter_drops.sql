-- Quest-starting drops that could never drop.
-- Each item below starts its quest when looted, but the loot row said
-- "drop only while the quest is active" (QuestRequired=1). Nobody has the
-- quest before the item, so the item never dropped and the quest never started.
-- Make them normal drops.
--   268548 Empty Powder Keg      -> 95213 Stolen Blasting Powder (Rockjaws)
--   255155                       -> 92910 Harvesting the Harvesters
--   247826                       -> 91740 Croaky's Head
--   247862                       -> 91746 Elmpaw's Head
--   268812                       -> 95328 Whispering Horror Residue
--   275722                       -> 96876 Ukor's Lost Pack
--   275723                       -> 96877 Halikor's Hoof
--   279275                       -> 98028 Baron Marinous
--   21749                        -> 8770 Target: Hive'Ashi Defenders
UPDATE `creature_loot_template` SET `QuestRequired`=0
WHERE `ItemType`=0 AND `QuestRequired`=1
  AND `Item` IN (268548,255155,247826,247862,268812,275722,275723,279275,21749);
