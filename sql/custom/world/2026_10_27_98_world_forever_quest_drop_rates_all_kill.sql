-- Quest drops whose chance was Wowhead Forever's all-kill count (drops / kills). Wowhead counts every kill, also by players who
-- cannot get the item (quest done or not taken; the server never shows them a quest item or a quest-starting item), so those
-- counts read far below the real chance. Same rule as 2026_10_27_97 (Fang of Githyiss):
--  * quest-starting item from a named or boss creature: every kill (only players who can take the quest see it);
--  * vanilla quest item: the vMaNGOS chance the item has on every other creature (80%, quest only);
--  * Forever quest objective: the rule of the other Forever quest drops, Wowhead x1.84 with a 48% floor, quest only.
-- Random quest starters from common creatures (An Old History Book, Primitive Drawing, Note Scrap, Bloodied Insignia, ...) are
-- left as they are: there is no per-player chance to set them by.

-- Quest starters, named / boss
UPDATE `creature_loot_template` SET `Chance`=100, `Comment`='Warchief Rend Blackhand: Head of Rend Blackhand, starts 84377, every kill' WHERE `Entry`=10429 AND `ItemType`=0 AND `Item`=227911;
UPDATE `creature_loot_template` SET `Chance`=100, `Comment`='Charlga Razorflank: Tattered Note, starts 79358, every kill' WHERE `Entry`=4421 AND `ItemType`=0 AND `Item`=212748;
UPDATE `creature_loot_template` SET `Chance`=100, `Comment`='The Baron: Abominable Head, starts 97288, every kill' WHERE `Entry`=250660 AND `ItemType`=0 AND `Item`=280438;
-- Forever quest objective from a named creature (its only source)
UPDATE `creature_loot_template` SET `Chance`=100, `QuestRequired`=1, `Comment`='Incinerator Gar''im: Summoner''s Salt (quest 78091), every kill' WHERE `Entry`=214519 AND `ItemType`=0 AND `Item`=209778;
-- Vanilla quest items, vMaNGOS chance
UPDATE `creature_loot_template` SET `Chance`=80, `QuestRequired`=1, `Comment`='Old Fire-Eye: Worgen Skull, vMaNGOS chance' WHERE `Entry`=259897 AND `ItemType`=0 AND `Item`=895;
UPDATE `creature_loot_template` SET `Chance`=80, `QuestRequired`=1, `Comment`='Old Fire-Eye: Worgen Fang, vMaNGOS chance' WHERE `Entry`=259897 AND `ItemType`=0 AND `Item`=896;
UPDATE `creature_loot_template` SET `Chance`=80, `Comment`='Centaur Ear, vMaNGOS chance' WHERE `Entry` IN (253272,255560,262838) AND `ItemType`=0 AND `Item`=6067;
UPDATE `creature_loot_template` SET `Chance`=80, `Comment`='Lord Tomas: Hillsbrad Human Skull, vMaNGOS chance' WHERE `Entry`=266216 AND `ItemType`=0 AND `Item`=3692;
-- Forever quest objectives, Wowhead x1.84 floor 48
UPDATE `creature_loot_template` SET `Chance`=56.65, `QuestRequired`=1, `Comment`='Cracked Sentry Core: Wowhead Forever 137/445 x1.84 (quest 96988)' WHERE `Entry`=246664 AND `ItemType`=0 AND `Item`=275997;
UPDATE `creature_loot_template` SET `Chance`=48, `QuestRequired`=1, `Comment`='Malignant Rotclaw Flesh: Wowhead Forever 1/9 x1.84 floor 48 (quest 98283)' WHERE `Entry`=271334 AND `ItemType`=0 AND `Item`=280331;
UPDATE `creature_loot_template` SET `Chance`=48, `Comment`='Thicket Raptor Meat: Wowhead Forever 4/21 x1.84 floor 48, as on the other raptors' WHERE `Entry`=271732 AND `ItemType`=0 AND `Item`=271100;
UPDATE `creature_loot_template` SET `Chance`=48, `Comment`='Thicket Raptor Hide: Wowhead Forever 42/254 x1.84 floor 48, as on the other raptors' WHERE `Entry`=260802 AND `ItemType`=0 AND `Item`=284845;
