-- Classic 1.60 (WoW Forever): Forever quests that came in with the official beta sniff (quest text, objectives, levels,
-- loot) but no giver or turn-in, so nobody offers them (Discord: Cook Torka has no "This Fruit Could Bite Back").
-- Only quests a player can finish with the data we have. Giver/turn-in rows are taken from the quest text; rows the
-- text does not name directly are marked INFERRED. Safe to run again.

DELETE FROM `creature_queststarter` WHERE `quest` IN (96825,97279,97282,97921,98575,98576);
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES
(3191,96825,0),   -- Cook Torka: quest text "Cook Torka in Razor Hill wants you to collect 8 Prickly Pear Fruit"
(3143,97279,0),   -- INFERRED Gornek: "our weaponsmith Kzan behind me in the Den", Gornek is the quest giver in the Den
(3293,97282,0),   -- Rezlak: next quest of A Simmering Storm (97281), which is turned in to Rezlak
(263399,97921,0), -- INFERRED Sam Sarsaparilla: gives the other Elwynn Camping 101 quests (97923 Mining, 97925 Tailoring)
(3143,98575,0),   -- INFERRED Gornek: gives the other Valley of Trials class letters (3082-3090)
(3143,98576,0);   -- INFERRED Gornek: same

DELETE FROM `creature_questender` WHERE `quest` IN (96825,96876,97279,97281,97282,97921,98575,98576);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES
(3191,96825,0),   -- Cook Torka
(6786,96876,0),   -- Ukor: "Return the pack to Ukor in southern Durotar"
(3159,97279,0),   -- Kzan Thornslash: "return to Kzan Thornslash in the Den"
(3293,97281,0),   -- Rezlak: "Bring the Dull Stormy Orb to Rezlak"
(3293,97282,0),   -- Rezlak: "Bring Charged Thunder Lizard Organs to Rezlak"
(1218,97921,0),   -- Herbalist Pomeroy: "Speak with Herbalist Pomeroy in Elwynn Forest"
(3156,98575,0),   -- Nartok: "speak to Nartok inside the Den"
(5884,98576,0);   -- Mai'ah: "speak with Mai'ah near the entrance to the Den"

-- Kzan and Pomeroy had no quest giver flag
UPDATE `creature_template` SET `npcflag` = `npcflag` | 2 WHERE `entry` IN (3159,1218);
UPDATE `creature` SET `npcflag` = `npcflag` | 2 WHERE `id` IN (3159,1218) AND `npcflag` IS NOT NULL;

-- Class letters: Tablet = Troll, Parchment = Orc (race masks from the sniff), class from the letter's trainer, after
-- Cutting Teeth (788) like 3082-3090. 98575 is the Forever Troll warlock letter; 3090 Tainted Parchment also takes
-- Trolls now (2026_10_25_70), so the two share an exclusive group and a Troll warlock gets only one.
-- 97921 mirrors 97923 Camping 101: Mining (after 95998, skill 1).
DELETE FROM `quest_template_addon` WHERE `ID` IN (97921,98575,98576);
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`SourceSpellID`,`PrevQuestID`,`NextQuestID`,`ExclusiveGroup`,`BreadcrumbForQuestId`,`RewardMailTemplateID`,`RewardMailDelay`,`RequiredSkillID`,`RequiredSkillPoints`,`RequiredMinRepFaction`,`RequiredMaxRepFaction`,`RequiredMinRepValue`,`RequiredMaxRepValue`,`ProvidedItemCount`,`SpecialFlags`) VALUES
(97921,0,0,0,95998,0,0,0,0,0,182,1,0,0,0,0,0,0),
(98575,0,256,0,788,0,3090,0,0,0,0,0,0,0,0,0,1,0),
(98576,0,128,0,788,0,0,0,0,0,0,0,0,0,0,0,1,0);
UPDATE `quest_template_addon` SET `ExclusiveGroup` = 3090 WHERE `ID` = 3090;

-- Drops the quest text points to; no sniff loot exists for these items.
DELETE FROM `creature_loot_template` WHERE `Item` IN (277661,277665);
INSERT INTO `creature_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES
(3118,0,277661,8,0,1,0,1,1,'INFERRED 97281 start item Dull Stormy Orb: "the harpy was using it", Dustwind Storm Witch near Drygulch'),
(3130,0,277665,50,1,1,0,1,1,'INFERRED 97282: thunder lizards in the gorge west of Rezlak'),
(3131,0,277665,50,1,1,0,1,1,'INFERRED 97282: thunder lizards in the gorge west of Rezlak');
