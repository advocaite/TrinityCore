-- vMaNGOS (patch 1.12 = 10) loot converted to TrinityCore columns, same convention as the baseline
DROP TABLE IF EXISTS tc_creature, tc_reference, tc_skinning, tc_pickpocket, tc_ct;
CREATE TABLE tc_ct AS
  SELECT t.entry, t.name, t.loot_id, t.skinning_loot_id, t.pickpocket_loot_id, t.gold_min, t.gold_max FROM creature_template t
  JOIN (SELECT entry, MAX(patch) p FROM creature_template WHERE patch <= 10 GROUP BY entry) m ON m.entry=t.entry AND m.p=t.patch;
ALTER TABLE tc_ct ADD PRIMARY KEY(entry);
CREATE TABLE tc_creature AS SELECT entry AS Entry, IF(mincountOrRef<0,1,0) AS ItemType, IF(mincountOrRef<0,-mincountOrRef,item) AS Item,
  ABS(ChanceOrQuestChance) AS Chance, IF(ChanceOrQuestChance<0 AND mincountOrRef>0,1,0) AS QuestRequired, 1 AS LootMode, groupid AS GroupId,
  IF(mincountOrRef<0,1,mincountOrRef) AS MinCount, maxcount AS MaxCount, condition_id
  FROM creature_loot_template WHERE patch_min<=10 AND patch_max>=10;
ALTER TABLE tc_creature ADD KEY(Entry);
CREATE TABLE tc_reference AS SELECT entry AS Entry, IF(mincountOrRef<0,1,0) AS ItemType, IF(mincountOrRef<0,-mincountOrRef,item) AS Item,
  ABS(ChanceOrQuestChance) AS Chance, IF(ChanceOrQuestChance<0 AND mincountOrRef>0,1,0) AS QuestRequired, 1 AS LootMode, groupid AS GroupId,
  IF(mincountOrRef<0,1,mincountOrRef) AS MinCount, maxcount AS MaxCount, condition_id
  FROM reference_loot_template WHERE patch_min<=10 AND patch_max>=10;
ALTER TABLE tc_reference ADD KEY(Entry);
CREATE TABLE tc_skinning AS SELECT entry AS Entry, IF(mincountOrRef<0,1,0) AS ItemType, IF(mincountOrRef<0,-mincountOrRef,item) AS Item,
  ABS(ChanceOrQuestChance) AS Chance, IF(ChanceOrQuestChance<0 AND mincountOrRef>0,1,0) AS QuestRequired, 1 AS LootMode, groupid AS GroupId,
  IF(mincountOrRef<0,1,mincountOrRef) AS MinCount, maxcount AS MaxCount, condition_id
  FROM skinning_loot_template WHERE patch_min<=10 AND patch_max>=10;
ALTER TABLE tc_skinning ADD KEY(Entry);
CREATE TABLE tc_pickpocket AS SELECT entry AS Entry, IF(mincountOrRef<0,1,0) AS ItemType, IF(mincountOrRef<0,-mincountOrRef,item) AS Item,
  ABS(ChanceOrQuestChance) AS Chance, IF(ChanceOrQuestChance<0 AND mincountOrRef>0,1,0) AS QuestRequired, 1 AS LootMode, groupid AS GroupId,
  IF(mincountOrRef<0,1,mincountOrRef) AS MinCount, maxcount AS MaxCount, condition_id
  FROM pickpocketing_loot_template WHERE patch_min<=10 AND patch_max>=10;
ALTER TABLE tc_pickpocket ADD KEY(Entry);
