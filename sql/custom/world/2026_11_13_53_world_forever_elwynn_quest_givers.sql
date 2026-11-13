-- WoW Forever: quests that nobody in our data gave or took back, although the NPC is there (live gives and ends them at these
-- NPCs; each checked against our quest titles). Safe to run more than once.
--   1646 The Tome of Divinity: Tiza Battleforge (6179), Ironforge (dwarf paladins). Ours only had the hand-in.
--   91752 The Big Picture: Kelsey Fargo (247226) -> Marshal McBride (197).   91758 Follow That Kobold!: McBride -> Tordrin
--   Sternblade (2026_11_13_52).   91772 Shhh! We're Hunting Kobolds: Tordrin -> Marshal Dughan (240).
--   92479 A Scribbled Letter (human hunters): McBride -> Tordrin.   96627 The Adventurer: McBride -> Sam Sarsaparilla.
--   96626 Camping 101: Cooking: Sam -> Tomas (1430).   97924 Camping 101: Skinning: Sam -> Helene Peltskinner (6306).
--   99127 A Net Disaster, 99128 Slimy Menace: Jason Mathers (383).   99143 Bottles and Baubles: Lee Brown (1651), Goldshire.
DELETE FROM `creature_queststarter` WHERE (`id`, `quest`) IN ((6179,1646),(247226,91752),(197,91758),(197,92479),(197,96627),(383,99127),(383,99128),(1651,99143));
INSERT INTO `creature_queststarter` (`id`, `quest`, `VerifiedBuild`) VALUES
(6179, 1646, 0), (247226, 91752, 0), (197, 91758, 0), (197, 92479, 0), (197, 96627, 0), (383, 99127, 0), (383, 99128, 0), (1651, 99143, 0);
DELETE FROM `creature_questender` WHERE (`id`, `quest`) IN ((197,91752),(240,91772),(1430,96626),(6306,97924),(383,99127),(383,99128),(1651,99143));
INSERT INTO `creature_questender` (`id`, `quest`, `VerifiedBuild`) VALUES
(197, 91752, 0), (240, 91772, 0), (1430, 96626, 0), (6306, 97924, 0), (383, 99127, 0), (383, 99128, 0), (1651, 99143, 0);
