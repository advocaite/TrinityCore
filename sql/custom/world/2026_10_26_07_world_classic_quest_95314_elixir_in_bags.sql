-- Quest 95314 "That Shadowvale Green Elixir": looted Bottles of Whispering Elixir (268801) count but never reach the bags,
-- because objective 470252 is quest-bound (Flags2 & 1, retail sniff data); make it a normal bag item objective.
-- Rollback: UPDATE quest_objectives SET Flags2=Flags2|1 WHERE ID=470252;
UPDATE `quest_objectives` SET `Flags2`=`Flags2`&~1 WHERE `ID`=470252;
